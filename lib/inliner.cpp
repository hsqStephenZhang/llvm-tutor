#include "inliner.h"
#include <deque>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Analysis/InlineAdvisor.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/ConstantFolder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/Local.h>

#include <llvm/IR/InstIterator.h>
#include <llvm/Passes/PassBuilder.h>
#include <vector>

using namespace llvm;

bool myShouldInline(CallInst *CI) {
  Function *Callee = CI->getCalledFunction();
  if (!Callee)
    return false; // Indirect call

  // Do not inline functions marked as noinline
  if (Callee->hasFnAttribute(Attribute::NoInline))
    return false;

  // Simple heuristic: inline functions with less than 5 instructions
  size_t InstCount = 0;
  for (const BasicBlock &BB : *Callee) {
    InstCount += BB.size();
    if (InstCount > 10)
      return false;
  }

  // If the Callee has another function call, do not inline
  for (const BasicBlock &BB : *Callee) {
    for (const Instruction &I : BB) {
      if (isa<CallInst>(I) || isa<InvokeInst>(I)) {
        return false;
      }
    }
  }

  return true;
}

BasicBlock *inlineFunction(CallInst *cs) {
  if (!myShouldInline(cs)) {
    return nullptr;
  }
  Function *callee = cs->getCalledFunction();

  BasicBlock *callblock = cs->getParent();

  BasicBlock *post = callblock->splitBasicBlock(cs->getIterator(), "post-call");
  BasicBlock *pre = callblock;
  Function *caller = pre->getParent();

  ValueToValueMapTy VMap;
  for (unsigned i = 0; i < cs->arg_size(); ++i) {
    Value *Arg = cs->getArgOperand(i);
    Argument *Param = callee->getArg(i);
    VMap[Param] = Arg;
  }

  BasicBlock *inlinedEntry = nullptr;
  std::vector<BasicBlock *> calleeBlocks;
  for (BasicBlock &bb : *callee) {
    BasicBlock *newbb = CloneBasicBlock(&bb, VMap, ".inlined", caller);
    if (&bb == &callee->getEntryBlock()) {
      inlinedEntry = newbb;
    }
    calleeBlocks.push_back(newbb);
    VMap[&bb] = newbb;

    for (Instruction &I : *newbb) {
      VMap[&I] = &I;
    }
  }

  // fix instructions + terminators + phi nodes automatically
  for (BasicBlock *newbb : calleeBlocks) {
    for (Instruction &I : *newbb) {
      RemapInstruction(&I, VMap,
                       RF_NoModuleLevelChanges | RF_IgnoreMissingLocals);
    }
  }

  // now all blocks are cloned, we need to repair instructions

  // 1. fix pre's terminator to jump to inlinedEntry
  pre->getTerminator()->setSuccessor(0, inlinedEntry);

  // find all return instructions in the cloned blocks
  // if num > 1, we need to create a phi node in the post block to merge them
  std::vector<Instruction *> retInstrs;
  for (BasicBlock *bb : calleeBlocks) {
    for (Instruction &inst : *bb) {
      if (ReturnInst *ret = dyn_cast<ReturnInst>(&inst)) {
        retInstrs.push_back(ret);
      }
    }
  }
  errs() << "Found " << retInstrs.size() << " return instructions\n";

  llvm::LLVMContext &Context = caller->getContext();
  llvm::IRBuilder<> Builder(Context);

  if (retInstrs.size() == 0) {
    cs->eraseFromParent();
  } else if (retInstrs.size() == 1) {
    Instruction *callee_ret = retInstrs[0];
    Value *retVal = callee_ret->getOperand(0);
    cs->replaceAllUsesWith(retVal);

    Builder.SetInsertPoint(callee_ret);
    Builder.CreateBr(post);
    callee_ret->eraseFromParent();
    cs->eraseFromParent();
  } else {
    Builder.SetInsertPoint(post->begin());
    auto phi = Builder.CreatePHI(cs->getType(), retInstrs.size(), "retphi");
    for (auto retInst : retInstrs) {
      // join
      phi->addIncoming(retInst->getOperand(0), retInst->getParent());
      // fork
      Builder.SetInsertPoint(retInst);
      Builder.CreateBr(post);
      retInst->eraseFromParent();
    }

    cs->replaceAllUsesWith(phi);
    cs->eraseFromParent();
  }

  return post;
}

//------------------------------------------------------------------------------
// Inline Pass Implementation
//------------------------------------------------------------------------------
PreservedAnalyses Inliner::run(Function &Func,
                               llvm::FunctionAnalysisManager &FAM) {
  errs() << "Inliner pass running on function: " << Func.getName() << "\n";
  bool changed = false;

  std::deque<BasicBlock *> worklist;
  for (auto &BB : Func) {
    worklist.push_back(&BB);
  }

  while (!worklist.empty()) {
    BasicBlock *BB = worklist.front();
    worklist.pop_front();

    for (auto &I : *BB) {
      if (auto *CB = dyn_cast<CallInst>(&I)) {
        if (auto post = inlineFunction(CB); post != nullptr) {
          // handle the reset of the original basic block
          worklist.push_back(post);
          changed = true;
          // break to avoid iterator invalidation
          break;
        }
      }
    }
  }

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
PassPluginLibraryInfo getInlinerPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "my-inliner", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "my-inliner") {
                    FPM.addPass(Inliner());
                    return true;
                  }

                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getInlinerPluginInfo();
}
