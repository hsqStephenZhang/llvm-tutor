#include "loop_rotate.h"

#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/CodeGen/MachineBasicBlock.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/ConstantFolder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include <cassert>
#include <optional>
#include <tuple>
#include <utility>

using namespace llvm;

// the loop must satisfy:
//    1. has only one header
//    2. header has only two succ, body and exit
//    3. all latches unconditionally jmp to header
//    4. header has no side effects
// we will return some info needed for rotation,
// the body will be the new header, the old header will be the guard
std::optional<
    std::tuple<BasicBlock *, BasicBlock *, SmallVector<BasicBlock *, 5>>>
LoopCanRotate(Loop *loop) {
  auto header = loop->getHeader();

  for (auto &inst : *header) {
    if (inst.mayHaveSideEffects()) {
      return std::nullopt;
    }
  }

  SmallVector<BasicBlock *, 2> exitBlocks;
  loop->getExitBlocks(exitBlocks);
  if (exitBlocks.size() == 1 &&
      llvm::is_contained(successors(header), exitBlocks[0]) &&
      succ_size(header) == 2) {
    BasicBlock *guard = header;

    BasicBlock *newHeader;
    for (auto succ : successors(header)) {
      if (succ != exitBlocks[0]) {
        newHeader = succ;
      }
    }
    assert(newHeader);

    SmallVector<BasicBlock *, 5> latches;
    loop->getLoopLatches(latches);

    auto latchesAreAllUncond = true;

    for (auto latch : latches) {
      if (auto br = dyn_cast<BranchInst>(latch->getTerminator());
          !br || !br->isUnconditional()) {
        latchesAreAllUncond = false;
        break;
      }
    }

    if (latchesAreAllUncond) {
      return std::make_tuple(guard, newHeader, latches);
    }
  }

  return std::nullopt;
}

std::pair<Value *, llvm::BasicBlock *> getInitValueAndBB(PHINode &phi,
                                                         Loop *loop) {
  Value *init = nullptr;
  BasicBlock *init_bb = nullptr;
  for (int i = 0; i < phi.getNumIncomingValues(); i++) {
    BasicBlock *bb = phi.getIncomingBlock(i);
    Value *val = phi.getIncomingValue(i);
    if (!loop->contains(bb)) {
      init = val;
      init_bb = bb;
      break;
    }
  }
  assert(init);
  return std::make_pair(init, init_bb);
}

//------------------------------------------------------------------------------
// Loop Rotate Pass
//------------------------------------------------------------------------------
PreservedAnalyses LoopRotate::run(Function &Func,
                                  llvm::FunctionAnalysisManager &FAM) {

  bool changed = false;

  // rename the header to be "guard"
  // copied the header's instructions to all latches
  // fix the phi nodes in the guard since now it should only use init value
  // create new phi nodes in the

  LoopInfo &info = FAM.getResult<LoopAnalysis>(Func);
  for (auto loop : info) {
    auto header = loop->getHeader();

    if (auto pair = LoopCanRotate(loop); pair.has_value()) {
      auto [guard, newHeader, latches] = pair.value();
      guard->setName(guard->getName() + ".guard");

      ValueToValueMapTy VMap;

      // 1. add new phi nodes in newHeader
      // we might have to fix the IncomingBlock for the cloned phi node
      for (auto &phi : header->phis()) {
        auto inComingBB = getInitValueAndBB(phi, loop).second;
        ValueToValueMapTy tmp;
        tmp[inComingBB] = guard;
        auto newPhi = phi.clone();
        RemapInstruction(newPhi, tmp);
        newPhi->insertBefore(newHeader->begin());

        VMap[&phi] = newPhi;
      }

      // 2. copy instructions in header to the end of all latches
      // the new terminator for all latches is `br cond, newHeader, exit`
      for (auto latch : latches) {
        latch->getTerminator()->eraseFromParent();
      }
      for (auto &inst : *header) {
        if (llvm::isa<PHINode>(inst)) {
          continue;
        }

        for (auto latch : latches) {
          auto newInst = inst.clone();
          RemapInstruction(newInst, VMap);
          newInst->insertInto(latch, latch->end());
        }
      }

      // 3. fix phi nodes in guard
      for (auto &phi : llvm::make_early_inc_range(header->phis())) {
        Value *init = nullptr;
        for (int i = 0; i < phi.getNumIncomingValues(); i++) {
          BasicBlock *bb = phi.getIncomingBlock(i);
          Value *val = phi.getIncomingValue(i);
          if (!loop->contains(bb)) {
            init = val;
            break;
          }
        }
        assert(init);
        phi.replaceAllUsesWith(init);
        phi.eraseFromParent();
      }

      changed = true;
    }
  }

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
PassPluginLibraryInfo getLoopRotatePluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "my-looprotate", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "my-looprotate") {
                    FPM.addPass(LoopRotate());
                    return true;
                  }

                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getLoopRotatePluginInfo();
}
