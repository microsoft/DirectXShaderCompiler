//===- ThreadLocal.cpp - Thread Local Data ----------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ThreadLocal.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the llvm::sys::ThreadLocal class.                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Config/config.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ThreadLocal.h"

//===----------------------------------------------------------------------===//
//=== WARNING: Implementation here must contain only TRULY operating system
//===          independent code.
//===----------------------------------------------------------------------===//

#if !defined(LLVM_ENABLE_THREADS) || LLVM_ENABLE_THREADS == 0
// Define all methods as no-ops if threading is explicitly disabled
namespace llvm {
using namespace sys;
ThreadLocalImpl::ThreadLocalImpl() : data() { }
ThreadLocalImpl::~ThreadLocalImpl() { }
void ThreadLocalImpl::setInstance(const void* d) {
  static_assert(sizeof(d) <= sizeof(data), "size too big");
  void **pd = reinterpret_cast<void**>(&data);
  *pd = const_cast<void*>(d);
}
void *ThreadLocalImpl::getInstance() {
  void **pd = reinterpret_cast<void**>(&data);
  return *pd;
}
void ThreadLocalImpl::removeInstance() {
  setInstance(nullptr);
}
}
#elif defined(LLVM_ON_UNIX)
#include "Unix/ThreadLocal.inc"
#elif defined( LLVM_ON_WIN32)
#include "Windows/ThreadLocal.inc"
#else
#warning Neither LLVM_ON_UNIX nor LLVM_ON_WIN32 set in Support/ThreadLocal.cpp
#endif
