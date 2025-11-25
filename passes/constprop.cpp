//=============================================================================
// FILE:
//    ConvertFCmpEq.cpp
//
// DESCRIPTION:
//    Transformation pass which uses the results of the FindFCmpEq analysis pass
//    to convert all equality-based floating point comparison instructions in a
//    function to indirect, difference-based comparisons.
//
//    This example demonstrates how to couple an analysis pass with a
//    transformation pass, the use of statistics (the STATISTIC macro), and LLVM
//    debugging operations (the LLVM_DEBUG macro and the llvm::dbgs() output
//    stream). It also demonstrates how instructions can be modified without
//    having to completely replace them.
//
//    Originally developed for [1].
//
//    [1] "Writing an LLVM Optimization" by Jonathan Smith
//
// USAGE:
//      opt --load-pass-plugin libConvertFCmpEq.dylib [--stats] `\`
//        --passes='convert-fcmp-eq' --disable-output <input-llvm-file>
//
// License: MIT
//=============================================================================
#include "constprop.h"

#include <llvm/Analysis/ConstantFolding.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>

#include <deque>

using namespace llvm;

//------------------------------------------------------------------------------
// Sparse Conditional Constant Propagation Pass Implementation
//------------------------------------------------------------------------------
PreservedAnalyses ConstPropPass::run(Function &F,
                                     llvm::FunctionAnalysisManager &FAM) {

  errs() << "Running my-constprop on function: " << F.getName() << "\n";
  bool changed = false;

  std::deque<Instruction *> worklist;
  for (Instruction &I : instructions(F)) {
    worklist.push_back(&I);
  }

  while (!worklist.empty()) {
    Instruction *I = worklist.back();
    worklist.pop_back();

    if (Constant *C =
            ConstantFoldInstruction(I, F.getParent()->getDataLayout())) {
      std::vector<Instruction *> users;
      for (auto *U : I->users()) {
        if (Instruction *UI = dyn_cast<Instruction>(U))
          worklist.push_back(UI);
      }

      I->replaceAllUsesWith(C);
      I->eraseFromParent();
      changed = true;
    }
  }

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
PassPluginLibraryInfo getConstPropPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "my-constprop", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "my-constprop") {
                    FPM.addPass(ConstPropPass());
                    return true;
                  }

                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getConstPropPluginInfo();
}
