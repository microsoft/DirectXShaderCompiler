///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilCBuffer.h                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Representation of HLSL constant buffer (cbuffer).                         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/HLSL/DxilResourceBase.h"


namespace hlsl {

/// Use this class to represent HLSL cbuffer.
class DxilCBuffer : public DxilResourceBase {
public:
  DxilCBuffer();
  virtual ~DxilCBuffer();

  unsigned GetSize() const;

  void SetSize(unsigned InstanceSizeInBytes);

private:
  unsigned m_SizeInBytes;   // Cbuffer instance size in bytes.
};

} // namespace hlsl
