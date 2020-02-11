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
#include <float.h>
#include <strsafe.h>
#include <intsafe.h>
#include <dxgiformat.h>
#include <d3d12.h>
#define D3DX12_NO_STATE_OBJECT_HELPERS
#include "dxc/Support/d3dx12.h"
#include "D3D12TokenizedProgramFormat.hpp"
#include "ShaderBinary/ShaderBinary.h"

#define ASSUME( _exp ) { assert( _exp ); __analysis_assume( _exp ); __assume( _exp ); }
