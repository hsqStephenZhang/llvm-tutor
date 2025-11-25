#include "DCE.h"

#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/ConstantFolder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Transforms/Utils/Local.h>

#include <deque>

using namespace llvm;

bool removeUnreachableBBs(Function &Func) {
  bool changed = false;

  // 1. remove unreachable bbs

  std::set<BasicBlock *> reachable;
  std::deque<BasicBlock *> worklist = {&Func.getEntryBlock()};

  while (!worklist.empty()) {
    BasicBlock *cur = worklist.back();
    worklist.pop_back();
    if (!reachable.insert(cur).second) {
      continue;
    }

    for (auto succ : successors(cur)) {
      worklist.push_back(succ);
    }
  }
  for (auto &bb : llvm::make_early_inc_range(Func)) {
    if (reachable.find(&bb) == reachable.end()) {
      bb.removeFromParent();
      changed = true;
    }
  }
  return changed;
}

//------------------------------------------------------------------------------
// Simple Dead Code Elimination Pass
//------------------------------------------------------------------------------
PreservedAnalyses DCE::run(Function &Func, llvm::FunctionAnalysisManager &FAM) {

  bool changed = false;

  changed |= removeUnreachableBBs(Func);

  std::deque<Instruction *> worklist;
  // we heavily relies on isInstructionTriviallyDead utility
  for (Instruction &I : instructions(Func)) {
    if (I.use_empty() && isInstructionTriviallyDead(&I)) {
      worklist.push_back(&I);
    }
  }

  while (!worklist.empty()) {
    Instruction *I = worklist.back();
    worklist.pop_back();

    if (I->use_empty() && isInstructionTriviallyDead(I)) {
      // Add its operands to the worklist before deleting it.
      for (auto &Op : I->operands()) {
        if (Instruction *OpI = dyn_cast<Instruction>(Op)) {
          worklist.push_back(OpI);
        }
      }
      I->eraseFromParent();
      changed = true;
    }
  }

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
PassPluginLibraryInfo getDCEPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "my-dce", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "my-dce") {
                    FPM.addPass(DCE());
                    return true;
                  }

                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDCEPluginInfo();
}
