#include "simplifycfg.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassPlugin.h"
#include <deque>
#include <llvm/ADT/ADL.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/ConstantFolder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>
#include <llvm/Transforms/Utils/Local.h>

#include <llvm/IR/InstIterator.h>
#include <llvm/Passes/PassBuilder.h>
#include <map>

using namespace llvm;

bool removeUnreachable(Function &Func) {
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

bool mergeBBs(Function &Func) {
  bool changed = false;
  // std::deque<BasicBlock *> worklist;
  // worklist.push_back(&Func.getEntryBlock());

  // std::map<BasicBlock *, bool> visited;

  // while (!worklist.empty()) {
  //   BasicBlock *A = worklist.front();
  //   worklist.pop_front();

  //   if (visited[A]) {
  //     continue;
  //   } else {
  //     visited[A] = true;
  //   }

  //   if (auto br = dyn_cast<BranchInst>(A->getTerminator())) {

  //   } else {
  //     for (auto succ : successors(A)) {
  //       worklist.push_back(succ);
  //     }
  //   }
  // }
  return changed;
}

//------------------------------------------------------------------------------
// SimplifyCFG Pass Implementation
// will do the following things:
// 1. Remove unreachable bbs
// 2. Remove bbs that has no solid instructions but unconditional branch
// 3. Merge A, B that A unconditionally br to B and B has only one pred A
// 4. Hoisting and Sinking for simple `if else` structure
// 5. Optional, turn phinode into select inst for simple `if else` structure
//------------------------------------------------------------------------------
PreservedAnalyses SimplifyCFG::run(Function &Func,
                                   llvm::FunctionAnalysisManager &FAM) {

  bool changed = false;

  // 1. remove unreachable bbs
  changed |= removeUnreachable(Func);

  // 2. Remove bbs that has no solid instructions but unconditional branch
  changed |= removeEmptyUnconditionBrBlocks(Func);

  // 3. Merge A, B that A unconditionally br to B and B has only one pred A
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
