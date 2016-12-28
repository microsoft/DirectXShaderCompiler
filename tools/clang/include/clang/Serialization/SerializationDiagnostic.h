//===--- SerializationDiagnostic.h - Serialization Diagnostics -*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SerializationDiagnostic.h                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_SERIALIZATION_SERIALIZATIONDIAGNOSTIC_H
#define LLVM_CLANG_SERIALIZATION_SERIALIZATIONDIAGNOSTIC_H

#include "clang/Basic/Diagnostic.h"

namespace clang {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,NOWERROR,SHOWINSYSHEADER,CATEGORY) ENUM,
#define SERIALIZATIONSTART
#include "clang/Basic/DiagnosticSerializationKinds.inc"
#undef DIAG
      NUM_BUILTIN_SERIALIZATION_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace clang

#endif
