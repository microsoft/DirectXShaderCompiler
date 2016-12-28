//===- MCSchedule.cpp - Scheduling ------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCSchedule.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines the default scheduling model.                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/MC/MCSchedule.h"
#include <type_traits>

using namespace llvm;

static_assert(std::is_pod<MCSchedModel>::value,
              "We shouldn't have a static constructor here");
const MCSchedModel MCSchedModel::Default = {DefaultIssueWidth,
                                            DefaultMicroOpBufferSize,
                                            DefaultLoopMicroOpBufferSize,
                                            DefaultLoadLatency,
                                            DefaultHighLatency,
                                            DefaultMispredictPenalty,
                                            false,
                                            true,
                                            0,
                                            nullptr,
                                            nullptr,
                                            0,
                                            0,
                                            nullptr};
