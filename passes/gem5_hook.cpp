#include <llvm/IR/Analysis.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/ConstantFolder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Transforms/Utils/Local.h>

#include <llvm/IR/InstIterator.h>
#include <llvm/Passes/PassBuilder.h>

using namespace llvm;

class Gem5Hook : public PassInfoMixin<Gem5Hook> {
public:
  PreservedAnalyses run(Function &Func, llvm::FunctionAnalysisManager &FAM);
};

//------------------------------------------------------------------------------
// Insert `m5_reset_stats(0, 0);` at the beginning of the `main` function
// and `m5_dump_stats(0, 0);` at the end of the `main` function
//------------------------------------------------------------------------------
PreservedAnalyses Gem5Hook::run(Function &Func,
                                llvm::FunctionAnalysisManager &FAM) {

  // Only modify the main function
  if (Func.getName() != "main") {
    return PreservedAnalyses::all();
  }

  // declare m5_reset_stats and m5_dump_stats functions
  LLVMContext &Context = Func.getContext();
  Type *VoidTy = Type::getVoidTy(Context);
  Type *Int32Ty = Type::getInt32Ty(Context);
  std::vector<Type *> ParamTypes = {Int32Ty, Int32Ty};
  FunctionType *M5FuncTy = FunctionType::get(VoidTy, ParamTypes, false);
  FunctionCallee M5ResetStatsFunc =
      Func.getParent()->getOrInsertFunction("m5_reset_stats", M5FuncTy);
  FunctionCallee M5DumpStatsFunc =
      Func.getParent()->getOrInsertFunction("m5_dump_stats", M5FuncTy);

  auto *arg = ConstantInt::get(Int32Ty, 0);

  auto entry = &Func.getEntryBlock();
  auto reset_call = CallInst::Create(M5ResetStatsFunc, {arg, arg}, "");
  reset_call->insertBefore(entry->begin());

  for (auto &BB : Func) {
    auto *term = BB.getTerminator();
    if (isa<ReturnInst>(term)) {
      auto dump_call = CallInst::Create(M5DumpStatsFunc, {arg, arg}, "");
      dump_call->insertBefore(term);
    }
  }

  return PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
PassPluginLibraryInfo getGem5HookPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "gem5-hook", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "gem5-hook") {
                    FPM.addPass(Gem5Hook());
                    return true;
                  }

                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getGem5HookPluginInfo();
}
