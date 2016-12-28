//===--- OacrIgnoreCond.h - OACR directives ---------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// OacrIgnoreCond.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

// 
// In free builds, configuration options relating to compiler switches,
// most importantly languages, become constants, thereby removing
// codepaths and reducing disk footprint.
//
// OACR has a number of warnings however for these degenerate conditionals,
// which this file suppresses.

// OACR error 6235
#pragma prefast(disable: __WARNING_NONZEROLOGICALOR, "external project has dead branches for unsupported configuration combinations, by design")
// OACR error 6236
#pragma prefast(disable: __WARNING_LOGICALORNONZERO, "external project has dead branches for unsupported configuration combinations, by design")
// OACR error 6236
#pragma prefast(disable: __WARNING_ZEROLOGICALANDLOSINGSIDEEFFECTS, "external project has dead branches for unsupported configuration combinations, by design")
// OACR error 6285
#pragma prefast(disable: __WARNING_LOGICALOROFCONSTANTS, "external project has dead branches for unsupported configuration combinations, by design")
// OACR error 6286
#pragma prefast(disable: __WARNING_NONZEROLOGICALORLOSINGSIDEEFFECTS, "external project has dead branches for unsupported configuration combinations, by design")
// OACR error 6287
#pragma prefast(disable: __WARNING_REDUNDANTTEST, "external project has dead branches for unsupported configuration combinations, by design")

// local variable is initialized but not referenced - every LangOpts use on stack triggers this
#pragma warning(disable: 4189)
