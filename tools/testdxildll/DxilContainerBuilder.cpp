///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcontainerbuilder.cpp                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the Dxil Container Builder                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DxilContainer/DxcContainerBuilder.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/ErrorCodes.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/dxcapi.h"

#include <algorithm>

using namespace hlsl;

HRESULT CreateDxcContainerBuilder(_In_ REFIID riid, _Out_ LPVOID *ppv) {
  // Call dxil.dll's containerbuilder
  *ppv = nullptr;
  CComPtr<DxcContainerBuilder> Result(
      DxcContainerBuilder::Alloc(DxcGetThreadMallocNoRef()));
  IFROOM(Result.p);
  Result->Init();
  return Result->QueryInterface(riid, ppv);
}
