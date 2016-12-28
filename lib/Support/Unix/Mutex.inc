//===- llvm/Support/Unix/Mutex.inc - Unix Mutex Implementation ---*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Mutex.inc                                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the Unix specific (non-pthread) Mutex class.         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
//=== WARNING: Implementation here must contain only generic UNIX code that  //
//===          is guaranteed to work on *all* UNIX variants.                 //

namespace llvm
{
using namespace sys;

MutexImpl::MutexImpl( bool recursive)
{
}

MutexImpl::~MutexImpl()
{
}

bool
MutexImpl::release()
{
  return true;
}

bool
MutexImpl::tryacquire( void )
{
  return true;
}

}
