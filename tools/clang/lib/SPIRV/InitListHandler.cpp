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

#include <algorithm>
#include <iterator>

#include "llvm/ADT/SmallVector.h"

namespace clang {
namespace spirv {

InitListHandler::InitListHandler(SPIRVEmitter &emitter)
    : theEmitter(emitter), theBuilder(emitter.getModuleBuilder()),
      typeTranslator(emitter.getTypeTranslator()),
      diags(emitter.getDiagnosticsEngine()) {}

uint32_t InitListHandler::processInit(const InitListExpr *expr) {
  initializers.clear();
  scalars.clear();

  flatten(expr);
  // Reverse the whole initializer list so we can manipulate the list at the
  // tail of the vector. This is more efficient than using a deque.
  std::reverse(std::begin(initializers), std::end(initializers));

  return doProcess(expr->getType(), expr->getExprLoc());
}

uint32_t InitListHandler::processCast(QualType toType, const Expr *expr) {
  initializers.clear();
  scalars.clear();

  initializers.push_back(expr);

  return doProcess(toType, expr->getExprLoc());
}

uint32_t InitListHandler::doProcess(QualType type, SourceLocation srcLoc) {
  const uint32_t init = createInitForType(type, srcLoc);

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
      initializers.push_back(init);
    }
  }
}

void InitListHandler::decompose(const Expr *expr) {
  const QualType type = expr->getType();

  if (hlsl::IsHLSLVecType(type)) {
    const uint32_t vec = theEmitter.loadIfGLValue(expr);
    const QualType elemType = hlsl::GetHLSLVecElementType(type);
    const auto size = hlsl::GetHLSLVecSize(type);

    decomposeVector(vec, elemType, size);
  } else if (hlsl::IsHLSLMatType(type)) {
    const uint32_t mat = theEmitter.loadIfGLValue(expr);
    const QualType elemType = hlsl::GetHLSLMatElementType(type);

    uint32_t rowCount = 0, colCount = 0;
    hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

    if (rowCount == 1 || colCount == 1) {
      // This also handles the scalar case
      decomposeVector(mat, elemType, rowCount == 1 ? colCount : rowCount);
    } else {
      const uint32_t elemTypeId = typeTranslator.translateType(elemType);
      for (uint32_t i = 0; i < rowCount; ++i)
        for (uint32_t j = 0; j < colCount; ++j) {
          const uint32_t element =
              theBuilder.createCompositeExtract(elemTypeId, mat, {i, j});
          scalars.emplace_back(element, elemType);
        }
    }
  } else {
    llvm_unreachable("decompose() should only handle vector or matrix types");
  }
}

void InitListHandler::decomposeVector(uint32_t vec, QualType elemType,
                                      uint32_t size) {
  if (size == 1) {
    // Decomposing of size-1 vector just results in the vector itself.
    scalars.emplace_back(vec, elemType);
  } else {
    const uint32_t elemTypeId = typeTranslator.translateType(elemType);
    for (uint32_t i = 0; i < size; ++i) {
      const uint32_t element =
          theBuilder.createCompositeExtract(elemTypeId, vec, {i});
      scalars.emplace_back(element, elemType);
    }
  }
}

bool InitListHandler::tryToSplitStruct() {
  if (initializers.empty())
    return false;

  auto *init = const_cast<Expr *>(initializers.back());
  const QualType initType = init->getType();
  if (!initType->isStructureType() ||
      // Sampler types will pass the above check but we cannot split it.
      TypeTranslator::isSampler(initType))
    return false;

  // We are certain the current intializer will be replaced by now.
  initializers.pop_back();

  const auto &context = theEmitter.getASTContext();
  const auto *structDecl = initType->getAsStructureType()->getDecl();

  // Create MemberExpr for each field of the struct
  llvm::SmallVector<const Expr *, 4> fields;
  for (auto *field : structDecl->fields()) {
    fields.push_back(MemberExpr::Create(
        context, init, /*isarraw*/ false, /*OperatorLoc*/ {},
        /*QualifierLoc*/ {}, /*TemplateKWLoc*/ {}, field,
        DeclAccessPair::make(field, AS_none),
        DeclarationNameInfo(field->getDeclName(), /*NameLoc*/ {}),
        /*TemplateArgumentListInfo*/ nullptr, field->getType(),
        init->getValueKind(), OK_Ordinary));
  }

  // Push in the reverse order
  initializers.insert(initializers.end(), fields.rbegin(), fields.rend());

  return true;
}

bool InitListHandler::tryToSplitConstantArray() {
  if (initializers.empty())
    return false;

  auto *init = const_cast<Expr *>(initializers.back());
  const QualType initType = init->getType();
  if (!initType->isConstantArrayType())
    return false;

  // We are certain the current intializer will be replaced by now.
  initializers.pop_back();

  const auto &context = theEmitter.getASTContext();
  const auto u32Type = context.getIntTypeForBitwidth(32, /*sigined*/ 0);

  const auto *arrayType = context.getAsConstantArrayType(initType);
  const auto elemType = arrayType->getElementType();
  // TODO: handle (unlikely) extra large array size?
  const auto size = static_cast<uint32_t>(arrayType->getSize().getZExtValue());

  // Create ArraySubscriptExpr for each element of the array
  // TODO: It will generate lots of elements if the array size is very large.
  // But do we have a better solution?
  llvm::SmallVector<const Expr *, 4> elements;
  for (uint32_t i = 0; i < size; ++i) {
    const auto iVal =
        llvm::APInt(/*numBits*/ 32, uint64_t(i), /*isSigned*/ false);
    auto *index = IntegerLiteral::Create(context, iVal, u32Type, {});
    const auto *element = new (context)
        ArraySubscriptExpr(init, index, elemType, VK_LValue, OK_Ordinary, {});
    elements.push_back(element);
  }

  // Push in the reverse order
  initializers.insert(initializers.end(), elements.rbegin(), elements.rend());

  return true;
}

uint32_t InitListHandler::createInitForType(QualType type,
                                            SourceLocation srcLoc) {
  type = type.getCanonicalType();

  if (type->isBuiltinType())
    return createInitForBuiltinType(type, srcLoc);

  if (hlsl::IsHLSLVecType(type))
    return createInitForVectorType(hlsl::GetHLSLVecElementType(type),
                                   hlsl::GetHLSLVecSize(type), srcLoc);

  if (hlsl::IsHLSLMatType(type)) {
    return createInitForMatrixType(type, srcLoc);
  }

  // Samplers, (RW)Buffers, (RW)Textures
  // It is important that this happens before checking of structure types.
  if (TypeTranslator::isOpaqueType(type))
    return createInitForSamplerImageType(type, srcLoc);

  // This should happen before the check for normal struct types
  if (TypeTranslator::isAKindOfStructuredOrByteBuffer(type)) {
    emitError("cannot handle structured/byte buffer as initializer", srcLoc);
    return 0;
  }

  if (type->isStructureType())
    return createInitForStructType(type);

  if (type->isConstantArrayType())
    return createInitForConstantArrayType(type, srcLoc);

  emitError("initializer for type %0 unimplemented", srcLoc) << type;
  return 0;
}

uint32_t InitListHandler::createInitForBuiltinType(QualType type,
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

  const Expr *init = initializers.back();
  initializers.pop_back();

  if (!init->getType()->isBuiltinType()) {
    decompose(init);
    return createInitForBuiltinType(type, srcLoc);
  }

  const uint32_t value = theEmitter.loadIfGLValue(init);
  return theEmitter.castToType(value, init->getType(), type, srcLoc);
}

uint32_t InitListHandler::createInitForVectorType(QualType elemType,
                                                  uint32_t count,
                                                  SourceLocation srcLoc) {
  // If we don't have leftover scalars, we can try to see if there is a vector
  // of the same size in the original initializer list so that we can use it
  // directly. For all other cases, we need to construct a new vector as the
  // initializer.
  if (scalars.empty()) {
    // Keep splitting structs or arrays
    while (tryToSplitStruct() || tryToSplitConstantArray())
      ;

    const Expr *init = initializers.back();

    if (hlsl::IsHLSLVecType(init->getType()) &&
        hlsl::GetHLSLVecSize(init->getType()) == count) {
      initializers.pop_back();
      /// HLSL vector types are parameterized templates and we cannot
      /// construct them. So we construct an ExtVectorType here instead.
      /// This is unfortunate since it means we need to handle ExtVectorType
      /// in all type casting methods in SPIRVEmitter.
      const auto toVecType =
          theEmitter.getASTContext().getExtVectorType(elemType, count);
      return theEmitter.castToType(theEmitter.loadIfGLValue(init),
                                   init->getType(), toVecType, srcLoc);
    }
  }

  if (count == 1)
    return createInitForBuiltinType(elemType, srcLoc);

  llvm::SmallVector<uint32_t, 4> elements;
  for (uint32_t i = 0; i < count; ++i) {
    // All elements are scalars, which should already be casted to the correct
    // type if necessary.
    elements.push_back(createInitForBuiltinType(elemType, srcLoc));
  }

  const uint32_t elemTypeId = typeTranslator.translateType(elemType);
  const uint32_t vecType = theBuilder.getVecType(elemTypeId, count);

  // TODO: use OpConstantComposite when all components are constants
  return theBuilder.createCompositeConstruct(vecType, elements);
}

uint32_t InitListHandler::createInitForMatrixType(QualType matrixType,
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

    const Expr *init = initializers.back();

    if (hlsl::IsHLSLMatType(init->getType())) {
      uint32_t initRowCount = 0, initColCount = 0;
      hlsl::GetHLSLMatRowColCount(init->getType(), initRowCount, initColCount);

      if (rowCount == initRowCount && colCount == initColCount) {
        initializers.pop_back();
        // TODO: We only support FP matrices now. Do type cast here after
        // adding more matrix types.
        return theEmitter.loadIfGLValue(init);
      }
    }
  }

  if (rowCount == 1)
    return createInitForVectorType(elemType, colCount, srcLoc);
  if (colCount == 1)
    return createInitForVectorType(elemType, rowCount, srcLoc);

  llvm::SmallVector<uint32_t, 4> vectors;
  for (uint32_t i = 0; i < rowCount; ++i) {
    // All elements are vectors, which should already be casted to the correct
    // type if necessary.
    vectors.push_back(createInitForVectorType(elemType, colCount, srcLoc));
  }

  // TODO: use OpConstantComposite when all components are constants
  return theBuilder.createCompositeConstruct(
      typeTranslator.translateType(matrixType), vectors);
}

uint32_t InitListHandler::createInitForStructType(QualType type) {
  assert(type->isStructureType() && !TypeTranslator::isSampler(type));

  // Same as the vector case, first try to see if we already have a struct at
  // the beginning of the initializer queue.
  if (scalars.empty()) {
    // Keep splitting arrays
    while (tryToSplitConstantArray())
      ;

    const Expr *init = initializers.back();
    // We can only avoid decomposing and reconstructing when the type is
    // exactly the same.
    if (type.getCanonicalType() == init->getType().getCanonicalType()) {
      initializers.pop_back();
      return theEmitter.loadIfGLValue(init);
    }

    // Otherwise, if the next initializer is a struct, it is not of the same
    // type as we expected. Split it. Just need to do one iteration since a
    // field in the next struct initializer may be of the same struct type as
    // a field we are about the construct.
    tryToSplitStruct();
  }

  llvm::SmallVector<uint32_t, 4> fields;
  const RecordDecl *structDecl = type->getAsStructureType()->getDecl();
  for (const auto *field : structDecl->fields()) {
    fields.push_back(createInitForType(field->getType(), field->getLocation()));
    if (!fields.back())
      return 0;
  }

  const uint32_t typeId = typeTranslator.translateType(type);
  // TODO: use OpConstantComposite when all components are constants
  return theBuilder.createCompositeConstruct(typeId, fields);
}

uint32_t
InitListHandler::createInitForConstantArrayType(QualType type,
                                                SourceLocation srcLoc) {
  assert(type->isConstantArrayType());

  // Same as the vector case, first try to see if we already have an array at
  // the beginning of the initializer queue.
  if (scalars.empty()) {
    // Keep splitting structs
    while (tryToSplitStruct())
      ;

    const Expr *init = initializers.back();
    // We can only avoid decomposing and reconstructing when the type is
    // exactly the same.
    if (type.getCanonicalType() == init->getType().getCanonicalType()) {
      initializers.pop_back();
      return theEmitter.loadIfGLValue(init);
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

  llvm::SmallVector<uint32_t, 4> elements;
  for (uint32_t i = 0; i < size; ++i)
    elements.push_back(createInitForType(elemType, srcLoc));

  const uint32_t typeId = typeTranslator.translateType(type);
  // TODO: use OpConstantComposite when all components are constants
  return theBuilder.createCompositeConstruct(typeId, elements);
}

uint32_t InitListHandler::createInitForSamplerImageType(QualType type,
                                                        SourceLocation srcLoc) {
  assert(TypeTranslator::isOpaqueType(type));

  // Samplers, (RW)Buffers, and (RW)Textures are translated into OpTypeSampler
  // and OpTypeImage. They should be treated similar as builtin types.

  if (!scalars.empty()) {
    const auto init = scalars.front();
    scalars.pop_front();
    // Require exact type match between the initializer and the target component
    if (init.second.getCanonicalType() != type.getCanonicalType()) {
      emitError("cannot cast initializer type %0 into variable type %1", srcLoc)
          << init.second << type;
      return 0;
    }
    return init.first;
  }

  // Keep splitting structs or arrays
  while (tryToSplitStruct() || tryToSplitConstantArray())
    ;

  const Expr *init = initializers.back();
  initializers.pop_back();

  if (init->getType().getCanonicalType() != type.getCanonicalType()) {
    init->dump();
    emitError("cannot cast initializer type %0 into variable type %1",
              init->getLocStart())
        << init->getType() << type;
    return 0;
  }
  return theEmitter.loadIfGLValue(init);
}

} // end namespace spirv
} // end namespace clang
