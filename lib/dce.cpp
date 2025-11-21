#include "DCE.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassPlugin.h"
#include <deque>
#include <llvm/IR/Constant.h>
#include <llvm/IR/ConstantFolder.h>
#include <llvm/Transforms/Utils/Local.h>

#include <llvm/IR/InstIterator.h>
#include <llvm/Passes/PassBuilder.h>

using namespace llvm;

//------------------------------------------------------------------------------
// Simple Dead Code Elimination Pass
//------------------------------------------------------------------------------
PreservedAnalyses DCE::run(Function &Func, llvm::FunctionAnalysisManager &FAM) {

  bool changed = false;

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
PassPluginLibraryInfo getConvertFCmpEqPluginInfo() {
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
  return getConvertFCmpEqPluginInfo();
}
