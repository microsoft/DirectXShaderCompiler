///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLSLOptions.h                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Option defined related to optimization customization.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef LLVM_HLSL_OPTIMIZATION_OPTIONS_H
#define LLVM_HLSL_OPTIMIZATION_OPTIONS_H

namespace hlsl {
  // Optimizations that can be disabled
  enum OptimizationToggles {
    OptToggleGvn = (1 << 0)
  };
}

#endif
