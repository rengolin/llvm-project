//===- IndexDialect.cpp - Index dialect definition -------------------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Index/IR/IndexDialect.h"
#include "mlir/Dialect/Index/IR/IndexOps.h"
#include "mlir/Conversion/ConvertToLLVM/ToLLVMInterface.h"
#include "mlir/Transforms/InliningUtils.h"
#include "mlir/Transforms/Passes.h"

using namespace mlir;
using namespace mlir::index;

//===----------------------------------------------------------------------===//
// IndexDialect
//===----------------------------------------------------------------------===//
namespace {
/// This class defines the interface for handling inlining for index
/// dialect operations.
struct IndexInlinerInterface : public DialectInlinerInterface {
  using DialectInlinerInterface::DialectInlinerInterface;

  /// All index dialect ops can be inlined.
  bool isLegalToInline(Operation *, Region *, bool, IRMapping &) const final {
    return true;
  }
};
} // namespace

void IndexDialect::initialize() {
  registerAttributes();
  registerOperations();
  addInterfaces<IndexInlinerInterface>();
  declarePromisedInterface<ConvertToLLVMPatternInterface, IndexDialect>();
}

void IndexDialect::getCanonicalizationPatterns(
    RewritePatternSet &results, bool registerOperationCanonicalization) const {
  if (registerOperationCanonicalization) {
    CanonicalizationPatternList<
#define GET_OP_LIST
#include "mlir/Dialect/Index/IR/IndexOps.cpp.inc"
        >::insert(results);
  }
}

//===----------------------------------------------------------------------===//
// ODS-Generated Definitions
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Index/IR/IndexOpsDialect.cpp.inc"
