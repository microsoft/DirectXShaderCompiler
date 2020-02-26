///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxcPixDxilDebugInfo.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Defines the main class for dxcompiler's API for PIX support.              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"

#include "dxc/Support/exception.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"

#include "DxcPixLiveVariables.h"
#include "DxcPixDxilDebugInfo.h"

STDMETHODIMP dxil_debug_info::DxcPixDxilDebugInfo::GetLiveVariablesAt(
  _In_ DWORD InstructionOffset,
  _COM_Outptr_ IDxcPixDxilLiveVariables **ppLiveVariables)
{
  return m_LiveVars->GetLiveVariablesAtInstruction(
      FindInstruction(InstructionOffset),
      ppLiveVariables);
}

STDMETHODIMP dxil_debug_info::DxcPixDxilDebugInfo::IsVariableInRegister(
    _In_ DWORD InstructionOffset,
    _In_ const wchar_t *VariableName)
{
  return S_OK;
}

STDMETHODIMP dxil_debug_info::DxcPixDxilDebugInfo::GetFunctionName(
    _In_ DWORD InstructionOffset,
    _Outptr_result_z_ BSTR *ppFunctionName)
{
  llvm::Instruction *IP = FindInstruction(InstructionOffset);

  const llvm::DITypeIdentifierMap EmptyMap;

  if (const llvm::DebugLoc &DL = IP->getDebugLoc())
  {
    auto *S = llvm::dyn_cast<llvm::DIScope>(DL.getScope());
    while(S != nullptr && !llvm::isa<llvm::DICompileUnit>(S))
    {
      if (auto *SS = llvm::dyn_cast<llvm::DISubprogram>(S))
      {
        *ppFunctionName = CComBSTR(CA2W(SS->getName().data())).Detach();
        return S_OK;
      }

      S = S->getScope().resolve(EmptyMap);
    }
  }

  *ppFunctionName = CComBSTR(L"<???>").Detach();
  return S_FALSE;
}

STDMETHODIMP dxil_debug_info::DxcPixDxilDebugInfo::GetStackDepth(
    _In_ DWORD InstructionOffset,
    _Outptr_ DWORD *StackDepth
)
{
  llvm::Instruction *IP = FindInstruction(InstructionOffset);

  DWORD Depth = 0;
  llvm::DebugLoc DL = IP->getDebugLoc();
  while (DL && DL.getInlinedAtScope() != nullptr)
  {
    DL = DL.getInlinedAt();
    ++Depth;
  }

  *StackDepth = Depth;
  return S_OK;
}

#include "DxilDiaSession.h"

dxil_debug_info::DxcPixDxilDebugInfo::DxcPixDxilDebugInfo(
    IMalloc *pMalloc,
    dxil_dia::Session *pSession)
    : m_pMalloc(pMalloc)
    , m_pSession(pSession)
    , m_LiveVars(new LiveVariables())
{
  m_LiveVars->Init(this);
}

dxil_debug_info::DxcPixDxilDebugInfo::~DxcPixDxilDebugInfo() = default;

llvm::Module* dxil_debug_info::DxcPixDxilDebugInfo::GetModuleRef()
{
  return &m_pSession->ModuleRef();
}

llvm::Instruction* dxil_debug_info::DxcPixDxilDebugInfo::FindInstruction(
    DWORD InstructionOffset
) const
{
  const auto Instructions = m_pSession->InstructionsRef();
  auto it = Instructions.find(InstructionOffset);
  if (it == Instructions.end())
  {
    throw hlsl::Exception(E_BOUNDS, "Out-of-bounds: Instruction offset");
  }

  return const_cast<llvm::Instruction *>(it->second);
}