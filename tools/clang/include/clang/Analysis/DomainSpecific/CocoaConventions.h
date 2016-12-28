//===- CocoaConventions.h - Special handling of Cocoa conventions -*- C++ -*--//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CocoaConventions.h                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements cocoa naming convention analysis.                    //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_ANALYSIS_DOMAINSPECIFIC_COCOACONVENTIONS_H
#define LLVM_CLANG_ANALYSIS_DOMAINSPECIFIC_COCOACONVENTIONS_H

#include "clang/Basic/LLVM.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
class FunctionDecl;
class QualType;
  
namespace ento {
namespace cocoa {
  
  bool isRefType(QualType RetTy, StringRef Prefix,
                 StringRef Name = StringRef());
    
  bool isCocoaObjectRef(QualType T);

}

namespace coreFoundation {
  bool isCFObjectRef(QualType T);
  
  bool followsCreateRule(const FunctionDecl *FD);
}

}} // end: "clang:ento"

#endif
