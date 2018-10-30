///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilContainerAssembler.h                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Helpers for writing to dxil container.                                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include "dxc/DxilContainer/DxilContainer.h"

namespace hlsl {

class AbstractMemoryStream;
class DxilModule;
class RootSignatureHandle;
namespace DXIL {
enum class SignatureKind;
}

class DxilPartWriter {
public:
  virtual ~DxilPartWriter() {}
  virtual uint32_t size() const = 0;
  virtual void write(AbstractMemoryStream *pStream) = 0;
};

class DxilContainerWriter : public DxilPartWriter  {
public:
  typedef std::function<void(AbstractMemoryStream*)> WriteFn;
  virtual ~DxilContainerWriter() {}
  virtual void AddPart(uint32_t FourCC, uint32_t Size, WriteFn Write) = 0;
};

DxilPartWriter *NewProgramSignatureWriter(const DxilModule &M, DXIL::SignatureKind Kind);
DxilPartWriter *NewRootSignatureWriter(const RootSignatureHandle &S);
DxilPartWriter *NewFeatureInfoWriter(const DxilModule &M);
DxilPartWriter *NewPSVWriter(const DxilModule &M, uint32_t PSVVersion = 0);
DxilPartWriter *NewRDATWriter(const DxilModule &M, uint32_t InfoVersion = 0);

DxilContainerWriter *NewDxilContainerWriter();

void SerializeDxilContainerForModule(hlsl::DxilModule *pModule,
                                     AbstractMemoryStream *pModuleBitcode,
                                     AbstractMemoryStream *pStream,
                                     SerializeDxilFlags Flags);
void SerializeDxilContainerForRootSignature(hlsl::RootSignatureHandle *pRootSigHandle,
                                     AbstractMemoryStream *pStream);

} // namespace hlsl