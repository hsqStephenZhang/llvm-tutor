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
#include "SCCP.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassPlugin.h"
#include <llvm/IR/Constant.h>
#include <llvm/IR/ConstantFolder.h>

#include <llvm/Passes/PassBuilder.h>

using namespace llvm;

//------------------------------------------------------------------------------
// Sparse Conditional Constant Propagation Pass Implementation
//------------------------------------------------------------------------------
PreservedAnalyses SCCP::run(Function &Func,
                            llvm::FunctionAnalysisManager &FAM) {

  return PreservedAnalyses::all();
}

bool SCCP::run(Function &Func) {
  // ConstantFoldInstruction()
  return true;
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
PassPluginLibraryInfo getSCCPPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "my-sccp", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "my-sccp") {
                    FPM.addPass(SCCP());
                    return true;
                  }

                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getSCCPPluginInfo();
}
