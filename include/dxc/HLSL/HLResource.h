///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLResource.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Representation of HLSL SRVs and UAVs in high-level DX IR.                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/HLSL/DxilResource.h"


namespace hlsl {

/// Use this class to represent an HLSL resource (SRV/UAV) in HLDXIR.
class HLResource : public DxilResource {
public:
  //QQQ
  // TODO: this does not belong here. QQQ
  //static Kind KeywordToKind(const std::string &keyword);
  
  HLResource();
};

} // namespace hlsl
