//===--- TypeTranslator.cpp - TypeTranslator implementation ------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/TypeTranslator.h"
#include "clang/AST/HlslTypes.h"

namespace clang {
namespace spirv {

uint32_t TypeTranslator::translateType(QualType type) {
  const auto *typePtr = type.getTypePtr();

  // Primitive types
  if (const auto *builtinType = dyn_cast<BuiltinType>(typePtr)) {
    switch (builtinType->getKind()) {
    case BuiltinType::Void:
      return theBuilder.getVoidType();
    case BuiltinType::Float:
      return theBuilder.getFloatType();
    default:
      emitError("Primitive type '%0' is not supported yet.")
          << builtinType->getTypeClassName();
      return 0;
    }
  }

  // In AST, vector types are TypedefType of TemplateSpecializationType.
  // We handle them via HLSL type inspection functions.
  if (hlsl::IsHLSLVecType(type)) {
    const auto elemType = hlsl::GetHLSLVecElementType(type);
    const auto elemCount = hlsl::GetHLSLVecSize(type);
    return theBuilder.getVecType(translateType(elemType), elemCount);
  }

  // Struct type
  if (const auto *structType = dyn_cast<RecordType>(typePtr)) {
    const auto *decl = structType->getDecl();

    // Collect all fields' types.
    std::vector<uint32_t> fieldTypes;
    for (const auto *field : decl->fields()) {
      fieldTypes.push_back(translateType(field->getType()));
    }

    return theBuilder.getStructType(fieldTypes);
  }

  emitError("Type '%0' is not supported yet.") << type->getTypeClassName();
  return 0;
}

} // end namespace spirv
} // end namespace clang