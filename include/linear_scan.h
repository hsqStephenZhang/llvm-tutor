#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/VirtRegMap.h"

namespace llvm {
class MyLinearScan : public MachineFunctionPass {
  bool runOnMachineFunction(MachineFunction &MF);
};
} // namespace llvm
