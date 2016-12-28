//=--- CommonBugCategories.h - Provides common issue categories -*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CommonBugCategories.h                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_STATICANALYZER_CORE_BUGREPORTER_COMMONBUGCATEGORIES_H
#define LLVM_CLANG_STATICANALYZER_CORE_BUGREPORTER_COMMONBUGCATEGORIES_H

// Common strings used for the "category" of many static analyzer issues.
namespace clang {
  namespace ento {
    namespace categories {
      extern const char * const CoreFoundationObjectiveC;
      extern const char * const LogicError;
      extern const char * const MemoryCoreFoundationObjectiveC;
      extern const char * const UnixAPI;
    }
  }
}
#endif

