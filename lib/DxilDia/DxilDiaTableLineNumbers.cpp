///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilDiaTableLineNumbers.cpp                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// DIA API implementation for DXIL modules.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "DxilDiaTableLineNumbers.h"

#include <utility>

#include "llvm/IR/DebugInfoMetadata.h"

#include "DxilDiaSession.h"

dxil_dia::LineNumber::LineNumber(
  /* [in] */ IMalloc *pMalloc,
  /* [in] */ Session *pSession,
  /* [in] */ const llvm::Instruction * inst)
  : m_pMalloc(pMalloc),
    m_pSession(pSession),
    m_inst(inst) {
}

const llvm::DebugLoc &dxil_dia::LineNumber::DL() const {
  DXASSERT(bool(m_inst->getDebugLoc()), "Trying to read line info from invalid debug location");
  return m_inst->getDebugLoc();
}

STDMETHODIMP dxil_dia::LineNumber::get_sourceFile(
  /* [retval][out] */ IDiaSourceFile **pRetVal) {
  DWORD id;
  HRESULT hr = get_sourceFileId(&id);
  if (hr != S_OK)
    return hr;
  return m_pSession->findFileById(id, pRetVal);
}

STDMETHODIMP dxil_dia::LineNumber::get_lineNumber(
  /* [retval][out] */ DWORD *pRetVal) {
  *pRetVal = DL().getLine();
  return S_OK;
}

STDMETHODIMP dxil_dia::LineNumber::get_lineNumberEnd(
  /* [retval][out] */ DWORD *pRetVal) {
  *pRetVal = DL().getLine();
  return S_OK;
}

STDMETHODIMP dxil_dia::LineNumber::get_columnNumber(
  /* [retval][out] */ DWORD *pRetVal) {
  *pRetVal = DL().getCol();
  return S_OK;
}

STDMETHODIMP dxil_dia::LineNumber::get_columnNumberEnd(
  /* [retval][out] */ DWORD *pRetVal) {
  *pRetVal = DL().getCol();
  return S_OK;
}

STDMETHODIMP dxil_dia::LineNumber::get_relativeVirtualAddress(
  /* [retval][out] */ DWORD *pRetVal) {
  *pRetVal = m_pSession->RvaMapRef()[m_inst];
  return S_OK;
}

STDMETHODIMP dxil_dia::LineNumber::get_sourceFileId(
  /* [retval][out] */ DWORD *pRetVal) {
  llvm::MDNode *pScope = DL().getScope();
  auto *pBlock = llvm::dyn_cast_or_null<llvm::DILexicalBlock>(pScope);
  if (pBlock != nullptr) {
    return m_pSession->getSourceFileIdByName(pBlock->getFile()->getFilename(), pRetVal);
  }
  auto *pSubProgram = llvm::dyn_cast_or_null<llvm::DISubprogram>(pScope);
  if (pSubProgram != nullptr) {
    return m_pSession->getSourceFileIdByName(pSubProgram->getFile()->getFilename(), pRetVal);
  }
  *pRetVal = 0;
  return S_FALSE;
}

STDMETHODIMP dxil_dia::LineNumber::get_compilandId(
  /* [retval][out] */ DWORD *pRetVal) {
  // Single compiland for now, so pretty simple.
  *pRetVal = HlslCompilandId;
  return S_OK;
}

dxil_dia::LineNumbersTable::LineNumbersTable(IMalloc *pMalloc, Session *pSession)
  : impl::TableBase<IDiaEnumLineNumbers, IDiaLineNumber>(pMalloc, pSession, Table::Kind::LineNumbers)
  , m_instructions(pSession->InstructionLinesRef())
{
  m_count = m_instructions.size();
}

dxil_dia::LineNumbersTable::LineNumbersTable(IMalloc *pMalloc, Session *pSession, std::vector<const llvm::Instruction*> &&instructions)
  : impl::TableBase<IDiaEnumLineNumbers, IDiaLineNumber>(pMalloc, pSession, Table::Kind::LineNumbers)
  , m_instructions(m_instructionsStorage)
  , m_instructionsStorage(std::move(instructions))
{
  m_count = m_instructions.size();
}


HRESULT dxil_dia::LineNumbersTable::GetItem(DWORD index, IDiaLineNumber **ppItem) {
  if (index >= m_instructions.size())
    return E_INVALIDARG;
  *ppItem = CreateOnMalloc<LineNumber>(m_pMalloc, m_pSession, m_instructions[index]);
  if (*ppItem == nullptr)
    return E_OUTOFMEMORY;
  (*ppItem)->AddRef();
  return S_OK;
}