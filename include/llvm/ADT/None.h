//===-- None.h - Simple null value for implicit construction ------*- C++ -*-=//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// None.h                                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file provides None, an enumerator for use in implicit constructors  //
//  of various (usually templated) types to make such construction more      //
//  terse.                                                                   //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_ADT_NONE_H
#define LLVM_ADT_NONE_H

namespace llvm {
/// \brief A simple null object to allow implicit construction of Optional<T>
/// and similar types without having to spell out the specialization's name.
enum class NoneType { None };
const NoneType None = None;
}

#endif
