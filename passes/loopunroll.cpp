#include "loopunroll.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/ConstantFolder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Transforms/Utils/LoopUtils.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <string>

using namespace llvm;

// trip count should be constant
// the indunction variable's update method should be linear(add, sub)
// the latch should be exactly one
bool shouldUnrollLoop(Loop *loop) { return true; }

//------------------------------------------------------------------------------
// Simple Loop Unroll Pass
//------------------------------------------------------------------------------
PreservedAnalyses LoopUnroll::run(Function &Func,
                                  llvm::FunctionAnalysisManager &FAM) {

  bool changed = false;

  auto factor = 4;

  ScalarEvolution &scalar_evo = FAM.getResult<ScalarEvolutionAnalysis>(Func);
  LoopInfo &info = FAM.getResult<LoopAnalysis>(Func);

  llvm::LLVMContext &Context = Func.getContext();
  llvm::IRBuilder<> Builder(Context);

  for (auto loop : info) {
    if (!shouldUnrollLoop(loop)) {
      continue;
    }

    Value *invar = nullptr;
    BasicBlock *header = loop->getHeader();
    BasicBlock *curLatch = loop->getLoopLatch();

    auto trip = scalar_evo.getSmallConstantTripCount(loop);
    errs() << "trip: " << trip << "\n";

    auto tripCnt = trip - 1;

    // clone the entire loop as the epilogue
    // link the orignal loop header(latch) with the epilogue header

    // for (auto &phi : header->phis()) {
    //   auto newPhi =
    //       Builder.CreatePHI(phi.getType(), phi.getNumIncomingValues(), "e");
    //   // should turn phi [initv, preheader] [inc, for.inc] <- let's call this
    //   // phi instruction `i`
    //   //        into phi [i, oldheader, inc.cloned, for.inc.cloned ]
    //   VMap[&phi] = newPhi;
    // }

    ValueToValueMapTy VMap;

    auto epilogue =
        cloneLoop(loop, loop->getParentLoop(), VMap, &info, nullptr);

    for (auto bb : epilogue->getBlocks()) {
      for (auto &inst : *bb) {
        RemapInstruction(&inst, VMap);
      }
    }

    //   // clone tripCnt-1 loop body inside the loop
    //   auto &body = loop->getBlocksSet();
    //   body.erase(loop->getLoopLatch());
    //   body.erase(loop->getHeader());

    //   SmallVector<Value *, 5> bodyArgs;

    //   // // we insert calculation of i+1, i + 2, ... i + tripCnt - 1 at the
    //   end of
    //   // // header
    //   // Type *ty = Type::getInt32Ty(Context);
    //   // auto c1 = ConstantInt::get(ty, 1);
    //   // for (int i = 1; i < tripCnt; i++) {
    //   //   Builder.SetInsertPoint(header->getTerminator());
    //   //   auto bodyArg = Builder.CreateAdd(invar, c1);
    //   //   bodyArgs.push_back(bodyArg);
    //   // }

    //   for (int i = 1; i < tripCnt; i++) {
    //     ValueToValueMapTy VMap;
    //     VMap[invar] = bodyArgs[i - 1]; // replace usage of i with i + cnt - 1
    //     for (auto bb : body) {
    //       std::string nameSuffix = &"unroll."[i];
    //       auto newBB = CloneBasicBlock(bb, VMap, nameSuffix, &Func);
    //     }
    //   }
    // }
  }

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
PassPluginLibraryInfo getLoopUnrollPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "my-loopunroll", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "my-loopunroll") {
                    FPM.addPass(LoopUnroll());
                    return true;
                  }

                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getLoopUnrollPluginInfo();
}