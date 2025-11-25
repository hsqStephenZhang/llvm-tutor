#include "simplifycfg.h"

#include <llvm/ADT/ADL.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringRef.h>
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
#include <llvm/Transforms/Utils/Local.h>

#include <deque>

using namespace llvm;

bool isEmptyBlock(BasicBlock &BB) {
  if (!BB.phis().empty()) {
    return false;
  }
  BranchInst *br = dyn_cast<BranchInst>(BB.getFirstNonPHIIt());

  // the block must be empty, and has unconditional jump
  if (!(br && br == BB.getTerminator() && br->isUnconditional())) {
    return false;
  }

  return true;
}

// Remove bbs that is empty and performs unconditional branch
bool removeEmptyUnconditionBrBlocks(Function &Func) {
  bool changed = false, should_rerun = true;

  // iterate till it does not change any more(might we use worklist instead?)
  while (should_rerun) {
    should_rerun = false;

    // will turn A->B->C into A->C, where B is our target and C is the succ
    // but should fix the phi node in C that has incoming from B
    for (BasicBlock &BB : llvm::make_early_inc_range(Func)) {
      if (BB.isEntryBlock()) {
        continue;
      }
      if (!isEmptyBlock(BB)) {
        continue;
      }

      auto succ = BB.getSingleSuccessor();

      // avoid loops
      if (succ == &BB) {
        continue;
      }

      // if successor uses phi node from B, then we cannot delete it.
      bool contains_phi = false;
      for (auto &phi : succ->phis()) {
        if (phi.getBasicBlockIndex(&BB) != -1) {
          contains_phi = true;
          break;
        }
      }

      if (contains_phi) {
        continue;
      }

      BB.replaceAllUsesWith(succ);
      BB.eraseFromParent();
      changed = should_rerun = true;

      // after turn A->B->C into A->C, C might be the new candidate for this opt
      // we should rerun this loop
    }
  }

  return changed;
}

bool canMerge(BasicBlock *A, BasicBlock *B) {
  // 1. A unconditionally jmp to B
  bool cond1 = false;
  if (auto br = dyn_cast<BranchInst>(A->getTerminator());
      br->isUnconditional() && A->getSingleSuccessor() == B) {
    cond1 = true;
  }
  if (!cond1) {
    return false;
  }

  // 2. B has only one predecessor A
  bool cond2 = B->hasNPredecessors(1) && B->getSinglePredecessor() == A;
  if (!cond2) {
    return false;
  }

  // 3. B has no complicated terminator (we will simplify: B's terminator is
  // Branch)
  auto br = dyn_cast<BranchInst>(A->getTerminator());
  if (!br) {
    return false;
  }

  // 4. B has only trivial phi node (we will reject all B that contains phi
  // node)
  if (!B->phis().empty()) {
    return false;
  }

  return true;
}

// Merge A, B that A unconditionally br to B and B has only one pred A
bool mergeBBs(Function &Func) {
  bool changed = false;
  std::deque<BasicBlock *> worklist;
  worklist.push_back(&Func.getEntryBlock());
  std::set<BasicBlock *> visited;

  // we will iterate over the cfg
  while (!worklist.empty()) {
    BasicBlock *A = worklist.front();
    worklist.pop_front();

    // has visited
    if (!visited.insert(A).second) {
      continue;
    }

    if (auto br = dyn_cast<BranchInst>(A->getTerminator());
        br && br->isUnconditional()) {
      auto B = A->getSingleSuccessor();
      if (canMerge(A, B)) {
        A->getTerminator()->removeFromParent();
        A->splice(A->end(), B);
        for (auto succ : successors(A)) {
          for (auto &phi : succ->phis()) {
            phi.replaceIncomingBlockWith(B, A);
          }
        }
        B->eraseFromParent();
      }
    }

    for (auto succ : successors(A)) {
      worklist.push_back(succ);
    }
  }
  return changed;
}

//------------------------------------------------------------------------------
// SimplifyCFG Pass Implementation
// will do the following things:
// 1. Remove bbs that has no solid instructions but unconditional branch
// 3. Merge A, B that A unconditionally br to B and B has only one pred A
// 3. Hoisting and Sinking for simple `if else` structure
// 4. Optional, turn phinode into select inst for simple `if else` structure
//------------------------------------------------------------------------------
PreservedAnalyses SimplifyCFG::run(Function &Func,
                                   llvm::FunctionAnalysisManager &FAM) {

  bool changed = false;

  // 1. Remove bbs that has no solid instructions but unconditional branch
  changed |= removeEmptyUnconditionBrBlocks(Func);

  // 2. Merge A, B that A unconditionally br to B and B has only one pred A
  changed |= mergeBBs(Func);

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
