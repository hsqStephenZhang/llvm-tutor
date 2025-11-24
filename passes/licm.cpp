#include "licm.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassPlugin.h"
#include <deque>
#include <llvm/ADT/SetVector.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/ConstantFolder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Local.h>
#include <map>

#include <llvm/IR/InstIterator.h>
#include <llvm/Passes/PassBuilder.h>

using namespace llvm;

// must be some trivial operation and for all operands,
// they must satisfy one of the following conditions:
// 1) is constant
// 2) is argument
// 3) is defined outside of the loop
bool isLoopInvariant(Loop *loop, Instruction *inst,
                     SmallPtrSetImpl<const BasicBlock *> &blocks_set) {
  // errs() << "Checking instruction: " << *inst << "\n";
  if (llvm::isa<BinaryOperator>(inst) || llvm::isa<UnaryOperator>(inst) ||
      llvm::isa<GetElementPtrInst>(inst) ||
      llvm::isa<CmpInst>(inst)) { // only consider simple operators

    for (auto &op : inst->operands()) {
      if (llvm::isa<Constant>(op) || llvm::isa<Argument>(op)) {
        continue;
      } else if (llvm::isa<Instruction>(op)) {
        Instruction *op_inst = llvm::cast<Instruction>(op);
        if (blocks_set.contains(op_inst->getParent())) {
          return false;
        }
      } else {
        return false;
      }
    }

    return true;
  }

  return false;
}

// check if inst's bb dominates all latches of the loop
// will cached the result in `dominates_all_latches_of_loop`
bool IsSaveToHoist(Loop *loop, Instruction *inst,
                   std::map<BasicBlock *, bool> &dominates_all_latches_of_loop,
                   SmallVector<BasicBlock *, 5> &latches, DominatorTree &DF) {

  // skip division and remainder operations since they might cause division
  // by zero exceptions
  if (inst->getOpcode() == Instruction::UDiv ||
      inst->getOpcode() == Instruction::SDiv ||
      inst->getOpcode() == Instruction::FDiv ||
      inst->getOpcode() == Instruction::URem ||
      inst->getOpcode() == Instruction::SRem ||
      inst->getOpcode() == Instruction::FRem) {
    return false;
  }

  auto BB = inst->getParent();
  if (auto val = dominates_all_latches_of_loop.find(BB);
      val != dominates_all_latches_of_loop.end()) {
    // return the cached result
    return val->second;
  }

  // BB must dominates all latches of the Loop
  auto dominates_all = true;
  for (auto latch : latches) {
    if (!DF.dominates(BB, latch)) {
      dominates_all = false;
      break;
    }
  }
  dominates_all_latches_of_loop[BB] = dominates_all;

  return dominates_all;
}

//------------------------------------------------------------------------------
// Loop InVariant Code Motion Pass
//------------------------------------------------------------------------------
PreservedAnalyses LICM::run(Function &Func,
                            llvm::FunctionAnalysisManager &FAM) {

  bool changed = false;
  // 1. detect all loops (by llvm or our analysis pass)
  // 2. traverse from inner to outer, for each one
  //    a) move instructions that is invariant and dominate all exits of the
  //    loop to pre-header b)

  DominatorTree DF(Func);

  LoopInfo &loopInfo = FAM.getResult<LoopAnalysis>(Func);
  for (auto loop : loopInfo) {
    std::map<BasicBlock *, bool> dominates_all_latches_of_loop;

    SmallVector<BasicBlock *, 5> latches;
    loop->getLoopLatches(latches);
    auto &blocks_set = loop->getBlocksSet();

    // we might move some instructions to pre-header, which could be further
    // used to move other instructions, so we need to rerun until no instruction
    // can be moved
    auto should_rerun = true;
    while (should_rerun) {
      should_rerun = false;

      auto preheader = loop->getLoopPreheader();
      if (!preheader) {
        continue;
      }

      std::deque<Instruction *> worklist;
      for (auto bb : loop->getBlocksVector()) {
        for (auto &inst : *bb) {
          worklist.push_back(&inst);
        }
      }

      for (auto inst : worklist) {
        if (isLoopInvariant(loop, inst, blocks_set) &&
            IsSaveToHoist(loop, inst, dominates_all_latches_of_loop, latches,
                          DF)) {
          // move the inst to the preheader
          // errs() << "Hoisting instruction: " << *inst
          //        << " to: " << preheader->getName() << "%"
          //        << preheader->getNumber() << "\n";
          inst->moveBefore(preheader->getTerminator());
          changed = true;
          should_rerun = true;
        }
      }
    }
  }

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
PassPluginLibraryInfo getLICMPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "my-licm", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "my-licm") {
                    FPM.addPass(LICM());
                    return true;
                  }

                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getLICMPluginInfo();
}
