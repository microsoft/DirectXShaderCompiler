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

uint32_t InitListHandler::process(const InitListExpr *expr) {
  initializers.clear();
  scalars.clear();

  flatten(expr);
  // Reverse the whole initializer list so we can manipulate the list at the
  // tail of the vector. This is more efficient than using a deque.
  std::reverse(std::begin(initializers), std::end(initializers));

  const uint32_t init = createInitForType(expr->getType());

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
  assert(!type->isBuiltinType()); // Cannot decompose builtin types

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
  } else if (type->isStructureType()) {
    llvm_unreachable("struct initializer should already been handled");
  } else {
    emitError("decomposing type %0 in initializer list unimplemented") << type;
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

void InitListHandler::tryToSplitStruct() {
  if (initializers.empty())
    return;

  auto *init = const_cast<Expr *>(initializers.back());
  const QualType initType = init->getType();
  if (!initType->isStructureType())
    return;

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
}

uint32_t InitListHandler::createInitForType(QualType type) {
  type = type.getCanonicalType();

  if (type->isBuiltinType())
    return createInitForBuiltinType(type);

  if (hlsl::IsHLSLVecType(type))
    return createInitForVectorType(hlsl::GetHLSLVecElementType(type),
                                   hlsl::GetHLSLVecSize(type));

  if (hlsl::IsHLSLMatType(type)) {
    uint32_t rowCount = 0, colCount = 0;
    hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);
    const QualType elemType = hlsl::GetHLSLMatElementType(type);

    return createInitForMatrixType(elemType, rowCount, colCount);
  }

  if (type->isStructureType())
    return createInitForStructType(type);

  emitError("unimplemented initializer for type '%0'") << type;
  return 0;
}

uint32_t InitListHandler::createInitForBuiltinType(QualType type) {
  assert(type->isBuiltinType());

  if (!scalars.empty()) {
    const auto init = scalars.front();
    scalars.pop_front();
    return theEmitter.castToType(init.first, init.second, type);
  }

  tryToSplitStruct();

  const Expr *init = initializers.back();
  initializers.pop_back();

  if (!init->getType()->isBuiltinType()) {
    decompose(init);
    return createInitForBuiltinType(type);
  }

  const uint32_t value = theEmitter.loadIfGLValue(init);
  return theEmitter.castToType(value, init->getType(), type);
}

uint32_t InitListHandler::createInitForVectorType(QualType elemType,
                                                  uint32_t count) {
  // If we don't have leftover scalars, we can try to see if there is a vector
  // of the same size in the original initializer list so that we can use it
  // directly. For all other cases, we need to construct a new vector as the
  // initializer.
  if (scalars.empty()) {
    // A struct may contain a whole vector.
    tryToSplitStruct();

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
                                   init->getType(), toVecType);
    }
  }

  if (count == 1)
    return createInitForBuiltinType(elemType);

  llvm::SmallVector<uint32_t, 4> elements;
  for (uint32_t i = 0; i < count; ++i) {
    // All elements are scalars, which should already be casted to the correct
    // type if necessary.
    elements.push_back(createInitForBuiltinType(elemType));
  }

  const uint32_t elemTypeId = typeTranslator.translateType(elemType);
  const uint32_t vecType = theBuilder.getVecType(elemTypeId, count);

  // TODO: use OpConstantComposite when all components are constants
  return theBuilder.createCompositeConstruct(vecType, elements);
}

uint32_t InitListHandler::createInitForMatrixType(QualType elemType,
                                                  uint32_t rowCount,
                                                  uint32_t colCount) {
  // Same as the vector case, first try to see if we already have a matrix at
  // the beginning of the initializer queue.
  if (scalars.empty()) {
    // A struct may contain a whole matrix.
    tryToSplitStruct();

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
    return createInitForVectorType(elemType, colCount);
  if (colCount == 1)
    return createInitForVectorType(elemType, rowCount);

  llvm::SmallVector<uint32_t, 4> vectors;
  for (uint32_t i = 0; i < rowCount; ++i) {
    // All elements are vectors, which should already be casted to the correct
    // type if necessary.
    vectors.push_back(createInitForVectorType(elemType, colCount));
  }

  const uint32_t elemTypeId = typeTranslator.translateType(elemType);
  const uint32_t vecType = theBuilder.getVecType(elemTypeId, colCount);
  const uint32_t matType = theBuilder.getMatType(vecType, rowCount);

  // TODO: use OpConstantComposite when all components are constants
  return theBuilder.createCompositeConstruct(matType, vectors);
}

uint32_t InitListHandler::createInitForStructType(QualType type) {
  // Same as the vector case, first try to see if we already have a struct at
  // the beginning of the initializer queue.
  if (scalars.empty()) {
    const Expr *init = initializers.back();
    // We can only avoid decomposing and reconstructing when the type is
    // exactly the same.
    if (type.getCanonicalType() == init->getType().getCanonicalType()) {
      initializers.pop_back();
      return theEmitter.loadIfGLValue(init);
    }

    // Otherwise, if the next initializer is a struct, it is not of the same
    // type as we expected. Split it.
    tryToSplitStruct();
  }

  llvm::SmallVector<uint32_t, 4> fields;
  const RecordDecl *structDecl = type->getAsStructureType()->getDecl();
  for (const auto *field : structDecl->fields())
    fields.push_back(createInitForType(field->getType()));

  const uint32_t typeId = typeTranslator.translateType(type);
  // TODO: use OpConstantComposite when all components are constants
  return theBuilder.createCompositeConstruct(typeId, fields);
}

} // end namespace spirv
} // end namespace clang
