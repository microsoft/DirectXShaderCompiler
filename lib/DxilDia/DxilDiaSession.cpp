///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilDiaSession.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// DIA API implementation for DXIL modules.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "DxilDiaSession.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"

#include "DxilDia.h"
#include "DxilDiaEnumTables.h"
#include "DxilDiaTable.h"
#include "DxilDiaTableInjectedSources.h"
#include "DxilDiaTableLineNumbers.h"

void dxil_dia::Session::Init(
    std::shared_ptr<llvm::LLVMContext> context,
    std::shared_ptr<llvm::Module> module,
    std::shared_ptr<llvm::DebugInfoFinder> finder) {
  m_pEnumTables = nullptr;
  m_module = module;
  m_context = context;
  m_finder = finder;
  m_dxilModule = std::make_unique<hlsl::DxilModule>(module.get());

  // Extract HLSL metadata.
  m_dxilModule->LoadDxilMetadata();

  // Get file contents.
  m_contents =
    m_module->getNamedMetadata(hlsl::DxilMDHelper::kDxilSourceContentsMDName);
  if (!m_contents)
    m_contents = m_module->getNamedMetadata("llvm.dbg.contents");

  m_defines =
    m_module->getNamedMetadata(hlsl::DxilMDHelper::kDxilSourceDefinesMDName);
  if (!m_defines)
    m_defines = m_module->getNamedMetadata("llvm.dbg.defines");

  m_mainFileName =
    m_module->getNamedMetadata(hlsl::DxilMDHelper::kDxilSourceMainFileNameMDName);
  if (!m_mainFileName)
    m_mainFileName = m_module->getNamedMetadata("llvm.dbg.mainFileName");

  m_arguments =
    m_module->getNamedMetadata(hlsl::DxilMDHelper::kDxilSourceArgsMDName);
  if (!m_arguments)
    m_arguments = m_module->getNamedMetadata("llvm.dbg.args");

  // Build up a linear list of instructions. The index will be used as the
  // RVA. Debug instructions are ommitted from this enumeration.
  for (const llvm::Function &fn : m_module->functions()) {
    for (llvm::const_inst_iterator it = inst_begin(fn), end = inst_end(fn); it != end; ++it) {
      const llvm::Instruction &i = *it;
      if (const auto *call = llvm::dyn_cast<const llvm::CallInst>(&i)) {
        const llvm::Function *pFn = call->getCalledFunction();
        if (pFn && pFn->getName().startswith("llvm.dbg.")) {
          continue;
        }
      }

      m_rvaMap.insert({ &i, static_cast<RVA>(m_instructions.size()) });
      m_instructions.push_back(&i);
      if (i.getDebugLoc()) {
        m_instructionLines.push_back(&i);
      }
    }
  }

  // Sanity check to make sure rva map is same as instruction index.
  for (size_t i = 0, e = m_instructions.size(); i < e; ++i) {
    DXASSERT(m_rvaMap.find(m_instructions[i]) != m_rvaMap.end(), "instruction not mapped to rva");
    DXASSERT(m_rvaMap[m_instructions[i]] == i, "instruction mapped to wrong rva");
  }
}

HRESULT dxil_dia::Session::getSourceFileIdByName(
    llvm::StringRef fileName,
    DWORD *pRetVal) {
  if (Contents() != nullptr) {
    for (unsigned i = 0; i < Contents()->getNumOperands(); ++i) {
      llvm::StringRef fn =
        llvm::dyn_cast<llvm::MDString>(Contents()->getOperand(i)->getOperand(0))
        ->getString();
      if (fn.equals(fileName)) {
        *pRetVal = i;
        return S_OK;
      }
    }
  }
  *pRetVal = 0;
  return S_FALSE;
}

STDMETHODIMP dxil_dia::Session::get_loadAddress(
    /* [retval][out] */ ULONGLONG *pRetVal) {
  *pRetVal = 0;
  return S_OK;
}

STDMETHODIMP dxil_dia::Session::getEnumTables(
    /* [out] */ _COM_Outptr_ IDiaEnumTables **ppEnumTables) {
  if (!m_pEnumTables) {
    DxcThreadMalloc TM(m_pMalloc);
    IFR(EnumTables::Create(this, &m_pEnumTables));
  }
  m_pEnumTables.p->AddRef();
  *ppEnumTables = m_pEnumTables;
  return S_OK;
}

STDMETHODIMP dxil_dia::Session::findFileById(
    /* [in] */ DWORD uniqueId,
    /* [out] */ IDiaSourceFile **ppResult) {
  if (!m_pEnumTables) {
    return E_INVALIDARG;
  }
  CComPtr<IDiaTable> pTable;
  VARIANT vtIndex;
  vtIndex.vt = VT_UI4;
  vtIndex.uintVal = (int)Table::Kind::SourceFiles;
  IFR(m_pEnumTables->Item(vtIndex, &pTable));
  CComPtr<IUnknown> pElt;
  IFR(pTable->Item(uniqueId, &pElt));
  return pElt->QueryInterface(ppResult);
}

namespace dxil_dia {
static HRESULT DxcDiaFindLineNumbersByRVA(
  Session *pSession,
  DWORD rva,
  DWORD length,
  IDiaEnumLineNumbers **ppResult)
{
  if (!ppResult)
    return E_POINTER;

  std::vector<const llvm::Instruction*> instructions;
  const std::vector<const llvm::Instruction*> &allInstructions = pSession->InstructionsRef();

  // Gather the list of insructions that map to the given rva range.
  for (DWORD i = rva; i < rva + length; ++i) {
    if (i >= allInstructions.size())
      return E_INVALIDARG;

    // Only include the instruction if it has debug info for line mappings.
    const llvm::Instruction *inst = allInstructions[i];
    if (inst->getDebugLoc())
      instructions.push_back(inst);
  }

  // Create line number table from explicit instruction list.
  IMalloc *pMalloc = pSession->GetMallocNoRef();
  *ppResult = CreateOnMalloc<LineNumbersTable>(pMalloc, pSession, std::move(instructions));
  if (*ppResult == nullptr)
    return E_OUTOFMEMORY;
  (*ppResult)->AddRef();
  return S_OK;
}
}  // namespace dxil_dia

STDMETHODIMP dxil_dia::Session::findLinesByAddr(
  /* [in] */ DWORD seg,
  /* [in] */ DWORD offset,
  /* [in] */ DWORD length,
  /* [out] */ IDiaEnumLineNumbers **ppResult) {
  DxcThreadMalloc TM(m_pMalloc);
  return DxcDiaFindLineNumbersByRVA(this, offset, length, ppResult);
}

STDMETHODIMP dxil_dia::Session::findLinesByRVA(
  /* [in] */ DWORD rva,
  /* [in] */ DWORD length,
  /* [out] */ IDiaEnumLineNumbers **ppResult) {
  DxcThreadMalloc TM(m_pMalloc);
  return DxcDiaFindLineNumbersByRVA(this, rva, length, ppResult);
}

STDMETHODIMP dxil_dia::Session::findInjectedSource(
  /* [in] */ LPCOLESTR srcFile,
  /* [out] */ IDiaEnumInjectedSources **ppResult) {
  if (Contents() != nullptr) {
    CW2A pUtf8FileName(srcFile);
    DxcThreadMalloc TM(m_pMalloc);
    IDiaTable *pTable;
    IFT(Table::Create(this, Table::Kind::InjectedSource, &pTable));
    auto *pInjectedSource =
      reinterpret_cast<InjectedSourcesTable *>(pTable);
    pInjectedSource->Init(pUtf8FileName.m_psz);
    *ppResult = pInjectedSource;
    return S_OK;
  }
  return S_FALSE;
}
