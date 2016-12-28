//===-- llvm/Support/AIXDataTypesFix.h - Fix datatype defs ------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// AIXDataTypesFix.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file overrides default system-defined types and limits which cannot be//
// done in DataTypes.h.in because it is processed by autoheader first, which //
// comments out any #undef statement                                         //
//
///////////////////////////////////////////////////////////////////////////////

// No include guards desired!

#ifndef SUPPORT_DATATYPES_H
#error "AIXDataTypesFix.h must only be included via DataTypes.h!"
#endif

// GCC is strict about defining large constants: they must have LL modifier.
// These will be defined properly at the end of DataTypes.h
#undef INT64_MAX
#undef INT64_MIN
