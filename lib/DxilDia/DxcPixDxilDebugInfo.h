///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcPixDxilDebugInfo.h                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Declares the main class for dxcompiler's API for PIX support.             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"
#include "dxc/dxcpix.h"

#include "dxc/Support/Global.h"
#include "dxc/Support/microcom.h"

#include <memory>

namespace dxil_dia
{
class Session;
}  // namespace dxil_dia

namespace llvm
{
class Instruction;
class Module;
}  // namespace llvm

namespace dxil_debug_info
{
class LiveVariables;

class DxcPixDxilDebugInfo : public IDxcPixDxilDebugInfo
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CComPtr<dxil_dia::Session> m_pSession;
  std::unique_ptr<LiveVariables> m_LiveVars;

  DxcPixDxilDebugInfo(
      IMalloc *pMalloc,
      dxil_dia::Session *pSession);

  llvm::Instruction* FindInstruction(
      DWORD InstructionOffset
  ) const;

public:
  ~DxcPixDxilDebugInfo();

  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_ALLOC(DxcPixDxilDebugInfo)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDxcPixDxilDebugInfo>(this, iid, ppvObject);
  }

  STDMETHODIMP GetLiveVariablesAt(
      _In_ DWORD InstructionOffset,
      _COM_Outptr_ IDxcPixDxilLiveVariables **ppLiveVariables) override;

  STDMETHODIMP IsVariableInRegister(
      _In_ DWORD InstructionOffset,
      _In_ const wchar_t *VariableName) override;

  STDMETHODIMP GetFunctionName(
      _In_ DWORD InstructionOffset,
      _Outptr_result_z_ BSTR *ppFunctionName) override;

  STDMETHODIMP GetStackDepth(
      _In_ DWORD InstructionOffset,
      _Outptr_ DWORD *StackDepth) override;

  llvm::Module *GetModuleRef();

  IMalloc *GetMallocNoRef()
  {
    return m_pMalloc;
  }
};
}  // namespace dxil_debug_info
