///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxccontainerbuilder.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the Dxil Container Builder                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DxilContainer/DxcContainerBuilder.h"

HRESULT CreateDxcHashingContainerBuilder(REFIID RRID, LPVOID *V) {
  // Call dxil.dll's containerbuilder
  *V = nullptr;
  CComPtr<DxcContainerBuilder> Result(
      DxcContainerBuilder::Alloc(DxcGetThreadMallocNoRef()));
  if (nullptr == Result.p)
    return E_OUTOFMEMORY;

  Result->Init();
  return Result->QueryInterface(RRID, V);
}
