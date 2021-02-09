///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSemantic.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Representation of HLSL parameter semantics.                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Type.h"

#include "DxilConstants.h"
#include "DxilShaderModel.h"

namespace hlsl {

/// Use this class to represent HLSL parameter semantic.
class Semantic {
public:
  using Kind = DXIL::SemanticKind;

  enum class CompTy {
    BoolTy         = 1 << 0,
    HalfTy         = 1 << 1,
    Int16Ty        = 1 << 2,
    FloatTy        = 1 << 3,
    Int32Ty        = 1 << 4,
    DoubleTy       = 1 << 5,
    Int64Ty        = 1 << 6,
    HalfOrFloatTy  = HalfTy | FloatTy,
    BoolOrInt32Ty  = BoolTy | Int32Ty,
    FloatOrInt32Ty = FloatTy | Int32Ty,
    Int16Or32Ty    = Int16Ty | Int32Ty,
    AnyIntTy       = BoolTy | Int16Ty | Int32Ty | Int64Ty,
    AnyFloatTy     = HalfTy | FloatTy | DoubleTy,
    AnyTy          = AnyIntTy | AnyFloatTy,
  };

  enum class SizeClass {
    Scalar,
    Vec2,
    Vec3,
    Vec4,
    Other,
    Unknown
  };

  static const int kUndefinedRow = -1;
  static const int kUndefinedCol = -1;

  static const Semantic *GetByName(llvm::StringRef name);
  static const Semantic *GetByName(llvm::StringRef Name, DXIL::SigPointKind sigPointKind,
    unsigned MajorVersion = ShaderModel::kHighestMajor, unsigned MinorVersion = ShaderModel::kHighestMinor);
  static const Semantic *Get(Kind kind);
  static const Semantic *Get(Kind kind, DXIL::SigPointKind sigPointKind,
    unsigned MajorVersion = ShaderModel::kHighestMajor, unsigned MinorVersion = ShaderModel::kHighestMinor);
  static const Semantic *GetInvalid();
  static const Semantic *GetArbitrary();
  static bool HasSVPrefix(llvm::StringRef Name);
  static void DecomposeNameAndIndex(llvm::StringRef FullName, llvm::StringRef *pName, unsigned *pIndex);

  Kind GetKind() const;
  const char *GetName() const;
  bool IsArbitrary() const;
  bool IsInvalid() const;
  bool IsSupportedType(llvm::Type *semTy) const;
  CompTy Semantic::GetCompType(llvm::Type* ty) const;
  SizeClass Semantic::GetCompCount(llvm::Type* ty) const;

private:
  Kind m_Kind;                   // Semantic kind.
  const char *m_pszName;         // Canonical name (for system semantics).
  CompTy m_allowedTys;           // Types allowed for the semantic
  SizeClass m_minCompCount;      // Minimum component count that is allowed for a semantic
  SizeClass m_maxCompCount;      // Maximum component count that is allowed for a semantic

  Semantic() = delete;
  Semantic(Kind Kind, const char *pszName, CompTy allowedTys, SizeClass minCompCount, SizeClass maxCompCount);

  // Table of all semantic properties.
  static const unsigned kNumSemanticRecords = (unsigned)Kind::Invalid + 1;
  static const Semantic ms_SemanticTable[kNumSemanticRecords];

  friend class ShaderModel;
  friend class SignatureElement;
};

} // namespace hlsl
