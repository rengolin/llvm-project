//===- LinalgEnums.h - Linalg enums interfaces ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the operation enums for Linalg operations. This file was
// created to avoid having to include Linalg.h for the enums and break the cross
// dependency between Linalg.h and LinalgInterfaces.h, which have dependencies
// on this file, but not each other.
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_DIALECT_LINALG_IR_LINALGENUMS_H_
#define MLIR_DIALECT_LINALG_IR_LINALGENUMS_H_

/// Include the generated linalg ops enums declarations.
#include "mlir/Dialect/Linalg/IR/LinalgOpsEnums.h.inc"

#endif // MLIR_DIALECT_LINALG_IR_LINALGENUMS_H_
