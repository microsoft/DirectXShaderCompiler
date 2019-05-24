//===------- InitListHandler.cpp - Initializer List Handler -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
//
//  This file implements an initalizer list handler that takes in an
//  InitListExpr and emits the corresponding SPIR-V instructions for it.
//
//===----------------------------------------------------------------------===//

#include "InitListHandler.h"
#include "clang/SPIRV/AstTypeProbe.h"

#include <algorithm>
#include <iterator>

#include "llvm/ADT/SmallVector.h"

namespace clang {
namespace spirv {

InitListHandler::InitListHandler(const ASTContext &ctx, SpirvEmitter &emitter)
    : astContext(ctx), theEmitter(emitter),
      spvBuilder(emitter.getSpirvBuilder()),
      diags(emitter.getDiagnosticsEngine()) {}

SpirvInstruction *InitListHandler::processInit(const InitListExpr *expr) {
  initializers.clear();
  scalars.clear();

  flatten(expr);
  // Reverse the whole initializer list so we can manipulate the list at the
  // tail of the vector. This is more efficient than using a deque.
  std::reverse(std::begin(initializers), std::end(initializers));

  return doProcess(expr->getType(), expr->getExprLoc());
}

SpirvInstruction *InitListHandler::processCast(QualType toType,
                                               const Expr *expr) {
  initializers.clear();
  scalars.clear();

  initializers.push_back(theEmitter.loadIfGLValue(expr));

  return doProcess(toType, expr->getExprLoc());
}

SpirvInstruction *InitListHandler::doProcess(QualType type,
                                             SourceLocation srcLoc) {
  auto *init = createInitForType(type, srcLoc);

  if (init) {
    // For successful translation, we should have consumed all initializers and
    // scalars extracted from them.
    assert(initializers.empty());
    assert(scalars.empty());
  }

  return init;
}

void InitListHandler::flatten(const InitListExpr *expr) {
  const auto numInits = expr->getNumInits();

  for (uint32_t i = 0; i < numInits; ++i) {
    const Expr *init = expr->getInit(i);
    if (const auto *subInitList = dyn_cast<InitListExpr>(init)) {
      flatten(subInitList);
    } else if (const auto *subInitList = dyn_cast<InitListExpr>(
                   // Ignore constructor casts which are no-ops
                   // For cases like: <type>(<initializer-list>)
                   init->IgnoreParenNoopCasts(theEmitter.getASTContext()))) {
      flatten(subInitList);
    } else {
      initializers.push_back(theEmitter.loadIfGLValue(init));
    }
  }
}

void InitListHandler::decompose(SpirvInstruction *inst) {
  const QualType type = inst->getAstResultType();
  auto loc = inst->getSourceLocation();

  QualType elemType = {};
  uint32_t elemCount = 0, rowCount = 0, colCount = 0;

  // Scalar cases, including vec1 and mat1x1.
  if (isScalarType(type, &elemType)) {
    scalars.emplace_back(inst, elemType);
  }
  // Vector cases, including mat1xN and matNx1 where N > 1.
  else if (isVectorType(type, &elemType, &elemCount)) {
    for (uint32_t i = 0; i < elemCount; ++i) {
      auto *element =
          spvBuilder.createCompositeExtract(elemType, inst, {i}, loc);
      scalars.emplace_back(element, elemType);
    }
  }
  // MxN matrix cases, where M > 1 and N > 1.
  else if (isMxNMatrix(type, &elemType, &rowCount, &colCount)) {
    for (uint32_t i = 0; i < rowCount; ++i)
      for (uint32_t j = 0; j < colCount; ++j) {
        auto *element =
            spvBuilder.createCompositeExtract(elemType, inst, {i, j}, loc);
        scalars.emplace_back(element, elemType);
      }
  }
  // The decompose method only supports scalar, vector, and matrix types.
  else {
    llvm_unreachable(
        "decompose() should only handle scalar or vector or matrix types");
  }
}

bool InitListHandler::tryToSplitStruct() {
  if (initializers.empty())
    return false;

  auto *init = initializers.back();
  const QualType initType = init->getAstResultType();
  if (!initType->isStructureType() ||
      // Sampler types will pass the above check but we cannot split it.
      isSampler(initType))
    return false;

  // We are certain the current intializer will be replaced by now.
  initializers.pop_back();
  const auto &loc = init->getSourceLocation();

  const auto *structDecl = initType->getAsStructureType()->getDecl();

  // Create MemberExpr for each field of the struct
  llvm::SmallVector<SpirvInstruction *, 4> fields;
  uint32_t i = 0;
  for (auto *field : structDecl->fields()) {
    auto *extract =
        spvBuilder.createCompositeExtract(field->getType(), init, {i}, loc);
    fields.push_back(extract);
    ++i;
  }

  // Push in the reverse order
  initializers.insert(initializers.end(), fields.rbegin(), fields.rend());

  return true;
}

bool InitListHandler::tryToSplitConstantArray() {
  if (initializers.empty())
    return false;

  auto *init = initializers.back();
  const QualType initType = init->getAstResultType();
  if (!initType->isConstantArrayType())
    return false;

  // We are certain the current intializer will be replaced by now.
  initializers.pop_back();
  const auto &loc = init->getSourceLocation();

  const auto &context = theEmitter.getASTContext();

  const auto *arrayType = context.getAsConstantArrayType(initType);
  const auto elemType = arrayType->getElementType();
  // TODO: handle (unlikely) extra large array size?
  const auto size = static_cast<uint32_t>(arrayType->getSize().getZExtValue());

  // Create ArraySubscriptExpr for each element of the array
  // TODO: It will generate lots of elements if the array size is very large.
  // But do we have a better solution?
  llvm::SmallVector<SpirvInstruction *, 4> elements;
  for (uint32_t i = 0; i < size; ++i) {
    auto *extract = spvBuilder.createCompositeExtract(elemType, init, {i}, loc);
    elements.push_back(extract);
  }

  // Push in the reverse order
  initializers.insert(initializers.end(), elements.rbegin(), elements.rend());

  return true;
}

SpirvInstruction *InitListHandler::createInitForType(QualType type,
                                                     SourceLocation srcLoc) {
  type = type.getCanonicalType();

  if (type->isBuiltinType())
    return createInitForBuiltinType(type, srcLoc);

  QualType elemType = {};
  uint32_t elemCount = 0;
  if (isVectorType(type, &elemType, &elemCount))
    return createInitForVectorType(elemType, elemCount, srcLoc);

  // The purpose of this check is for vectors of size 1 (for which isVectorType
  // is false).
  if (isScalarType(type, &elemType))
    return createInitForVectorType(elemType, 1, srcLoc);

  if (hlsl::IsHLSLMatType(type)) {
    return createInitForMatrixType(type, srcLoc);
  }

  // Samplers, (RW)Buffers, (RW)Textures
  // It is important that this happens before checking of structure types.
  if (isOpaqueType(type))
    return createInitForSamplerImageType(type, srcLoc);

  // This should happen before the check for normal struct types
  if (isAKindOfStructuredOrByteBuffer(type)) {
    emitError("cannot handle structured/byte buffer as initializer", srcLoc);
    return nullptr;
  }

  if (type->isStructureType())
    return createInitForStructType(type, srcLoc);

  if (type->isConstantArrayType())
    return createInitForConstantArrayType(type, srcLoc);

  emitError("initializer for type %0 unimplemented", srcLoc) << type;
  return nullptr;
}

SpirvInstruction *
InitListHandler::createInitForBuiltinType(QualType type,
                                          SourceLocation srcLoc) {
  assert(type->isBuiltinType());

  if (!scalars.empty()) {
    const auto init = scalars.front();
    scalars.pop_front();
    return theEmitter.castToType(init.first, init.second, type, srcLoc);
  }

  // Keep splitting structs or arrays
  while (tryToSplitStruct() || tryToSplitConstantArray())
    ;

  auto init = initializers.back();
  initializers.pop_back();

  if (!init->getAstResultType()->isBuiltinType()) {
    decompose(init);
    return createInitForBuiltinType(type, srcLoc);
  }

  return theEmitter.castToType(init, init->getAstResultType(), type, srcLoc);
}

SpirvInstruction *
InitListHandler::createInitForVectorType(QualType elemType, uint32_t count,
                                         SourceLocation srcLoc) {
  // If we don't have leftover scalars, we can try to see if there is a vector
  // of the same size in the original initializer list so that we can use it
  // directly. For all other cases, we need to construct a new vector as the
  // initializer.
  if (scalars.empty()) {
    // Keep splitting structs or arrays
    while (tryToSplitStruct() || tryToSplitConstantArray())
      ;

    auto init = initializers.back();
    const auto initType = init->getAstResultType();

    uint32_t elemCount = 0;
    if (isVectorType(initType, nullptr, &elemCount) && elemCount == count) {
      initializers.pop_back();
      /// HLSL vector types are parameterized templates and we cannot
      /// construct them. So we construct an ExtVectorType here instead.
      /// This is unfortunate since it means we need to handle ExtVectorType
      /// in all type casting methods in SpirvEmitter.
      const auto toVecType =
          theEmitter.getASTContext().getExtVectorType(elemType, count);
      return theEmitter.castToType(init, initType, toVecType, srcLoc);
    }
  }

  if (count == 1)
    return createInitForBuiltinType(elemType, srcLoc);

  llvm::SmallVector<SpirvInstruction *, 4> elements;
  for (uint32_t i = 0; i < count; ++i) {
    // All elements are scalars, which should already be casted to the correct
    // type if necessary.
    elements.push_back(createInitForBuiltinType(elemType, srcLoc));
  }

  const QualType vecType = astContext.getExtVectorType(elemType, count);

  // TODO: use OpConstantComposite when all components are constants
  return spvBuilder.createCompositeConstruct(vecType, elements, srcLoc);
}

SpirvInstruction *
InitListHandler::createInitForMatrixType(QualType matrixType,
                                         SourceLocation srcLoc) {
  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(matrixType, rowCount, colCount);
  const QualType elemType = hlsl::GetHLSLMatElementType(matrixType);

  // Same as the vector case, first try to see if we already have a matrix at
  // the beginning of the initializer queue.
  if (scalars.empty()) {
    // Keep splitting structs or arrays
    while (tryToSplitStruct() || tryToSplitConstantArray())
      ;

    auto init = initializers.back();

    if (hlsl::IsHLSLMatType(init->getAstResultType())) {
      uint32_t initRowCount = 0, initColCount = 0;
      hlsl::GetHLSLMatRowColCount(init->getAstResultType(), initRowCount,
                                  initColCount);
      if (rowCount == initRowCount && colCount == initColCount) {
        initializers.pop_back();
        return theEmitter.castToType(init, init->getAstResultType(), matrixType,
                                     srcLoc);
      }
    }
  }

  if (rowCount == 1)
    return createInitForVectorType(elemType, colCount, srcLoc);
  if (colCount == 1)
    return createInitForVectorType(elemType, rowCount, srcLoc);

  llvm::SmallVector<SpirvInstruction *, 4> vectors;
  for (uint32_t i = 0; i < rowCount; ++i) {
    // All elements are vectors, which should already be casted to the correct
    // type if necessary.
    vectors.push_back(createInitForVectorType(elemType, colCount, srcLoc));
  }

  // TODO: use OpConstantComposite when all components are constants
  return spvBuilder.createCompositeConstruct(matrixType, vectors, srcLoc);
}

SpirvInstruction *
InitListHandler::createInitForStructType(QualType type, SourceLocation srcLoc) {
  assert(type->isStructureType() && !isSampler(type));

  // Same as the vector case, first try to see if we already have a struct at
  // the beginning of the initializer queue.
  if (scalars.empty()) {
    // Keep splitting arrays
    while (tryToSplitConstantArray())
      ;

    auto init = initializers.back();
    // We can only avoid decomposing and reconstructing when the type is
    // exactly the same.
    if (type.getCanonicalType() ==
        init->getAstResultType().getCanonicalType()) {
      initializers.pop_back();
      return init;
    }

    // Otherwise, if the next initializer is a struct, it is not of the same
    // type as we expected. Split it. Just need to do one iteration since a
    // field in the next struct initializer may be of the same struct type as
    // a field we are about the construct.
    tryToSplitStruct();
  }

  llvm::SmallVector<SpirvInstruction *, 4> fields;
  const RecordDecl *structDecl = type->getAsStructureType()->getDecl();
  for (const auto *field : structDecl->fields()) {
    fields.push_back(createInitForType(field->getType(), field->getLocation()));
    if (!fields.back())
      return nullptr;
  }

  // TODO: use OpConstantComposite when all components are constants
  return spvBuilder.createCompositeConstruct(type, fields, srcLoc);
}

SpirvInstruction *
InitListHandler::createInitForConstantArrayType(QualType type,
                                                SourceLocation srcLoc) {
  assert(type->isConstantArrayType());

  // Same as the vector case, first try to see if we already have an array at
  // the beginning of the initializer queue.
  if (scalars.empty()) {
    // Keep splitting structs
    while (tryToSplitStruct())
      ;

    auto init = initializers.back();
    // We can only avoid decomposing and reconstructing when the type is
    // exactly the same.
    if (type.getCanonicalType() ==
        init->getAstResultType().getCanonicalType()) {
      initializers.pop_back();
      return init;
    }

    // Otherwise, if the next initializer is an array, it is not of the same
    // type as we expected. Split it. Just need to do one iteration since the
    // next array initializer may have the same element type as the one we
    // are about to construct but with different size.
    tryToSplitConstantArray();
  }

  const auto *arrType = theEmitter.getASTContext().getAsConstantArrayType(type);
  const auto elemType = arrType->getElementType();
  // TODO: handle (unlikely) extra large array size?
  const auto size = static_cast<uint32_t>(arrType->getSize().getZExtValue());

  llvm::SmallVector<SpirvInstruction *, 4> elements;
  for (uint32_t i = 0; i < size; ++i)
    elements.push_back(createInitForType(elemType, srcLoc));

  // TODO: use OpConstantComposite when all components are constants
  return spvBuilder.createCompositeConstruct(type, elements, srcLoc);
}

SpirvInstruction *
InitListHandler::createInitForSamplerImageType(QualType type,
                                               SourceLocation srcLoc) {
  assert(isOpaqueType(type));

  // Samplers, (RW)Buffers, and (RW)Textures are translated into OpTypeSampler
  // and OpTypeImage. They should be treated similar as builtin types.

  if (!scalars.empty()) {
    const auto init = scalars.front();
    scalars.pop_front();
    // Require exact type match between the initializer and the target component
    if (init.second.getCanonicalType() != type.getCanonicalType()) {
      emitError("cannot cast initializer type %0 into variable type %1", srcLoc)
          << init.second << type;
      return nullptr;
    }
    return init.first;
  }

  // Keep splitting structs or arrays
  while (tryToSplitStruct() || tryToSplitConstantArray())
    ;

  auto init = initializers.back();
  initializers.pop_back();

  if (init->getAstResultType().getCanonicalType() != type.getCanonicalType()) {
    emitError("Cannot cast initializer type %0 into variable type %1", srcLoc)
        << init->getAstResultType() << type;
    return nullptr;
  }
  return init;
}

} // end namespace spirv
} // end namespace clang
