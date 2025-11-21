//=============================================================================
// FILE:
//    HelloWorld.cpp
//
// DESCRIPTION:
//    Visits all functions in a module, prints their names and the number of
//    arguments via stderr. Strictly speaking, this is an analysis pass (i.e.
//    the functions are not modified). However, in order to keep things simple
//    there's no 'print' method here (every analysis pass should implement it).
//
// USAGE:
//    New PM
//      opt -load-pass-plugin=libHelloWorld.dylib -passes="hello-world" `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//=============================================================================
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <llvm/IR/BasicBlock.h>

#include <llvm/IR/CFG.h>
#include <map>
#include <vector>

using namespace llvm;

//-----------------------------------------------------------------------------
// HelloWorld implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {

static void dfsPostOrder(BasicBlock *BB, std::map<BasicBlock *, bool> &visited,
                         std::vector<BasicBlock *> &order) {
  visited[BB] = true;
  for (auto Succ : successors(BB)) {
    if (!visited[Succ]) {
      dfsPostOrder(Succ, visited, order);
    }
  }
  order.push_back(BB);
}

std::vector<BasicBlock *> computeRPO(Function &F) {
  std::map<BasicBlock *, bool> visited;
  std::vector<BasicBlock *> order;
  for (auto &BB : F) {
    visited[&BB] = false;
  }

  for (auto &BB : F) {
    if (!visited[&BB]) {
      dfsPostOrder(&BB, visited, order);
    }
  }

  std::reverse(order.begin(), order.end());
  return order;
}

class DomTreeNode {
public:
  BasicBlock *BB;
  std::vector<DomTreeNode *> Children;
};

// New PM implementation
struct RpoTraverse : PassInfoMixin<RpoTraverse> {
  // Main entry point, takes IR unit to run the pass on (&F) and the
  // corresponding pass manager (to be queried if need be)
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    auto entry = &F.getEntryBlock();
    std::unordered_map<BasicBlock *, BasicBlock *> idom;
    idom[entry] = entry;

    auto rpo = computeRPO(F);
    std::unordered_map<BasicBlock *, int> bb_rpo_num;
    for (int i = 0; i < (int)rpo.size(); i++) {
      bb_rpo_num[rpo[i]] = i;
    }

    // print rpo num
    // for (auto &BB : F) {
    //   errs() << "  BasicBlock: " << BB.getName()
    //          << ", rpo num: " << bb_rpo_num[&BB] << "\n";
    // }

    // Cooper-Harvey-Kennedy Algorithm
    bool changed = true;

    while (changed) {
      changed = false;
      for (auto B : rpo) {
        if (B == entry)
          continue;

        BasicBlock *new_idom = nullptr;

        for (auto pred : predecessors(B)) {
          if (idom[pred]) {
            if (new_idom == nullptr) { // first pred that idom is not null
              new_idom = pred;
            } else {
              // compute the intersect
              BasicBlock *b1 = new_idom;
              BasicBlock *b2 = pred;

              while (b1 != b2) {
                while (b1 != entry && bb_rpo_num[b1] > bb_rpo_num[b2]) {
                  b1 = idom[b1];
                }
                while (b2 != entry && bb_rpo_num[b1] < bb_rpo_num[b2]) {
                  b2 = idom[b2];
                }
              }
              assert(b1 == b2);
              new_idom = b1;
            }
          }
        }

        if (idom[B] != new_idom) {
          idom[B] = new_idom;
          changed = true;
        }
      }
    }

    // print the idom map
    // errs() << "Function: " << F.getName() << "\n";
    // for (auto &BB : F) {
    //   errs() << "  BasicBlock: " << BB.getName()
    //          << ", idom: " << idom[&BB]->getName() << "\n";
    // }

    std::map<BasicBlock *, DomTreeNode *> node_map;
    for (auto &BB : F) {
      DomTreeNode *node = new DomTreeNode();
      node->BB = &BB;
      node_map[&BB] = node;
    }
    for (auto &BB : F) {
      if (idom[&BB] != &BB) {
        DomTreeNode *parent = node_map[idom[&BB]];
        DomTreeNode *child = node_map[&BB];
        parent->Children.push_back(child);
      }
    }

    auto root = node_map[entry];
    std::function<void(DomTreeNode *, int)> printDomTree;
    printDomTree = [&](DomTreeNode *node, int depth) {
      for (int i = 0; i < depth; i++) {
        errs() << "  ";
      }
      errs() << "BB_" << node->BB->getNumber() << "%" << node->BB->getName()
             << "\n";
      for (auto child : node->Children) {
        printDomTree(child, depth + 1);
      }
    };
    errs() << "Dominator Tree:\n";
    printDomTree(root, 0);

    for (auto &BB : F) {
      delete node_map[&BB];
    }

    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};
} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getHelloWorldPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "my-domtree", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "my-domtree") {
                    FPM.addPass(RpoTraverse());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getHelloWorldPluginInfo();
}
