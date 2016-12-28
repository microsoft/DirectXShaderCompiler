///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilCBuffer.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilCBuffer.h"
#include "dxc/Support/Global.h"


namespace hlsl {

//------------------------------------------------------------------------------
//
// DxilCBuffer methods.
//
DxilCBuffer::DxilCBuffer()
: DxilResourceBase(DxilResourceBase::Class::CBuffer)
, m_SizeInBytes(0) {
  SetKind(DxilResourceBase::Kind::CBuffer);
}

DxilCBuffer::~DxilCBuffer() {}

unsigned DxilCBuffer::GetSize() const { return m_SizeInBytes; }

void DxilCBuffer::SetSize(unsigned InstanceSizeInBytes) { m_SizeInBytes = InstanceSizeInBytes; }

} // namespace hlsl
