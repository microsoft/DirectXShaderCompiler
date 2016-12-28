//===- llvm/Support/COM.h ---------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// COM.h                                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
/// \file                                                                    //
///
/// Provides a library for accessing COM functionality of the Host OS.       //
///
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_SUPPORT_COM_H
#define LLVM_SUPPORT_COM_H

namespace llvm {
namespace sys {

enum class COMThreadingMode { SingleThreaded, MultiThreaded };

class InitializeCOMRAII {
public:
  explicit InitializeCOMRAII(COMThreadingMode Threading,
                             bool SpeedOverMemory = false);
  ~InitializeCOMRAII();

private:
  InitializeCOMRAII(const InitializeCOMRAII &) = delete;
  void operator=(const InitializeCOMRAII &) = delete;
};
}
}

#endif
