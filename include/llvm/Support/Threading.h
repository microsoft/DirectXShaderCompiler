//===-- llvm/Support/Threading.h - Control multithreading mode --*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Threading.h                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file declares helper functions for running LLVM in a multi-threaded  //
// environment.                                                              //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_SUPPORT_THREADING_H
#define LLVM_SUPPORT_THREADING_H

namespace llvm {
  /// Returns true if LLVM is compiled with support for multi-threading, and
  /// false otherwise.
  bool llvm_is_multithreaded();

  /// llvm_execute_on_thread - Execute the given \p UserFn on a separate
  /// thread, passing it the provided \p UserData and waits for thread 
  /// completion.
  ///
  /// This function does not guarantee that the code will actually be executed
  /// on a separate thread or honoring the requested stack size, but tries to do
  /// so where system support is available.
  ///
  /// \param UserFn - The callback to execute.
  /// \param UserData - An argument to pass to the callback function.
  /// \param RequestedStackSize - If non-zero, a requested size (in bytes) for
  /// the thread stack.
  void llvm_execute_on_thread(void (*UserFn)(void*), void *UserData,
                              unsigned RequestedStackSize = 0);
}

#endif
