#include "linear_scan.h"

using namespace llvm;

bool MyLinearScan::runOnMachineFunction(MachineFunction &MF) {
  errs() << "Running MyLinearScan on function: " << MF.getName() << "\n";
  return false;
}