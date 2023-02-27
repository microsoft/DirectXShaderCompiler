///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ShaderBinaryIncludes.cpp                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "windows.h"

#include <assert.h>
#include <d3d12.h>
#include <dxgiformat.h>
#include <float.h>
#include <intsafe.h>
#include <strsafe.h>
#define D3DX12_NO_STATE_OBJECT_HELPERS
#include "ShaderBinary/ShaderBinary.h"
#include "dxc/Support/D3D12TokenizedProgramFormat.hpp"
#include "dxc/Support/d3dx12.h"

#define ASSUME(_exp)                                                           \
  {                                                                            \
    assert(_exp);                                                              \
    __analysis_assume(_exp);                                                   \
    __assume(_exp);                                                            \
  }
