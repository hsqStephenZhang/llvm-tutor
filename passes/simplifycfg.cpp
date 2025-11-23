#include "simplifycfg.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassPlugin.h"
#include <deque>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/ConstantFolder.h>
#include <llvm/Transforms/Utils/Local.h>

#include <llvm/IR/InstIterator.h>
#include <llvm/Passes/PassBuilder.h>

using namespace llvm;

//------------------------------------------------------------------------------
// SimplifyCFG Pass Implementation
// will do the following things:
// 1. Remove unreachable bbs
// 2. Merge A, B that A unconditionally br to B and B has only one pred A
// 3. Remove bbs that has no solid instructions but unconditional branch
// 4. Hoisting and Sinking for simple `if else` structure
// 5. Optional, turn phinode into select inst for simple `if else` structure
//------------------------------------------------------------------------------
PreservedAnalyses SimplifyCFG::run(Function &Func,
                                   llvm::FunctionAnalysisManager &FAM) {

  bool changed = false;

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
PassPluginLibraryInfo getSimplifyCFGPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "my-simplifycfg", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "my-simplifycfg") {
                    FPM.addPass(SimplifyCFG());
                    return true;
                  }

                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getSimplifyCFGPluginInfo();
}
