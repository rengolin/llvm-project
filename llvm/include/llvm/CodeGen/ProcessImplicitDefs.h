//===- llvm/CodeGen/ProcessImplicitDefs.h -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_PROCESSIMPLICITDEFS_H
#define LLVM_CODEGEN_PROCESSIMPLICITDEFS_H

#include "llvm/CodeGen/MachinePassManager.h"

namespace llvm {

class ProcessImplicitDefsPass : public PassInfoMixin<ProcessImplicitDefsPass> {
public:
  PreservedAnalyses run(MachineFunction &MF,
                        MachineFunctionAnalysisManager &MFAM);
  MachineFunctionProperties getRequiredProperties() const {
    return MachineFunctionProperties().setIsSSA();
  }
};

} // namespace llvm

#endif // LLVM_CODEGEN_PROCESSIMPLICITDEFS_H
