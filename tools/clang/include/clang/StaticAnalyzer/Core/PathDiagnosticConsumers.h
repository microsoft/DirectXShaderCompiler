//===--- PathDiagnosticClients.h - Path Diagnostic Clients ------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PathDiagnosticConsumers.h                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defines the interface to create different path diagostic clients.//
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_STATICANALYZER_CORE_PATHDIAGNOSTICCONSUMERS_H
#define LLVM_CLANG_STATICANALYZER_CORE_PATHDIAGNOSTICCONSUMERS_H

#include <string>
#include <vector>

namespace clang {

class AnalyzerOptions;
class Preprocessor;

namespace ento {

class PathDiagnosticConsumer;
typedef std::vector<PathDiagnosticConsumer*> PathDiagnosticConsumers;

#define ANALYSIS_DIAGNOSTICS(NAME, CMDFLAG, DESC, CREATEFN)\
void CREATEFN(AnalyzerOptions &AnalyzerOpts,\
              PathDiagnosticConsumers &C,\
              const std::string &Prefix,\
              const Preprocessor &PP);
#include "clang/StaticAnalyzer/Core/Analyses.def"

} // end 'ento' namespace
} // end 'clang' namespace

#endif
