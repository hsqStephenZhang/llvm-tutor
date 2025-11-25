#include "gvn.h"

#include <algorithm>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/ConstantFolding.h>
#include <llvm/Analysis/InstructionSimplify.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/ConstantFolder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Value.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Local.h>

#include <cassert>
#include <deque>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>

using namespace llvm;

class Expression {
  int Opcode;
  Type *ty;
  SmallVector<int, 4> operandClassIds;
  // for phi nodes
  SmallVector<BasicBlock *, 4> inComingBBs;

public:
  Expression(int Opcode, Type *ty, SmallVector<int, 4> operandClassIds)
      : Opcode(Opcode), ty(ty), operandClassIds(operandClassIds) {}

  Expression(int Opcode, Type *ty, SmallVector<int, 4> operandClassIds,
             SmallVector<BasicBlock *, 4> inComingBBs)
      : Opcode(Opcode), ty(ty), operandClassIds(operandClassIds),
        inComingBBs(inComingBBs) {}

  bool operator==(const Expression &Other) const {
    return Opcode == Other.Opcode && ty == Other.ty &&
           operandClassIds == Other.operandClassIds;
  }

  friend llvm::hash_code hash_value(const Expression &E) {
    return llvm::hash_combine(
        E.Opcode, E.ty,
        llvm::hash_combine_range(E.operandClassIds.begin(),
                                 E.operandClassIds.end()),
        llvm::hash_combine_range(E.inComingBBs.begin(), E.inComingBBs.end()));
  }
};

namespace std {
template <> struct hash<Expression> {
  size_t operator()(const Expression &E) const { return (size_t)hash_value(E); }
};
} // namespace std

class CongruenceClass {
public:
  int id;
  Instruction *leader;
  std::set<Instruction *> members;

  CongruenceClass(Instruction *leader, int id) : id(id), leader(leader) {
    if (leader) {
      members.insert(leader);
    }
  }

  // add a new inst to members
  void add(Instruction *inst, DominatorTree &DT);
  // remove an inst from members
  void remove(Instruction *inst, DominatorTree &DT);
};

void CongruenceClass::add(Instruction *inst, DominatorTree &DT) {
  assert(leader);
  members.insert(inst);
  if (DT.dominates(inst, leader)) {
    leader = inst;
  }
}

void CongruenceClass::remove(Instruction *inst, DominatorTree &DT) {
  members.erase(inst);

  if (inst != leader)
    return;

  leader = nullptr;

  if (members.empty())
    return;

  Instruction *candidate = *members.begin();

  // we could have members in two sibling branches
  // neither dominates another
  // in that case, we will just pick one
  // at the end, when we replace the inst with leader
  // we will check if the leader dominates the replaced one
  // this makes sure that the GVN won't have any affect in this situation
  for (Instruction *I : members) {
    bool dominatesAll = true;
    for (Instruction *J : members) {
      if (I != J && !DT.dominates(I, J)) {
        dominatesAll = false;
        break;
      }
    }
    if (dominatesAll) {
      candidate = I;
      break;
    }
  }

  leader = candidate;
}

using CanonicalResult = std::variant<Expression, CongruenceClass *>;

// return nullopt if we don't handle this kind of instructions
std::optional<CanonicalResult> calculateCanonicalExpr(
    Instruction *inst,
    std::unordered_map<Value *, CongruenceClass *> &classes) {
  // TODO: handle simplification and reassociation here
  if (isa<BinaryOperator>(inst) || isa<CmpInst>(inst)) {
    auto opcode = inst->getOpcode();
    auto id1 = classes[inst->getOperand(0)]->id;
    auto id2 = classes[inst->getOperand(1)]->id;
    if (inst->isCommutative() && id1 > id2) {
      std::swap(id1, id2);
    }
    SmallVector<int, 4> ids = {id1, id2};
    return Expression(opcode, inst->getType(), ids);
  } else if (auto unary = dyn_cast<UnaryOperator>(inst); unary) {
    auto id1 = classes[unary->getOperand(0)]->id;
    SmallVector<int, 4> ids = {id1};
    return Expression(unary->getOpcode(), unary->getType(), ids);
  } else if (auto phi = dyn_cast<PHINode>(inst); phi) {
    SmallVector<int, 4> ids;
    SmallVector<BasicBlock *, 4> bbs;

    for (int i = 0; i < phi->getNumIncomingValues(); i++) {
      auto bb = phi->getIncomingBlock(i);
      bbs.push_back(bb);
      auto value = phi->getIncomingValue(i);
      if (auto cls = classes[value]; cls) {
        ids.push_back(cls->id);
      }
    }
    bool all_same = std::all_of(ids.begin(), ids.end(),
                                [&](int x) { return x == ids.front(); });
    if (all_same) {
      return classes[phi->getIncomingValue(0)];
    } else {
      return Expression(phi->getOpcode(), phi->getType(), ids, bbs);
    }
  }
  return std::nullopt;
}

//------------------------------------------------------------------------------
// New GVN Pass based on congruence classes(closure)
//------------------------------------------------------------------------------
PreservedAnalyses GVN::run(Function &Func, llvm::FunctionAnalysisManager &FAM) {

  bool changed = false;

  DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(Func);

  // if we think of Expression and Instruction as a tree
  // Instruction is the leaf, Expression is the non-leaf node
  llvm::SpecificBumpPtrAllocator<CongruenceClass> allocator;
  std::unordered_map<Value *, CongruenceClass *> valToClass;
  std::unordered_map<Expression, CongruenceClass *> exprToClass;
  int clsId = 0;

  for (auto &arg : Func.args()) {
    void *mem = allocator.Allocate();
    // the leader does not matter for class that has only one
    auto cls = new (mem) CongruenceClass(nullptr, clsId++);
    valToClass[&arg] = cls;
  }

  std::deque<Instruction *> worklist;
  for (auto &inst : instructions(Func)) {
    void *mem = allocator.Allocate();
    auto cls = new (mem) CongruenceClass(&inst, clsId++);
    valToClass[&inst] = cls;
    worklist.push_back(&inst);

    for (int i = 0; i < inst.getNumOperands(); i++) {
      if (auto op = inst.getOperand(i);
          isa<Constant>(op) && valToClass[op] == nullptr) {
        void *mem = allocator.Allocate();
        // the leader does not matter for class that has only one
        auto cls = new (mem) CongruenceClass(nullptr, clsId++);
        valToClass[op] = cls;
      }
    }
  }

  while (!worklist.empty()) {
    Instruction *inst = worklist.front();
    worklist.pop_front();

    // get the canonical expr of inst
    auto res0 = calculateCanonicalExpr(inst, valToClass);
    if (!res0.has_value()) {
      continue;
    }
    auto res = res0.value();

    CongruenceClass *newCls = nullptr;
    if (auto equalCls = std::get_if<CongruenceClass *>(&res)) {
      newCls = *equalCls;
    } else if (auto expr = std::get_if<Expression>(&res)) {
      if (auto it = exprToClass.find(*expr); it != exprToClass.end()) {
        newCls = it->second;
      } else {
        void *mem = allocator.Allocate();
        auto cls = new (mem) CongruenceClass(inst, clsId++);
        exprToClass[*expr] = cls;
        newCls = cls;
      }
    }

    auto oldCls = valToClass[inst];

    if (oldCls->id != newCls->id) {
      oldCls->remove(inst, DT);
      newCls->add(inst, DT);
      valToClass[inst] = newCls;
      changed = true;

      for (auto user : inst->users()) {
        if (auto i = dyn_cast<Instruction>(user)) {
          worklist.push_back(i);
        }
      }
    }
  }

  if (changed) {
    for (auto &inst : llvm::make_early_inc_range(instructions(Func))) {
      auto cls = valToClass[&inst];
      if (cls->leader && cls->leader != &inst &&
          DT.dominates(cls->leader, &inst)) {
        inst.replaceAllUsesWith(cls->leader);
        inst.eraseFromParent();
      }
    }
  }

  allocator.DestroyAll();

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
PassPluginLibraryInfo getGVNPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "my-gvn", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "my-gvn") {
                    FPM.addPass(GVN());
                    return true;
                  }

                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getGVNPluginInfo();
}
