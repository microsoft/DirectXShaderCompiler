///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilRootSignature.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides support for manipulating root signature structures.              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/WinFunctions.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/dxcapi.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DiagnosticPrinter.h"

#include <string>
#include <algorithm>
#include <utility>
#include <vector>
#include <set>

#include "DxilRootSignatureHelper.h"

using namespace llvm;
using std::string;

namespace hlsl {

//////////////////////////////////////////////////////////////////////////////
// Root signature handler.

RootSignatureHandle::RootSignatureHandle(RootSignatureHandle&& other) {
  m_pDesc = nullptr;
  m_pSerialized = nullptr;
  std::swap(m_pDesc, other.m_pDesc);
  std::swap(m_pSerialized, other.m_pSerialized);
}

void RootSignatureHandle::Assign(const DxilVersionedRootSignatureDesc *pDesc,
                                 IDxcBlob *pSerialized) {
  Clear();
  m_pDesc = pDesc;
  m_pSerialized = pSerialized;
  if (m_pSerialized)
    m_pSerialized->AddRef();
}

void RootSignatureHandle::Clear() {
  hlsl::DeleteRootSignature(m_pDesc);
  m_pDesc = nullptr;
  if (m_pSerialized != nullptr) {
    m_pSerialized->Release();
    m_pSerialized = nullptr;
  }
}

const uint8_t *RootSignatureHandle::GetSerializedBytes() const {
  DXASSERT_NOMSG(m_pSerialized != nullptr);
  return (uint8_t *)m_pSerialized->GetBufferPointer();
}

unsigned RootSignatureHandle::GetSerializedSize() const {
  DXASSERT_NOMSG(m_pSerialized != nullptr);
  return m_pSerialized->GetBufferSize();
}

void RootSignatureHandle::EnsureSerializedAvailable() {
  DXASSERT_NOMSG(!IsEmpty());
  if (m_pSerialized == nullptr) {
    CComPtr<IDxcBlob> pResult;
    hlsl::SerializeRootSignature(m_pDesc, &pResult, nullptr, false);
    IFTBOOL(pResult != nullptr, E_FAIL);
    m_pSerialized = pResult.Detach();
  }
}

void RootSignatureHandle::Deserialize() {
  DXASSERT_NOMSG(m_pSerialized && !m_pDesc);
  DeserializeRootSignature((uint8_t*)m_pSerialized->GetBufferPointer(), (uint32_t)m_pSerialized->GetBufferSize(), &m_pDesc);
}

void RootSignatureHandle::LoadSerialized(const uint8_t *pData,
                                         unsigned length) {
  DXASSERT_NOMSG(IsEmpty());
  IDxcBlobEncoding *pCreated;
  IFT(DxcCreateBlobWithEncodingOnHeapCopy(pData, length, CP_UTF8, &pCreated));
  m_pSerialized = pCreated;
}

//////////////////////////////////////////////////////////////////////////////

namespace root_sig_helper {
// GetFlags/SetFlags overloads.
DxilRootDescriptorFlags GetFlags(const DxilRootDescriptor &) {
  // Upconvert root parameter flags to be volatile.
  return DxilRootDescriptorFlags::DataVolatile;
}
void SetFlags(DxilRootDescriptor &, DxilRootDescriptorFlags) {
  // Drop the flags; none existed in rs_1_0.
}
DxilRootDescriptorFlags GetFlags(const DxilRootDescriptor1 &D) {
  return D.Flags;
}
void SetFlags(DxilRootDescriptor1 &D, DxilRootDescriptorFlags Flags) {
  D.Flags = Flags;
}
void SetFlags(DxilContainerRootDescriptor1 &D, DxilRootDescriptorFlags Flags) {
  D.Flags = (uint32_t)Flags;
}
DxilDescriptorRangeFlags GetFlags(const DxilDescriptorRange &D) {
  // Upconvert range flags to be volatile.
  DxilDescriptorRangeFlags Flags =
      DxilDescriptorRangeFlags::DescriptorsVolatile;

  // Sampler does not have data.
  if (D.RangeType != DxilDescriptorRangeType::Sampler)
    Flags = (DxilDescriptorRangeFlags)(
        (unsigned)Flags | (unsigned)DxilDescriptorRangeFlags::DataVolatile);

  return Flags;
}
void SetFlags(DxilDescriptorRange &, DxilDescriptorRangeFlags) {}
DxilDescriptorRangeFlags GetFlags(const DxilContainerDescriptorRange &D) {
  // Upconvert range flags to be volatile.
  DxilDescriptorRangeFlags Flags =
      DxilDescriptorRangeFlags::DescriptorsVolatile;

  // Sampler does not have data.
  if (D.RangeType != (uint32_t)DxilDescriptorRangeType::Sampler)
    Flags |= DxilDescriptorRangeFlags::DataVolatile;

  return Flags;
}
void SetFlags(DxilContainerDescriptorRange &, DxilDescriptorRangeFlags) {}
DxilDescriptorRangeFlags GetFlags(const DxilDescriptorRange1 &D) {
  return D.Flags;
}
void SetFlags(DxilDescriptorRange1 &D, DxilDescriptorRangeFlags Flags) {
  D.Flags = Flags;
}
DxilDescriptorRangeFlags GetFlags(const DxilContainerDescriptorRange1 &D) {
  return (DxilDescriptorRangeFlags)D.Flags;
}
void SetFlags(DxilContainerDescriptorRange1 &D,
              DxilDescriptorRangeFlags Flags) {
  D.Flags = (uint32_t)Flags;
}

} // namespace root_sig_helper

//////////////////////////////////////////////////////////////////////////////

template <typename T>
void DeleteRootSignatureTemplate(const T &RS) {
  for (unsigned i = 0; i < RS.NumParameters; i++) {
    const auto &P = RS.pParameters[i];
    if (P.ParameterType == DxilRootParameterType::DescriptorTable) {
      delete[] P.DescriptorTable.pDescriptorRanges;
    }
  }

  delete[] RS.pParameters;
  delete[] RS.pStaticSamplers;
}

void DeleteRootSignature(const DxilVersionedRootSignatureDesc * pRootSignature)
{
  if (pRootSignature == nullptr)
    return;

  switch (pRootSignature->Version)
  {
  case DxilRootSignatureVersion::Version_1_0:
    DeleteRootSignatureTemplate<DxilRootSignatureDesc>(pRootSignature->Desc_1_0);
    break;
  case DxilRootSignatureVersion::Version_1_1:
  default:
    DXASSERT(pRootSignature->Version == DxilRootSignatureVersion::Version_1_1, "else version is incorrect");
    DeleteRootSignatureTemplate<DxilRootSignatureDesc1>(pRootSignature->Desc_1_1);
    break;
  }

  delete pRootSignature;
}

} // namespace hlsl
