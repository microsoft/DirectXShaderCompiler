//===----- Attr.h - Enum values for C Attribute Kinds ----------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// AttrKinds.h                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///
/// \file                                                                    //
/// \brief Defines the clang::attr::Kind enum.                               //
///
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_BASIC_ATTRKINDS_H
#define LLVM_CLANG_BASIC_ATTRKINDS_H

namespace clang {

namespace attr {

// \brief A list of all the recognized kinds of attributes.
enum Kind {
#define ATTR(X) X,
#define LAST_INHERITABLE_ATTR(X) X, LAST_INHERITABLE = X,
#define LAST_INHERITABLE_PARAM_ATTR(X) X, LAST_INHERITABLE_PARAM = X,
#include "clang/Basic/AttrList.inc"
  NUM_ATTRS
};

} // end namespace attr
} // end namespace clang

#endif
