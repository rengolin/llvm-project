//===- AffineMap.cpp - MLIR Affine Map Classes ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir/IR/AffineMap.h"
#include "AffineMapDetail.h"
#include "mlir/IR/AffineExpr.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/MathExtras.h"
#include <numeric>
#include <optional>
#include <type_traits>

using namespace mlir;

using llvm::divideCeilSigned;
using llvm::divideFloorSigned;
using llvm::mod;

namespace {

// AffineExprConstantFolder evaluates an affine expression using constant
// operands passed in 'operandConsts'. Returns an IntegerAttr attribute
// representing the constant value of the affine expression evaluated on
// constant 'operandConsts', or nullptr if it can't be folded.
class AffineExprConstantFolder {
public:
  AffineExprConstantFolder(unsigned numDims, ArrayRef<Attribute> operandConsts)
      : numDims(numDims), operandConsts(operandConsts) {}

  /// Attempt to constant fold the specified affine expr, or return null on
  /// failure.
  IntegerAttr constantFold(AffineExpr expr) {
    if (auto result = constantFoldImpl(expr))
      return IntegerAttr::get(IndexType::get(expr.getContext()), *result);
    return nullptr;
  }

  bool hasPoison() const { return hasPoison_; }

private:
  std::optional<int64_t> constantFoldImpl(AffineExpr expr) {
    switch (expr.getKind()) {
    case AffineExprKind::Add:
      return constantFoldBinExpr(
          expr, [](int64_t lhs, int64_t rhs) { return lhs + rhs; });
    case AffineExprKind::Mul:
      return constantFoldBinExpr(
          expr, [](int64_t lhs, int64_t rhs) { return lhs * rhs; });
    case AffineExprKind::Mod:
      return constantFoldBinExpr(
          expr, [this](int64_t lhs, int64_t rhs) -> std::optional<int64_t> {
            if (rhs < 1) {
              hasPoison_ = true;
              return std::nullopt;
            }
            return mod(lhs, rhs);
          });
    case AffineExprKind::FloorDiv:
      return constantFoldBinExpr(
          expr, [this](int64_t lhs, int64_t rhs) -> std::optional<int64_t> {
            if (rhs == 0) {
              hasPoison_ = true;
              return std::nullopt;
            }
            return divideFloorSigned(lhs, rhs);
          });
    case AffineExprKind::CeilDiv:
      return constantFoldBinExpr(
          expr, [this](int64_t lhs, int64_t rhs) -> std::optional<int64_t> {
            if (rhs == 0) {
              hasPoison_ = true;
              return std::nullopt;
            }
            return divideCeilSigned(lhs, rhs);
          });
    case AffineExprKind::Constant:
      return cast<AffineConstantExpr>(expr).getValue();
    case AffineExprKind::DimId:
      if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
              operandConsts[cast<AffineDimExpr>(expr).getPosition()]))
        return attr.getInt();
      return std::nullopt;
    case AffineExprKind::SymbolId:
      if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
              operandConsts[numDims +
                            cast<AffineSymbolExpr>(expr).getPosition()]))
        return attr.getInt();
      return std::nullopt;
    }
    llvm_unreachable("Unknown AffineExpr");
  }

  // TODO: Change these to operate on APInts too.
  std::optional<int64_t> constantFoldBinExpr(
      AffineExpr expr,
      llvm::function_ref<std::optional<int64_t>(int64_t, int64_t)> op) {
    auto binOpExpr = cast<AffineBinaryOpExpr>(expr);
    if (auto lhs = constantFoldImpl(binOpExpr.getLHS()))
      if (auto rhs = constantFoldImpl(binOpExpr.getRHS()))
        return op(*lhs, *rhs);
    return std::nullopt;
  }

  // The number of dimension operands in AffineMap containing this expression.
  unsigned numDims;
  // The constant valued operands used to evaluate this AffineExpr.
  ArrayRef<Attribute> operandConsts;
  bool hasPoison_{false};
};

} // namespace

/// Returns a single constant result affine map.
AffineMap AffineMap::getConstantMap(int64_t val, MLIRContext *context) {
  return get(/*dimCount=*/0, /*symbolCount=*/0,
             {getAffineConstantExpr(val, context)});
}

/// Returns an identity affine map (d0, ..., dn) -> (dp, ..., dn) on the most
/// minor dimensions.
AffineMap AffineMap::getMinorIdentityMap(unsigned dims, unsigned results,
                                         MLIRContext *context) {
  assert(dims >= results && "Dimension mismatch");
  auto id = AffineMap::getMultiDimIdentityMap(dims, context);
  return AffineMap::get(dims, 0, id.getResults().take_back(results), context);
}

AffineMap AffineMap::getFilteredIdentityMap(
    MLIRContext *ctx, unsigned numDims,
    llvm::function_ref<bool(AffineDimExpr)> keepDimFilter) {
  auto identityMap = getMultiDimIdentityMap(numDims, ctx);

  // Apply filter to results.
  llvm::SmallBitVector dropDimResults(numDims);
  for (auto [idx, resultExpr] : llvm::enumerate(identityMap.getResults()))
    dropDimResults[idx] = !keepDimFilter(cast<AffineDimExpr>(resultExpr));

  return identityMap.dropResults(dropDimResults);
}

bool AffineMap::isMinorIdentity() const {
  return getNumDims() >= getNumResults() &&
         *this ==
             getMinorIdentityMap(getNumDims(), getNumResults(), getContext());
}

SmallVector<unsigned> AffineMap::getBroadcastDims() const {
  SmallVector<unsigned> broadcastedDims;
  for (const auto &[resIdx, expr] : llvm::enumerate(getResults())) {
    if (auto constExpr = dyn_cast<AffineConstantExpr>(expr)) {
      if (constExpr.getValue() != 0)
        continue;
      broadcastedDims.push_back(resIdx);
    }
  }

  return broadcastedDims;
}

/// Returns true if this affine map is a minor identity up to broadcasted
/// dimensions which are indicated by value 0 in the result.
bool AffineMap::isMinorIdentityWithBroadcasting(
    SmallVectorImpl<unsigned> *broadcastedDims) const {
  if (broadcastedDims)
    broadcastedDims->clear();
  if (getNumDims() < getNumResults())
    return false;
  unsigned suffixStart = getNumDims() - getNumResults();
  for (const auto &idxAndExpr : llvm::enumerate(getResults())) {
    unsigned resIdx = idxAndExpr.index();
    AffineExpr expr = idxAndExpr.value();
    if (auto constExpr = dyn_cast<AffineConstantExpr>(expr)) {
      // Each result may be either a constant 0 (broadcasted dimension).
      if (constExpr.getValue() != 0)
        return false;
      if (broadcastedDims)
        broadcastedDims->push_back(resIdx);
    } else if (auto dimExpr = dyn_cast<AffineDimExpr>(expr)) {
      // Or it may be the input dimension corresponding to this result position.
      if (dimExpr.getPosition() != suffixStart + resIdx)
        return false;
    } else {
      return false;
    }
  }
  return true;
}

/// Return true if this affine map can be converted to a minor identity with
/// broadcast by doing a permute. Return a permutation (there may be
/// several) to apply to get to a minor identity with broadcasts.
/// Ex:
///  * (d0, d1, d2) -> (0, d1) maps to minor identity (d1, 0 = d2) with
///  perm = [1, 0] and broadcast d2
///  * (d0, d1, d2) -> (d0, 0) cannot be mapped to a minor identity by
///  permutation + broadcast
///  * (d0, d1, d2, d3) -> (0, d1, d3) maps to minor identity (d1, 0 = d2, d3)
///  with perm = [1, 0, 2] and broadcast d2
///  * (d0, d1) -> (d1, 0, 0, d0) maps to minor identity (d0, d1) with extra
///  leading broadcat dimensions. The map returned would be (0, 0, d0, d1) with
///  perm = [3, 0, 1, 2]
bool AffineMap::isPermutationOfMinorIdentityWithBroadcasting(
    SmallVectorImpl<unsigned> &permutedDims) const {
  unsigned projectionStart =
      getNumResults() < getNumInputs() ? getNumInputs() - getNumResults() : 0;
  permutedDims.clear();
  SmallVector<unsigned> broadcastDims;
  permutedDims.resize(getNumResults(), 0);
  // If there are more results than input dimensions we want the new map to
  // start with broadcast dimensions in order to be a minor identity with
  // broadcasting.
  unsigned leadingBroadcast =
      getNumResults() > getNumInputs() ? getNumResults() - getNumInputs() : 0;
  llvm::SmallBitVector dimFound(std::max(getNumInputs(), getNumResults()),
                                false);
  for (const auto &idxAndExpr : llvm::enumerate(getResults())) {
    unsigned resIdx = idxAndExpr.index();
    AffineExpr expr = idxAndExpr.value();
    // Each result may be either a constant 0 (broadcast dimension) or a
    // dimension.
    if (auto constExpr = dyn_cast<AffineConstantExpr>(expr)) {
      if (constExpr.getValue() != 0)
        return false;
      broadcastDims.push_back(resIdx);
    } else if (auto dimExpr = dyn_cast<AffineDimExpr>(expr)) {
      if (dimExpr.getPosition() < projectionStart)
        return false;
      unsigned newPosition =
          dimExpr.getPosition() - projectionStart + leadingBroadcast;
      permutedDims[resIdx] = newPosition;
      dimFound[newPosition] = true;
    } else {
      return false;
    }
  }
  // Find a permuation for the broadcast dimension. Since they are broadcasted
  // any valid permutation is acceptable. We just permute the dim into a slot
  // without an existing dimension.
  unsigned pos = 0;
  for (auto dim : broadcastDims) {
    while (pos < dimFound.size() && dimFound[pos]) {
      pos++;
    }
    permutedDims[dim] = pos++;
  }
  return true;
}

/// Returns an AffineMap representing a permutation.
AffineMap AffineMap::getPermutationMap(ArrayRef<unsigned> permutation,
                                       MLIRContext *context) {
  assert(!permutation.empty() &&
         "Cannot create permutation map from empty permutation vector");
  const auto *m = llvm::max_element(permutation);
  auto permutationMap = getMultiDimMapWithTargets(*m + 1, permutation, context);
  assert(permutationMap.isPermutation() && "Invalid permutation vector");
  return permutationMap;
}
AffineMap AffineMap::getPermutationMap(ArrayRef<int64_t> permutation,
                                       MLIRContext *context) {
  SmallVector<unsigned> perm = llvm::map_to_vector(
      permutation, [](int64_t i) { return static_cast<unsigned>(i); });
  return AffineMap::getPermutationMap(perm, context);
}

AffineMap AffineMap::getMultiDimMapWithTargets(unsigned numDims,
                                               ArrayRef<unsigned> targets,
                                               MLIRContext *context) {
  SmallVector<AffineExpr, 4> affExprs;
  for (unsigned t : targets)
    affExprs.push_back(getAffineDimExpr(t, context));
  AffineMap result = AffineMap::get(/*dimCount=*/numDims, /*symbolCount=*/0,
                                    affExprs, context);
  return result;
}

/// Creates an affine map each for each list of AffineExpr's in `exprsList`
/// while inferring the right number of dimensional and symbolic inputs needed
/// based on the maximum dimensional and symbolic identifier appearing in the
/// expressions.
template <typename AffineExprContainer>
static SmallVector<AffineMap, 4>
inferFromExprList(ArrayRef<AffineExprContainer> exprsList,
                  MLIRContext *context) {
  if (exprsList.empty())
    return {};
  int64_t maxDim = -1, maxSym = -1;
  getMaxDimAndSymbol(exprsList, maxDim, maxSym);
  SmallVector<AffineMap, 4> maps;
  maps.reserve(exprsList.size());
  for (const auto &exprs : exprsList)
    maps.push_back(AffineMap::get(/*dimCount=*/maxDim + 1,
                                  /*symbolCount=*/maxSym + 1, exprs, context));
  return maps;
}

SmallVector<AffineMap, 4>
AffineMap::inferFromExprList(ArrayRef<ArrayRef<AffineExpr>> exprsList,
                             MLIRContext *context) {
  return ::inferFromExprList(exprsList, context);
}

SmallVector<AffineMap, 4>
AffineMap::inferFromExprList(ArrayRef<SmallVector<AffineExpr, 4>> exprsList,
                             MLIRContext *context) {
  return ::inferFromExprList(exprsList, context);
}

uint64_t AffineMap::getLargestKnownDivisorOfMapExprs() {
  uint64_t gcd = 0;
  for (AffineExpr resultExpr : getResults()) {
    uint64_t thisGcd = resultExpr.getLargestKnownDivisor();
    gcd = std::gcd(gcd, thisGcd);
  }
  if (gcd == 0)
    gcd = std::numeric_limits<uint64_t>::max();
  return gcd;
}

AffineMap AffineMap::getMultiDimIdentityMap(unsigned numDims,
                                            MLIRContext *context) {
  SmallVector<AffineExpr, 4> dimExprs;
  dimExprs.reserve(numDims);
  for (unsigned i = 0; i < numDims; ++i)
    dimExprs.push_back(mlir::getAffineDimExpr(i, context));
  return get(/*dimCount=*/numDims, /*symbolCount=*/0, dimExprs, context);
}

MLIRContext *AffineMap::getContext() const { return map->context; }

bool AffineMap::isIdentity() const {
  if (getNumDims() != getNumResults())
    return false;
  ArrayRef<AffineExpr> results = getResults();
  for (unsigned i = 0, numDims = getNumDims(); i < numDims; ++i) {
    auto expr = dyn_cast<AffineDimExpr>(results[i]);
    if (!expr || expr.getPosition() != i)
      return false;
  }
  return true;
}

bool AffineMap::isSymbolIdentity() const {
  if (getNumSymbols() != getNumResults())
    return false;
  ArrayRef<AffineExpr> results = getResults();
  for (unsigned i = 0, numSymbols = getNumSymbols(); i < numSymbols; ++i) {
    auto expr = dyn_cast<AffineDimExpr>(results[i]);
    if (!expr || expr.getPosition() != i)
      return false;
  }
  return true;
}

bool AffineMap::isEmpty() const {
  return getNumDims() == 0 && getNumSymbols() == 0 && getNumResults() == 0;
}

bool AffineMap::isSingleConstant() const {
  return getNumResults() == 1 && isa<AffineConstantExpr>(getResult(0));
}

bool AffineMap::isConstant() const {
  return llvm::all_of(getResults(), llvm::IsaPred<AffineConstantExpr>);
}

int64_t AffineMap::getSingleConstantResult() const {
  assert(isSingleConstant() && "map must have a single constant result");
  return cast<AffineConstantExpr>(getResult(0)).getValue();
}

SmallVector<int64_t> AffineMap::getConstantResults() const {
  assert(isConstant() && "map must have only constant results");
  SmallVector<int64_t> result;
  for (auto expr : getResults())
    result.emplace_back(cast<AffineConstantExpr>(expr).getValue());
  return result;
}

unsigned AffineMap::getNumDims() const {
  assert(map && "uninitialized map storage");
  return map->numDims;
}
unsigned AffineMap::getNumSymbols() const {
  assert(map && "uninitialized map storage");
  return map->numSymbols;
}
unsigned AffineMap::getNumResults() const { return getResults().size(); }
unsigned AffineMap::getNumInputs() const {
  assert(map && "uninitialized map storage");
  return map->numDims + map->numSymbols;
}
ArrayRef<AffineExpr> AffineMap::getResults() const {
  assert(map && "uninitialized map storage");
  return map->results();
}
AffineExpr AffineMap::getResult(unsigned idx) const {
  return getResults()[idx];
}

unsigned AffineMap::getDimPosition(unsigned idx) const {
  return cast<AffineDimExpr>(getResult(idx)).getPosition();
}

std::optional<unsigned> AffineMap::getResultPosition(AffineExpr input) const {
  if (!isa<AffineDimExpr>(input))
    return std::nullopt;

  for (unsigned i = 0, numResults = getNumResults(); i < numResults; i++) {
    if (getResult(i) == input)
      return i;
  }

  return std::nullopt;
}

/// Folds the results of the application of an affine map on the provided
/// operands to a constant if possible. Returns false if the folding happens,
/// true otherwise.
LogicalResult AffineMap::constantFold(ArrayRef<Attribute> operandConstants,
                                      SmallVectorImpl<Attribute> &results,
                                      bool *hasPoison) const {
  // Attempt partial folding.
  SmallVector<int64_t, 2> integers;
  partialConstantFold(operandConstants, &integers, hasPoison);

  // If all expressions folded to a constant, populate results with attributes
  // containing those constants.
  if (integers.empty())
    return failure();

  auto range = llvm::map_range(integers, [this](int64_t i) {
    return IntegerAttr::get(IndexType::get(getContext()), i);
  });
  results.append(range.begin(), range.end());
  return success();
}

AffineMap AffineMap::partialConstantFold(ArrayRef<Attribute> operandConstants,
                                         SmallVectorImpl<int64_t> *results,
                                         bool *hasPoison) const {
  assert(getNumInputs() == operandConstants.size());

  // Fold each of the result expressions.
  AffineExprConstantFolder exprFolder(getNumDims(), operandConstants);
  SmallVector<AffineExpr, 4> exprs;
  exprs.reserve(getNumResults());

  for (auto expr : getResults()) {
    auto folded = exprFolder.constantFold(expr);
    if (exprFolder.hasPoison() && hasPoison) {
      *hasPoison = true;
      return {};
    }
    // If did not fold to a constant, keep the original expression, and clear
    // the integer results vector.
    if (folded) {
      exprs.push_back(
          getAffineConstantExpr(folded.getInt(), folded.getContext()));
      if (results)
        results->push_back(folded.getInt());
    } else {
      exprs.push_back(expr);
      if (results) {
        results->clear();
        results = nullptr;
      }
    }
  }

  return get(getNumDims(), getNumSymbols(), exprs, getContext());
}

/// Walk all of the AffineExpr's in this mapping. Each node in an expression
/// tree is visited in postorder.
void AffineMap::walkExprs(llvm::function_ref<void(AffineExpr)> callback) const {
  for (auto expr : getResults())
    expr.walk(callback);
}

/// This method substitutes any uses of dimensions and symbols (e.g.
/// dim#0 with dimReplacements[0]) in subexpressions and returns the modified
/// expression mapping.  Because this can be used to eliminate dims and
/// symbols, the client needs to specify the number of dims and symbols in
/// the result.  The returned map always has the same number of results.
AffineMap AffineMap::replaceDimsAndSymbols(ArrayRef<AffineExpr> dimReplacements,
                                           ArrayRef<AffineExpr> symReplacements,
                                           unsigned numResultDims,
                                           unsigned numResultSyms) const {
  SmallVector<AffineExpr, 8> results;
  results.reserve(getNumResults());
  for (auto expr : getResults())
    results.push_back(
        expr.replaceDimsAndSymbols(dimReplacements, symReplacements));
  return get(numResultDims, numResultSyms, results, getContext());
}

/// Sparse replace method. Apply AffineExpr::replace(`expr`, `replacement`) to
/// each of the results and return a new AffineMap with the new results and
/// with the specified number of dims and symbols.
AffineMap AffineMap::replace(AffineExpr expr, AffineExpr replacement,
                             unsigned numResultDims,
                             unsigned numResultSyms) const {
  SmallVector<AffineExpr, 4> newResults;
  newResults.reserve(getNumResults());
  for (AffineExpr e : getResults())
    newResults.push_back(e.replace(expr, replacement));
  return AffineMap::get(numResultDims, numResultSyms, newResults, getContext());
}

/// Sparse replace method. Apply AffineExpr::replace(`map`) to each of the
/// results and return a new AffineMap with the new results and with the
/// specified number of dims and symbols.
AffineMap AffineMap::replace(const DenseMap<AffineExpr, AffineExpr> &map,
                             unsigned numResultDims,
                             unsigned numResultSyms) const {
  SmallVector<AffineExpr, 4> newResults;
  newResults.reserve(getNumResults());
  for (AffineExpr e : getResults())
    newResults.push_back(e.replace(map));
  return AffineMap::get(numResultDims, numResultSyms, newResults, getContext());
}

AffineMap
AffineMap::replace(const DenseMap<AffineExpr, AffineExpr> &map) const {
  SmallVector<AffineExpr, 4> newResults;
  newResults.reserve(getNumResults());
  for (AffineExpr e : getResults())
    newResults.push_back(e.replace(map));
  return AffineMap::inferFromExprList(newResults, getContext()).front();
}

AffineMap AffineMap::dropResults(const llvm::SmallBitVector &positions) const {
  auto exprs = llvm::to_vector<4>(getResults());
  // TODO: this is a pretty terrible API .. is there anything better?
  for (auto pos = positions.find_last(); pos != -1;
       pos = positions.find_prev(pos))
    exprs.erase(exprs.begin() + pos);
  return AffineMap::get(getNumDims(), getNumSymbols(), exprs, getContext());
}

AffineMap AffineMap::compose(AffineMap map) const {
  assert(getNumDims() == map.getNumResults() && "Number of results mismatch");
  // Prepare `map` by concatenating the symbols and rewriting its exprs.
  unsigned numDims = map.getNumDims();
  unsigned numSymbolsThisMap = getNumSymbols();
  unsigned numSymbols = numSymbolsThisMap + map.getNumSymbols();
  SmallVector<AffineExpr, 8> newDims(numDims);
  for (unsigned idx = 0; idx < numDims; ++idx) {
    newDims[idx] = getAffineDimExpr(idx, getContext());
  }
  SmallVector<AffineExpr, 8> newSymbols(numSymbols - numSymbolsThisMap);
  for (unsigned idx = numSymbolsThisMap; idx < numSymbols; ++idx) {
    newSymbols[idx - numSymbolsThisMap] =
        getAffineSymbolExpr(idx, getContext());
  }
  auto newMap =
      map.replaceDimsAndSymbols(newDims, newSymbols, numDims, numSymbols);
  SmallVector<AffineExpr, 8> exprs;
  exprs.reserve(getResults().size());
  for (auto expr : getResults())
    exprs.push_back(expr.compose(newMap));
  return AffineMap::get(numDims, numSymbols, exprs, map.getContext());
}

SmallVector<int64_t, 4> AffineMap::compose(ArrayRef<int64_t> values) const {
  assert(getNumSymbols() == 0 && "Expected symbol-less map");
  SmallVector<AffineExpr, 4> exprs;
  MLIRContext *ctx = getContext();
  for (int64_t value : values)
    exprs.push_back(getAffineConstantExpr(value, ctx));
  SmallVector<int64_t, 4> res;
  res.reserve(getNumResults());
  for (auto e : getResults())
    res.push_back(cast<AffineConstantExpr>(e.replaceDims(exprs)).getValue());
  return res;
}

size_t AffineMap::getNumOfZeroResults() const {
  size_t res = 0;
  for (auto expr : getResults()) {
    auto constExpr = dyn_cast<AffineConstantExpr>(expr);
    if (constExpr && constExpr.getValue() == 0)
      res++;
  }

  return res;
}

AffineMap AffineMap::dropZeroResults() {
  SmallVector<AffineExpr> newExprs;

  for (auto expr : getResults()) {
    auto constExpr = dyn_cast<AffineConstantExpr>(expr);
    if (!constExpr || constExpr.getValue() != 0)
      newExprs.push_back(expr);
  }
  return AffineMap::get(getNumDims(), getNumSymbols(), newExprs, getContext());
}

bool AffineMap::isProjectedPermutation(bool allowZeroInResults) const {
  if (getNumSymbols() > 0)
    return false;

  // Having more results than inputs means that results have duplicated dims or
  // zeros that can't be mapped to input dims.
  if (getNumResults() > getNumInputs())
    return false;

  SmallVector<bool, 8> seen(getNumInputs(), false);
  // A projected permutation can have, at most, only one instance of each input
  // dimension in the result expressions. Zeros are allowed as long as the
  // number of result expressions is lower or equal than the number of input
  // expressions.
  for (auto expr : getResults()) {
    if (auto dim = dyn_cast<AffineDimExpr>(expr)) {
      if (seen[dim.getPosition()])
        return false;
      seen[dim.getPosition()] = true;
    } else {
      auto constExpr = dyn_cast<AffineConstantExpr>(expr);
      if (!allowZeroInResults || !constExpr || constExpr.getValue() != 0)
        return false;
    }
  }

  // Results are either dims or zeros and zeros can be mapped to input dims.
  return true;
}

bool AffineMap::isPermutation() const {
  if (getNumDims() != getNumResults())
    return false;
  return isProjectedPermutation();
}

AffineMap AffineMap::getSubMap(ArrayRef<unsigned> resultPos) const {
  SmallVector<AffineExpr, 4> exprs;
  exprs.reserve(resultPos.size());
  for (auto idx : resultPos)
    exprs.push_back(getResult(idx));
  return AffineMap::get(getNumDims(), getNumSymbols(), exprs, getContext());
}

AffineMap AffineMap::getSliceMap(unsigned start, unsigned length) const {
  return AffineMap::get(getNumDims(), getNumSymbols(),
                        getResults().slice(start, length), getContext());
}

AffineMap AffineMap::getMajorSubMap(unsigned numResults) const {
  if (numResults == 0)
    return AffineMap();
  if (numResults > getNumResults())
    return *this;
  return getSliceMap(0, numResults);
}

AffineMap AffineMap::getMinorSubMap(unsigned numResults) const {
  if (numResults == 0)
    return AffineMap();
  if (numResults > getNumResults())
    return *this;
  return getSliceMap(getNumResults() - numResults, numResults);
}

/// Implementation detail to compress multiple affine maps with a compressionFun
/// that is expected to be either compressUnusedDims or compressUnusedSymbols.
/// The implementation keeps track of num dims and symbols across the different
/// affine maps.
static SmallVector<AffineMap> compressUnusedListImpl(
    ArrayRef<AffineMap> maps,
    llvm::function_ref<AffineMap(AffineMap)> compressionFun) {
  if (maps.empty())
    return SmallVector<AffineMap>();
  SmallVector<AffineExpr> allExprs;
  allExprs.reserve(maps.size() * maps.front().getNumResults());
  unsigned numDims = maps.front().getNumDims(),
           numSymbols = maps.front().getNumSymbols();
  for (auto m : maps) {
    assert(numDims == m.getNumDims() && numSymbols == m.getNumSymbols() &&
           "expected maps with same num dims and symbols");
    llvm::append_range(allExprs, m.getResults());
  }
  AffineMap unifiedMap = compressionFun(
      AffineMap::get(numDims, numSymbols, allExprs, maps.front().getContext()));
  unsigned unifiedNumDims = unifiedMap.getNumDims(),
           unifiedNumSymbols = unifiedMap.getNumSymbols();
  ArrayRef<AffineExpr> unifiedResults = unifiedMap.getResults();
  SmallVector<AffineMap> res;
  res.reserve(maps.size());
  for (auto m : maps) {
    res.push_back(AffineMap::get(unifiedNumDims, unifiedNumSymbols,
                                 unifiedResults.take_front(m.getNumResults()),
                                 m.getContext()));
    unifiedResults = unifiedResults.drop_front(m.getNumResults());
  }
  return res;
}

AffineMap mlir::compressDims(AffineMap map,
                             const llvm::SmallBitVector &unusedDims) {
  return projectDims(map, unusedDims, /*compressDimsFlag=*/true);
}

AffineMap mlir::compressUnusedDims(AffineMap map) {
  return compressDims(map, getUnusedDimsBitVector({map}));
}

SmallVector<AffineMap> mlir::compressUnusedDims(ArrayRef<AffineMap> maps) {
  return compressUnusedListImpl(
      maps, [](AffineMap m) { return compressUnusedDims(m); });
}

AffineMap mlir::compressSymbols(AffineMap map,
                                const llvm::SmallBitVector &unusedSymbols) {
  return projectSymbols(map, unusedSymbols, /*compressSymbolsFlag=*/true);
}

AffineMap mlir::compressUnusedSymbols(AffineMap map) {
  return compressSymbols(map, getUnusedSymbolsBitVector({map}));
}

SmallVector<AffineMap> mlir::compressUnusedSymbols(ArrayRef<AffineMap> maps) {
  return compressUnusedListImpl(
      maps, [](AffineMap m) { return compressUnusedSymbols(m); });
}

AffineMap mlir::foldAttributesIntoMap(Builder &b, AffineMap map,
                                      ArrayRef<OpFoldResult> operands,
                                      SmallVector<Value> &remainingValues) {
  SmallVector<AffineExpr> dimReplacements, symReplacements;
  int64_t numDims = 0;
  for (int64_t i = 0; i < map.getNumDims(); ++i) {
    if (auto attr = dyn_cast<Attribute>(operands[i])) {
      dimReplacements.push_back(
          b.getAffineConstantExpr(cast<IntegerAttr>(attr).getInt()));
    } else {
      dimReplacements.push_back(b.getAffineDimExpr(numDims++));
      remainingValues.push_back(cast<Value>(operands[i]));
    }
  }
  int64_t numSymbols = 0;
  for (int64_t i = 0; i < map.getNumSymbols(); ++i) {
    if (auto attr = dyn_cast<Attribute>(operands[i + map.getNumDims()])) {
      symReplacements.push_back(
          b.getAffineConstantExpr(cast<IntegerAttr>(attr).getInt()));
    } else {
      symReplacements.push_back(b.getAffineSymbolExpr(numSymbols++));
      remainingValues.push_back(cast<Value>(operands[i + map.getNumDims()]));
    }
  }
  return map.replaceDimsAndSymbols(dimReplacements, symReplacements, numDims,
                                   numSymbols);
}

AffineMap mlir::simplifyAffineMap(AffineMap map) {
  SmallVector<AffineExpr, 8> exprs;
  for (auto e : map.getResults()) {
    exprs.push_back(
        simplifyAffineExpr(e, map.getNumDims(), map.getNumSymbols()));
  }
  return AffineMap::get(map.getNumDims(), map.getNumSymbols(), exprs,
                        map.getContext());
}

AffineMap mlir::removeDuplicateExprs(AffineMap map) {
  auto results = map.getResults();
  SmallVector<AffineExpr, 4> uniqueExprs(results);
  uniqueExprs.erase(llvm::unique(uniqueExprs), uniqueExprs.end());
  return AffineMap::get(map.getNumDims(), map.getNumSymbols(), uniqueExprs,
                        map.getContext());
}

AffineMap mlir::inversePermutation(AffineMap map) {
  if (map.isEmpty())
    return map;
  assert(map.getNumSymbols() == 0 && "expected map without symbols");
  SmallVector<AffineExpr, 4> exprs(map.getNumDims());
  for (const auto &en : llvm::enumerate(map.getResults())) {
    auto expr = en.value();
    // Skip non-permutations.
    if (auto d = dyn_cast<AffineDimExpr>(expr)) {
      if (exprs[d.getPosition()])
        continue;
      exprs[d.getPosition()] = getAffineDimExpr(en.index(), d.getContext());
    }
  }
  SmallVector<AffineExpr, 4> seenExprs;
  seenExprs.reserve(map.getNumDims());
  for (auto expr : exprs)
    if (expr)
      seenExprs.push_back(expr);
  if (seenExprs.size() != map.getNumInputs())
    return AffineMap();
  return AffineMap::get(map.getNumResults(), 0, seenExprs, map.getContext());
}

AffineMap mlir::inverseAndBroadcastProjectedPermutation(AffineMap map) {
  assert(map.isProjectedPermutation(/*allowZeroInResults=*/true));
  MLIRContext *context = map.getContext();
  AffineExpr zero = mlir::getAffineConstantExpr(0, context);
  // Start with all the results as 0.
  SmallVector<AffineExpr, 4> exprs(map.getNumInputs(), zero);
  for (unsigned i : llvm::seq(unsigned(0), map.getNumResults())) {
    // Skip zeros from input map. 'exprs' is already initialized to zero.
    if (auto constExpr = dyn_cast<AffineConstantExpr>(map.getResult(i))) {
      assert(constExpr.getValue() == 0 &&
             "Unexpected constant in projected permutation");
      (void)constExpr;
      continue;
    }

    // Reverse each dimension existing in the original map result.
    exprs[map.getDimPosition(i)] = getAffineDimExpr(i, context);
  }
  return AffineMap::get(map.getNumResults(), /*symbolCount=*/0, exprs, context);
}

AffineMap mlir::concatAffineMaps(ArrayRef<AffineMap> maps,
                                 MLIRContext *context) {
  if (maps.empty())
    return AffineMap::get(context);
  unsigned numResults = 0, numDims = 0, numSymbols = 0;
  for (auto m : maps)
    numResults += m.getNumResults();
  SmallVector<AffineExpr, 8> results;
  results.reserve(numResults);
  for (auto m : maps) {
    for (auto res : m.getResults())
      results.push_back(res.shiftSymbols(m.getNumSymbols(), numSymbols));

    numSymbols += m.getNumSymbols();
    numDims = std::max(m.getNumDims(), numDims);
  }
  return AffineMap::get(numDims, numSymbols, results, context);
}

/// Common implementation to project out dimensions or symbols from an affine
/// map based on the template type.
/// Additionally, if 'compress' is true, the projected out dimensions or symbols
/// are also dropped from the resulting map.
template <typename AffineDimOrSymExpr>
static AffineMap projectCommonImpl(AffineMap map,
                                   const llvm::SmallBitVector &toProject,
                                   bool compress) {
  static_assert(llvm::is_one_of<AffineDimOrSymExpr, AffineDimExpr,
                                AffineSymbolExpr>::value,
                "expected AffineDimExpr or AffineSymbolExpr");

  constexpr bool isDim = std::is_same<AffineDimOrSymExpr, AffineDimExpr>::value;
  int64_t numDimOrSym = (isDim) ? map.getNumDims() : map.getNumSymbols();
  SmallVector<AffineExpr> replacements;
  replacements.reserve(numDimOrSym);

  auto createNewDimOrSym = (isDim) ? getAffineDimExpr : getAffineSymbolExpr;

  using replace_fn_ty =
      std::function<AffineExpr(AffineExpr, ArrayRef<AffineExpr>)>;
  replace_fn_ty replaceDims = [](AffineExpr e,
                                 ArrayRef<AffineExpr> replacements) {
    return e.replaceDims(replacements);
  };
  replace_fn_ty replaceSymbols = [](AffineExpr e,
                                    ArrayRef<AffineExpr> replacements) {
    return e.replaceSymbols(replacements);
  };
  replace_fn_ty replaceNewDimOrSym = (isDim) ? replaceDims : replaceSymbols;

  MLIRContext *context = map.getContext();
  int64_t newNumDimOrSym = 0;
  for (unsigned dimOrSym = 0; dimOrSym < numDimOrSym; ++dimOrSym) {
    if (toProject.test(dimOrSym)) {
      replacements.push_back(getAffineConstantExpr(0, context));
      continue;
    }
    int64_t newPos = compress ? newNumDimOrSym++ : dimOrSym;
    replacements.push_back(createNewDimOrSym(newPos, context));
  }
  SmallVector<AffineExpr> resultExprs;
  resultExprs.reserve(map.getNumResults());
  for (auto e : map.getResults())
    resultExprs.push_back(replaceNewDimOrSym(e, replacements));

  int64_t numDims = (compress && isDim) ? newNumDimOrSym : map.getNumDims();
  int64_t numSyms = (compress && !isDim) ? newNumDimOrSym : map.getNumSymbols();
  return AffineMap::get(numDims, numSyms, resultExprs, context);
}

AffineMap mlir::projectDims(AffineMap map,
                            const llvm::SmallBitVector &projectedDimensions,
                            bool compressDimsFlag) {
  return projectCommonImpl<AffineDimExpr>(map, projectedDimensions,
                                          compressDimsFlag);
}

AffineMap mlir::projectSymbols(AffineMap map,
                               const llvm::SmallBitVector &projectedSymbols,
                               bool compressSymbolsFlag) {
  return projectCommonImpl<AffineSymbolExpr>(map, projectedSymbols,
                                             compressSymbolsFlag);
}

AffineMap mlir::getProjectedMap(AffineMap map,
                                const llvm::SmallBitVector &projectedDimensions,
                                bool compressDimsFlag,
                                bool compressSymbolsFlag) {
  map = projectDims(map, projectedDimensions, compressDimsFlag);
  if (compressSymbolsFlag)
    map = compressUnusedSymbols(map);
  return map;
}

llvm::SmallBitVector mlir::getUnusedDimsBitVector(ArrayRef<AffineMap> maps) {
  unsigned numDims = maps[0].getNumDims();
  llvm::SmallBitVector numDimsBitVector(numDims, true);
  for (AffineMap m : maps) {
    for (unsigned i = 0; i < numDims; ++i) {
      if (m.isFunctionOfDim(i))
        numDimsBitVector.reset(i);
    }
  }
  return numDimsBitVector;
}

llvm::SmallBitVector mlir::getUnusedSymbolsBitVector(ArrayRef<AffineMap> maps) {
  unsigned numSymbols = maps[0].getNumSymbols();
  llvm::SmallBitVector numSymbolsBitVector(numSymbols, true);
  for (AffineMap m : maps) {
    for (unsigned i = 0; i < numSymbols; ++i) {
      if (m.isFunctionOfSymbol(i))
        numSymbolsBitVector.reset(i);
    }
  }
  return numSymbolsBitVector;
}

AffineMap
mlir::expandDimsToRank(AffineMap map, int64_t rank,
                       const llvm::SmallBitVector &projectedDimensions) {
  auto id = AffineMap::getMultiDimIdentityMap(rank, map.getContext());
  AffineMap proj = id.dropResults(projectedDimensions);
  return map.compose(proj);
}

//===----------------------------------------------------------------------===//
// MutableAffineMap.
//===----------------------------------------------------------------------===//

MutableAffineMap::MutableAffineMap(AffineMap map)
    : results(map.getResults()), numDims(map.getNumDims()),
      numSymbols(map.getNumSymbols()), context(map.getContext()) {}

void MutableAffineMap::reset(AffineMap map) {
  results.clear();
  numDims = map.getNumDims();
  numSymbols = map.getNumSymbols();
  context = map.getContext();
  llvm::append_range(results, map.getResults());
}

bool MutableAffineMap::isMultipleOf(unsigned idx, int64_t factor) const {
  return results[idx].isMultipleOf(factor);
}

// Simplifies the result affine expressions of this map. The expressions
// have to be pure for the simplification implemented.
void MutableAffineMap::simplify() {
  // Simplify each of the results if possible.
  // TODO: functional-style map
  for (unsigned i = 0, e = getNumResults(); i < e; i++) {
    results[i] = simplifyAffineExpr(getResult(i), numDims, numSymbols);
  }
}

AffineMap MutableAffineMap::getAffineMap() const {
  return AffineMap::get(numDims, numSymbols, results, context);
}
