//===- ChainedDiagnosticConsumer.cpp - Chain Diagnostic Clients -----------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ChainedDiagnosticConsumer.cpp                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Frontend/ChainedDiagnosticConsumer.h"

using namespace clang;

void ChainedDiagnosticConsumer::anchor() { }
