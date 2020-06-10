///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLSLOptimizationOptions.h                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Option defined related to optimization customization.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace hlsl {
  // Optimizations that can be disabled
  struct OptimizationOptions {
    unsigned DisableGVN : 1;
    unsigned Reserved : 31;
  };
}

