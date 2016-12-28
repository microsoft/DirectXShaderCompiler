//=- RPCChannel.inc - LLVM out-of-process JIT execution for Windows --=//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// RPCChannel.inc                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Implementation of the Windows-specific parts of the RPCChannel class      //
// which executes JITed code in a separate process from where it was built.  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

namespace llvm {

bool RPCChannel::createServer() { return false; }

bool RPCChannel::createClient() { return false; }

bool RPCChannel::WriteBytes(const void *Data, size_t Size) { return false; }

bool RPCChannel::ReadBytes(void *Data, size_t Size) { return false; }

void RPCChannel::Wait() {}

RPCChannel::~RPCChannel() {}

} // namespace llvm
