//===- WinEtwAdapter.h - Windows ETW Adapter, stub -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_WIN_ETW_ADAPTER_H
#define LLVM_SUPPORT_WIN_ETW_ADAPTER_H

// Event Tracing for Windows (ETW) provides application programmers the ability
// to start and stop event tracing sessions, instrument an application to
// provide trace events, and consume trace events.
#define EventRegisterMicrosoft_Windows_DXCompiler_API()
#define EventUnregisterMicrosoft_Windows_DXCompiler_API()
#define DxcEtw_DXCompilerCreateInstance_Start()
#define DxcEtw_DXCompilerCreateInstance_Stop(hr)
#define DxcEtw_DXCompilerInitialization_Start()
#define DxcEtw_DXCompilerInitialization_Stop(hr)
#define DxcEtw_DXCompilerShutdown_Start()
#define DxcEtw_DXCompilerShutdown_Stop(hr)
#define DxcEtw_DXCompilerCompile_Start()
#define DxcEtw_DXCompilerCompile_Stop(hr)
#define DxcEtw_DXCompilerDisassemble_Start()
#define DxcEtw_DXCompilerDisassemble_Stop(hr)
#define DxcEtw_DXCompilerPreprocess_Start()
#define DxcEtw_DXCompilerPreprocess_Stop(hr)
#define DxcEtw_DxcValidation_Start()
#define DxcEtw_DxcValidation_Stop(hr)

#endif // LLVM_SUPPORT_WIN_ETW_ADAPTER_H
