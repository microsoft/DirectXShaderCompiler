//===- llvm/Support/Windows/COM.inc - Windows COM Implementation *- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// COM.inc                                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the Windows portion of COM support.                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
//=== WARNING: Implementation here must contain only Windows code.           //

#include <objbase.h>

namespace llvm {
namespace sys {

InitializeCOMRAII::InitializeCOMRAII(COMThreadingMode Threading,
                                     bool SpeedOverMemory) {
  DWORD Coinit = 0;
  if (Threading == COMThreadingMode::SingleThreaded)
    Coinit |= COINIT_APARTMENTTHREADED;
  else
    Coinit |= COINIT_MULTITHREADED;
  if (SpeedOverMemory)
    Coinit |= COINIT_SPEED_OVER_MEMORY;
  ::CoInitializeEx(nullptr, Coinit);
}

InitializeCOMRAII::~InitializeCOMRAII() { ::CoUninitialize(); }
}
}
