//== DynamicTypeInfo.h - Runtime type information ----------------*- C++ -*--=//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DynamicTypeInfo.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#ifndef LLVM_CLANG_STATICANALYZER_CORE_PATHSENSITIVE_DYNAMICTYPEINFO_H
#define LLVM_CLANG_STATICANALYZER_CORE_PATHSENSITIVE_DYNAMICTYPEINFO_H

#include "clang/AST/Type.h"

namespace clang {
namespace ento {

/// \brief Stores the currently inferred strictest bound on the runtime type
/// of a region in a given state along the analysis path.
class DynamicTypeInfo {
private:
  QualType T;
  bool CanBeASubClass;

public:

  DynamicTypeInfo() : T(QualType()) {}
  DynamicTypeInfo(QualType WithType, bool CanBeSub = true)
    : T(WithType), CanBeASubClass(CanBeSub) {}

  /// \brief Return false if no dynamic type info is available.
  bool isValid() const { return !T.isNull(); }

  /// \brief Returns the currently inferred upper bound on the runtime type.
  QualType getType() const { return T; }

  /// \brief Returns false if the type information is precise (the type T is
  /// the only type in the lattice), true otherwise.
  bool canBeASubClass() const { return CanBeASubClass; }

  void Profile(llvm::FoldingSetNodeID &ID) const {
    ID.Add(T);
    ID.AddInteger((unsigned)CanBeASubClass);
  }
  bool operator==(const DynamicTypeInfo &X) const {
    return T == X.T && CanBeASubClass == X.CanBeASubClass;
  }
};

} // end ento
} // end clang

#endif
