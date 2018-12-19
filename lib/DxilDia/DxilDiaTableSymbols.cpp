///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilDiaTableSymbols.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// DIA API implementation for DXIL modules.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "DxilDiaTableSymbols.h"

#include <comdef.h>

#include "dxc/Support/Unicode.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Metadata.h"

#include "DxilDiaSession.h"

HRESULT dxil_dia::Symbol::Create(IMalloc *pMalloc, Session *pSession, DWORD index, DWORD symTag, Symbol **pSymbol) {
  *pSymbol = Alloc(pMalloc);
  if (*pSymbol == nullptr) return E_OUTOFMEMORY;
  (*pSymbol)->AddRef();
  (*pSymbol)->Init(pSession, index, symTag);
  return S_OK;
}

void dxil_dia::Symbol::Init(Session *pSession, DWORD index, DWORD symTag) {
  m_pSession = pSession;
  m_index = index;
  m_symTag = symTag;
}

STDMETHODIMP dxil_dia::Symbol::get_symIndexId(
  /* [retval][out] */ DWORD *pRetVal) {
  *pRetVal = m_index;
  return S_OK;
}

STDMETHODIMP dxil_dia::Symbol::get_symTag(
  /* [retval][out] */ DWORD *pRetVal) {
  *pRetVal = m_symTag;
  return S_OK;
}

STDMETHODIMP dxil_dia::Symbol::get_name(
  /* [retval][out] */ BSTR *pRetVal) {
  return m_name.CopyTo(pRetVal);
}

STDMETHODIMP dxil_dia::Symbol::get_dataKind(
  /* [retval][out] */ DWORD *pRetVal) {
  *pRetVal = m_dataKind;
  return m_dataKind ? S_OK : S_FALSE;
}

STDMETHODIMP dxil_dia::Symbol::get_sourceFileName(
  /* [retval][out] */ BSTR *pRetVal) {
  if (pRetVal == nullptr) {
    return E_INVALIDARG;
  }
  *pRetVal = m_sourceFileName.Copy();
  return S_OK;
}

STDMETHODIMP dxil_dia::Symbol::get_value(
  /* [retval][out] */ VARIANT *pRetVal) {
  return VariantCopy(pRetVal, &m_value);
}

dxil_dia::SymbolsTable::SymbolsTable(IMalloc *pMalloc, Session *pSession)
  : impl::TableBase<IDiaEnumSymbols, IDiaSymbol>(pMalloc, pSession, Table::Kind::Symbols) {
    // The count is as follows:
    // One symbol for the program.
    // One Compiland per compilation unit.
    // One CompilandDetails per compilation unit.
    // Three CompilandEnv per Compliands: hlslFlags, hlslTarget, hlslEntry, hlslDefines, hlslArguments.
    // One Function/Data for each global.
    // One symbol for each type.
  const size_t SymbolsPerCU = 1 + 1 + 5;
  m_count = 1 + pSession->InfoRef().compile_unit_count() * SymbolsPerCU;
}

HRESULT dxil_dia::SymbolsTable::GetItem(DWORD index, IDiaSymbol **ppItem) {
  DxcThreadMalloc TM(m_pMalloc);

  // Ids are one-based, so adjust the index.
  ++index;

  // Program symbol.
  CComPtr<Symbol> item;
  if (index == HlslProgramId) {
    IFR(Symbol::Create(m_pMalloc, m_pSession, index, SymTagExe, &item));
    item->SetName(L"HLSL");
  } else if (index == HlslCompilandId) {
    IFR(Symbol::Create(m_pMalloc, m_pSession, index, SymTagCompiland, &item));
    item->SetName(L"main");
    item->SetLexicalParent(HlslProgramId);
    if (m_pSession->MainFileName()) {
      llvm::StringRef strRef = llvm::dyn_cast<llvm::MDString>(m_pSession->MainFileName()->getOperand(0)->getOperand(0))->getString();
      std::string str(strRef.begin(), strRef.size()); // To make sure str is null terminated
      item->SetSourceFileName(_bstr_t(Unicode::UTF8ToUTF16StringOrThrow(str.data()).c_str()));
    }
  } else if (index == HlslCompilandDetailsId) {
    IFR(Symbol::Create(m_pMalloc, m_pSession, index, SymTagCompilandDetails, &item));
    item->SetLexicalParent(HlslCompilandId);
    // TODO: complete the rest of the compiland details
    // platform: 256, language: 16, frontEndMajor: 6, frontEndMinor: 3, value: 0, hasDebugInfo: 1, compilerName: comiler string goes here
  } else if (index == HlslCompilandEnvFlagsId) {
    IFR(Symbol::Create(m_pMalloc, m_pSession, index, SymTagCompilandEnv, &item));
    item->SetLexicalParent(HlslCompilandId);
    item->SetName(L"hlslFlags");
    item->SetValue(m_pSession->DxilModuleRef().GetGlobalFlags());
  } else if (index == HlslCompilandEnvTargetId) {
    IFR(Symbol::Create(m_pMalloc, m_pSession, index, SymTagCompilandEnv, &item));
    item->SetLexicalParent(HlslCompilandId);
    item->SetName(L"hlslTarget");
    item->SetValue(m_pSession->DxilModuleRef().GetShaderModel()->GetName());
  } else if (index == HlslCompilandEnvEntryId) {
    IFR(Symbol::Create(m_pMalloc, m_pSession, index, SymTagCompilandEnv, &item));
    item->SetLexicalParent(HlslCompilandId);
    item->SetName(L"hlslEntry");
    item->SetValue(m_pSession->DxilModuleRef().GetEntryFunctionName().c_str());
  } else if (index == HlslCompilandEnvDefinesId) {
    IFR(Symbol::Create(m_pMalloc, m_pSession, index, SymTagCompilandEnv, &item));
    item->SetLexicalParent(HlslCompilandId);
    item->SetName(L"hlslDefines");
    llvm::MDNode *definesNode = m_pSession->Defines()->getOperand(0);
    // Construct a double null terminated string for defines with L"\0" as a delimiter
    CComBSTR pBSTR;
    for (llvm::MDNode::op_iterator it = definesNode->op_begin(); it != definesNode->op_end(); ++it) {
      llvm::StringRef strRef = llvm::dyn_cast<llvm::MDString>(*it)->getString();
      std::string str(strRef.begin(), strRef.size());
      CA2W cv(str.c_str());
      pBSTR.Append(cv);
      pBSTR.Append(L"\0", 1);
    }
    pBSTR.Append(L"\0", 1);
    VARIANT Variant;
    Variant.bstrVal = pBSTR;
    Variant.vt = VARENUM::VT_BSTR;
    item->SetValue(&Variant);
  } else if (index == HlslCompilandEnvArgumentsId) {
    IFR(Symbol::Create(m_pMalloc, m_pSession, index, SymTagCompilandEnv, &item));
    item->SetLexicalParent(HlslCompilandId);
    item->SetName(L"hlslArguments");
    auto Arguments = m_pSession->Arguments()->getOperand(0);
    auto NumArguments = Arguments->getNumOperands();
    std::string args;
    for (unsigned i = 0; i < NumArguments; ++i) {
      llvm::StringRef strRef = llvm::dyn_cast<llvm::MDString>(Arguments->getOperand(i))->getString();
      if (!args.empty())
        args.push_back(' ');
      args = args + strRef.str();
    }
    item->SetValue(args.c_str());
  }

  // TODO: add support for global data and functions as well as types.

  *ppItem = item.Detach();
  return (*ppItem == nullptr) ? E_FAIL : S_OK;
}
