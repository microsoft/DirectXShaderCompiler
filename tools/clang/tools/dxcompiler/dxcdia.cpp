///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcdia.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the diagnostic APIs for a DirectX Compiler program.            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "clang/Sema/SemaHLSL.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

#include "dxc/Support/WinIncludes.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilMetadataHelper.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilUtil.h"
#include "dxc/Support/Global.h"
#ifdef _WIN32
#include "dia2.h"
#endif

#include "dxc/dxcapi.internal.h"

#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/dxcapi.impl.h"
#include <algorithm>
#include <array>
#ifdef _WIN32
#include <comdef.h>
#endif
#include "dxcutil.h"

using namespace llvm;
using namespace clang;
using namespace hlsl;

///////////////////////////////////////////////////////////////////////////////
// Forward declarations.
class DxcDiaDataSource;
class DxcDiaEnumSegments;
class DxcDiaEnumTables;
class DxcDiaSegment;
class DxcDiaSession;
class DxcDiaSymbol;
class DxcDiaTable;

///////////////////////////////////////////////////////////////////////////////
// Constants and helper structures.
enum class DiaTableKind {
  Symbols,
  SourceFiles,
  LineNumbers,
  Sections,
  SegmentMap,
  InjectedSource,
  FrameData,
  InputAssemblyFile
};
static const DiaTableKind FirstTableKind = DiaTableKind::Symbols;
static const DiaTableKind LastTableKind = DiaTableKind::InputAssemblyFile;

const LPCWSTR TableNames[] = {
  L"Symbols",
  L"SourceFiles",
  L"LineNumbers",
  L"Sections",
  L"SegmentMap",
  L"InjectedSource",
  L"FrameData",
  L"InputAssemblyFiles"
};

// Single program, single compiland allows for some simplifications.
static const DWORD HlslProgramId = 1;
static const DWORD HlslCompilandId = 2;
static const DWORD HlslCompilandDetailsId = 3;
static const DWORD HlslCompilandEnvFlagsId = 4;
static const DWORD HlslCompilandEnvTargetId = 5;
static const DWORD HlslCompilandEnvEntryId = 6;
static const DWORD HlslCompilandEnvDefinesId = 7;
static const DWORD HlslCompilandEnvArgumentsId = 8;

///////////////////////////////////////////////////////////////////////////////
// Memory helpers.
static
std::unique_ptr<MemoryBuffer> getMemBufferFromBlob(_In_ IDxcBlob *pBlob,
                                                   const Twine &BufferName) {
  StringRef Data((LPSTR)pBlob->GetBufferPointer(), pBlob->GetBufferSize());
  return MemoryBuffer::getMemBufferCopy(Data, BufferName);
}

static
std::unique_ptr<MemoryBuffer> getMemBufferFromStream(_In_ IStream *pStream,
                                                     const Twine &BufferName) {
  CComPtr<IDxcBlob> pBlob;
  if (SUCCEEDED(pStream->QueryInterface(&pBlob))) {
    return getMemBufferFromBlob(pBlob, BufferName);
  }

  STATSTG statstg;
  IFT(pStream->Stat(&statstg, STATFLAG_NONAME));
  size_t size = statstg.cbSize.LowPart;
  std::unique_ptr<llvm::MemoryBuffer> result(
    llvm::MemoryBuffer::getNewUninitMemBuffer(size, BufferName));
  char *pBuffer = (char *)result.get()->getBufferStart();
  ULONG read;
  IFT(pStream->Read(pBuffer, size, &read));
  return result;
}

static HRESULT StringRefToBSTR(llvm::StringRef value, BSTR *pRetVal) {
  try {
    wchar_t *wide;
    size_t sideSize;
    if (!Unicode::UTF8BufferToUTF16Buffer(value.data(), value.size(), &wide,
                                          &sideSize))
      return E_FAIL;
    *pRetVal = SysAllocString(wide);
    delete[] wide;
  }
  CATCH_CPP_RETURN_HRESULT();
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// DirectX compiler API.

static HRESULT CreateDxcDiaEnumTables(DxcDiaSession *, IDiaEnumTables **);
static HRESULT CreateDxcDiaTable(DxcDiaSession *, DiaTableKind kind, IDiaTable **ppTable);
static HRESULT DxcDiaFindLineNumbersByRVA(DxcDiaSession *, DWORD rva, DWORD length, IDiaEnumLineNumbers **);

class DxcDiaSession : public IDiaSession {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  std::shared_ptr<llvm::LLVMContext> m_context;
  std::shared_ptr<llvm::Module> m_module;
  std::shared_ptr<llvm::DebugInfoFinder> m_finder;
  std::unique_ptr<DxilModule> m_dxilModule;
  llvm::NamedMDNode *m_contents;
  llvm::NamedMDNode *m_defines;
  llvm::NamedMDNode *m_mainFileName;
  llvm::NamedMDNode *m_arguments;
  std::vector<const Instruction *> m_instructions;
  std::vector<const Instruction *> m_instructionLines; // Instructions with line info.
  typedef unsigned RVA;
  std::unordered_map<const Instruction *, RVA> m_rvaMap; // Map instruction to its RVA.
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcDiaSession)

  IMalloc *GetMallocNoRef() { return m_pMalloc.p; }

  void Init(std::shared_ptr<llvm::LLVMContext> context,
      std::shared_ptr<llvm::Module> module,
      std::shared_ptr<llvm::DebugInfoFinder> finder) {
    m_pEnumTables = nullptr;
    m_module = module;
    m_context = context;
    m_finder = finder;
    m_dxilModule = std::make_unique<DxilModule>(module.get());
  
    // Extract HLSL metadata.
    m_dxilModule->LoadDxilMetadata();

    // Get file contents.
    m_contents =
        m_module->getNamedMetadata(DxilMDHelper::kDxilSourceContentsMDName);
    if (!m_contents)
      m_contents = m_module->getNamedMetadata("llvm.dbg.contents");

    m_defines =
        m_module->getNamedMetadata(DxilMDHelper::kDxilSourceDefinesMDName);
    if (!m_defines)
      m_defines = m_module->getNamedMetadata("llvm.dbg.defines");

    m_mainFileName =
        m_module->getNamedMetadata(DxilMDHelper::kDxilSourceMainFileNameMDName);
    if (!m_mainFileName)
      m_mainFileName = m_module->getNamedMetadata("llvm.dbg.mainFileName");

    m_arguments =
        m_module->getNamedMetadata(DxilMDHelper::kDxilSourceArgsMDName);
    if (!m_arguments)
      m_arguments = m_module->getNamedMetadata("llvm.dbg.args");

    // Build up a linear list of instructions. The index will be used as the
    // RVA. Debug instructions are ommitted from this enumeration.
    for (const Function &fn : m_module->functions()) {
      for (const_inst_iterator it = inst_begin(fn), end = inst_end(fn); it != end; ++it) {
        const Instruction &i = *it;
        if (const CallInst *call = dyn_cast<const CallInst>(&i)) {
          const Function *pFn = call->getCalledFunction();
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
  llvm::NamedMDNode *Contents() { return m_contents; }
  llvm::NamedMDNode *Defines() { return m_defines; }
  llvm::NamedMDNode *MainFileName() { return m_mainFileName; }
  llvm::NamedMDNode *Arguments() { return m_arguments; }
  hlsl::DxilModule &DxilModuleRef() { return *m_dxilModule.get(); }
  llvm::Module &ModuleRef() { return *m_module.get(); }
  llvm::DebugInfoFinder &InfoRef() { return *m_finder.get(); }
  std::vector<const Instruction *> &InstructionsRef() { return m_instructions; }
  std::vector<const Instruction *> &InstructionLinesRef() { return m_instructionLines; }
  std::unordered_map<const Instruction *, RVA> &RvaMapRef() { return m_rvaMap; }

  HRESULT getSourceFileIdByName(StringRef fileName, DWORD *pRetVal) {
    if (Contents() != nullptr) {
      for (unsigned i = 0; i < Contents()->getNumOperands(); ++i) {
        StringRef fn =
            dyn_cast<MDString>(Contents()->getOperand(i)->getOperand(0))
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

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDiaSession>(this, iid, ppvObject);
  }

  STDMETHODIMP get_loadAddress(
    /* [retval][out] */ ULONGLONG *pRetVal) override { 
    *pRetVal = 0;
    return S_OK;
  }

  STDMETHODIMP put_loadAddress(
    /* [in] */ ULONGLONG NewVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_globalScope(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP getEnumTables(
    _COM_Outptr_ IDiaEnumTables **ppEnumTables) override {
    if (!m_pEnumTables) {
      DxcThreadMalloc TM(m_pMalloc);
      IFR(CreateDxcDiaEnumTables(this, &m_pEnumTables));
    }
    m_pEnumTables.p->AddRef();
    *ppEnumTables = m_pEnumTables;
    return S_OK;
  }

  STDMETHODIMP getSymbolsByAddr(
    /* [out] */ IDiaEnumSymbolsByAddr **ppEnumbyAddr) override { return E_NOTIMPL; }

  STDMETHODIMP findChildren(
    /* [in] */ IDiaSymbol *parent,
  /* [in] */ enum SymTagEnum symtag,
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD compareFlags,
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findChildrenEx(
    /* [in] */ IDiaSymbol *parent,
  /* [in] */ enum SymTagEnum symtag,
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD compareFlags,
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findChildrenExByAddr(
    /* [in] */ IDiaSymbol *parent,
  /* [in] */ enum SymTagEnum symtag,
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD compareFlags,
    /* [in] */ DWORD isect,
    /* [in] */ DWORD offset,
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findChildrenExByVA(
    /* [in] */ IDiaSymbol *parent,
  /* [in] */ enum SymTagEnum symtag,
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD compareFlags,
    /* [in] */ ULONGLONG va,
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findChildrenExByRVA(
    /* [in] */ IDiaSymbol *parent,
  /* [in] */ enum SymTagEnum symtag,
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD compareFlags,
    /* [in] */ DWORD rva,
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findSymbolByAddr(
    /* [in] */ DWORD isect,
    /* [in] */ DWORD offset,
  /* [in] */ enum SymTagEnum symtag,
    /* [out] */ IDiaSymbol **ppSymbol) override { return E_NOTIMPL; }

  STDMETHODIMP findSymbolByRVA(
    /* [in] */ DWORD rva,
  /* [in] */ enum SymTagEnum symtag,
    /* [out] */ IDiaSymbol **ppSymbol) override { return E_NOTIMPL; }

  STDMETHODIMP findSymbolByVA(
    /* [in] */ ULONGLONG va,
  /* [in] */ enum SymTagEnum symtag,
    /* [out] */ IDiaSymbol **ppSymbol) override { return E_NOTIMPL; }

  STDMETHODIMP findSymbolByToken(
    /* [in] */ ULONG token,
  /* [in] */ enum SymTagEnum symtag,
    /* [out] */ IDiaSymbol **ppSymbol) override { return E_NOTIMPL; }

  STDMETHODIMP symsAreEquiv(
    /* [in] */ IDiaSymbol *symbolA,
    /* [in] */ IDiaSymbol *symbolB) override { return E_NOTIMPL; }

  STDMETHODIMP symbolById(
    /* [in] */ DWORD id,
    /* [out] */ IDiaSymbol **ppSymbol) override { return E_NOTIMPL; }

  STDMETHODIMP findSymbolByRVAEx(
    /* [in] */ DWORD rva,
  /* [in] */ enum SymTagEnum symtag,
    /* [out] */ IDiaSymbol **ppSymbol,
    /* [out] */ long *displacement) override { return E_NOTIMPL; }

  STDMETHODIMP findSymbolByVAEx(
    /* [in] */ ULONGLONG va,
  /* [in] */ enum SymTagEnum symtag,
    /* [out] */ IDiaSymbol **ppSymbol,
    /* [out] */ long *displacement) override { return E_NOTIMPL; }

  STDMETHODIMP findFile(
    /* [in] */ IDiaSymbol *pCompiland,
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD compareFlags,
    /* [out] */ IDiaEnumSourceFiles **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findFileById(
    /* [in] */ DWORD uniqueId,
    /* [out] */ IDiaSourceFile **ppResult) override {
    if (!m_pEnumTables) {
      return E_INVALIDARG;
    }
    CComPtr<IDiaTable> pTable;
    VARIANT vtIndex;
    vtIndex.vt = VT_UI4;
    vtIndex.uintVal = (int)DiaTableKind::SourceFiles;
    IFR(m_pEnumTables->Item(vtIndex, &pTable));
    CComPtr<IUnknown> pElt;
    IFR(pTable->Item(uniqueId, &pElt));
    return pElt->QueryInterface(ppResult);
  }

  STDMETHODIMP findLines(
    /* [in] */ IDiaSymbol *compiland,
    /* [in] */ IDiaSourceFile *file,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findLinesByAddr(
    /* [in] */ DWORD seg,
    /* [in] */ DWORD offset,
    /* [in] */ DWORD length,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override {
      DxcThreadMalloc TM(m_pMalloc);
      return DxcDiaFindLineNumbersByRVA(this, offset, length, ppResult);
    }

  STDMETHODIMP findLinesByRVA(
    /* [in] */ DWORD rva,
    /* [in] */ DWORD length,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override {
    DxcThreadMalloc TM(m_pMalloc);
    return DxcDiaFindLineNumbersByRVA(this, rva, length, ppResult);
  }

  STDMETHODIMP findLinesByVA(
    /* [in] */ ULONGLONG va,
    /* [in] */ DWORD length,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findLinesByLinenum(
    /* [in] */ IDiaSymbol *compiland,
    /* [in] */ IDiaSourceFile *file,
    /* [in] */ DWORD linenum,
    /* [in] */ DWORD column,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInjectedSource(
      /* [in] */ LPCOLESTR srcFile,
      /* [out] */ IDiaEnumInjectedSources **ppResult) override;

  STDMETHODIMP getEnumDebugStreams(
    /* [out] */ IDiaEnumDebugStreams **ppEnumDebugStreams) override { return E_NOTIMPL; }

  STDMETHODIMP findInlineFramesByAddr(
    /* [in] */ IDiaSymbol *parent,
    /* [in] */ DWORD isect,
    /* [in] */ DWORD offset,
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInlineFramesByRVA(
    /* [in] */ IDiaSymbol *parent,
    /* [in] */ DWORD rva,
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInlineFramesByVA(
    /* [in] */ IDiaSymbol *parent,
    /* [in] */ ULONGLONG va,
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInlineeLines(
    /* [in] */ IDiaSymbol *parent,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInlineeLinesByAddr(
    /* [in] */ IDiaSymbol *parent,
    /* [in] */ DWORD isect,
    /* [in] */ DWORD offset,
    /* [in] */ DWORD length,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInlineeLinesByRVA(
    /* [in] */ IDiaSymbol *parent,
    /* [in] */ DWORD rva,
    /* [in] */ DWORD length,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInlineeLinesByVA(
    /* [in] */ IDiaSymbol *parent,
    /* [in] */ ULONGLONG va,
    /* [in] */ DWORD length,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInlineeLinesByLinenum(
    /* [in] */ IDiaSymbol *compiland,
    /* [in] */ IDiaSourceFile *file,
    /* [in] */ DWORD linenum,
    /* [in] */ DWORD column,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInlineesByName(
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD option,
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findAcceleratorInlineeLinesByLinenum(
    /* [in] */ IDiaSymbol *parent,
    /* [in] */ IDiaSourceFile *file,
    /* [in] */ DWORD linenum,
    /* [in] */ DWORD column,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findSymbolsForAcceleratorPointerTag(
    /* [in] */ IDiaSymbol *parent,
    /* [in] */ DWORD tagValue,
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findSymbolsByRVAForAcceleratorPointerTag(
    /* [in] */ IDiaSymbol *parent,
    /* [in] */ DWORD tagValue,
    /* [in] */ DWORD rva,
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findAcceleratorInlineesByName(
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD option,
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP addressForVA(
    /* [in] */ ULONGLONG va,
    /* [out] */ DWORD *pISect,
    /* [out] */ DWORD *pOffset) override { return E_NOTIMPL; }

  STDMETHODIMP addressForRVA(
    /* [in] */ DWORD rva,
    /* [out] */ DWORD *pISect,
    /* [out] */ DWORD *pOffset) override { return E_NOTIMPL; }

  STDMETHODIMP findILOffsetsByAddr(
    /* [in] */ DWORD isect,
    /* [in] */ DWORD offset,
    /* [in] */ DWORD length,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findILOffsetsByRVA(
    /* [in] */ DWORD rva,
    /* [in] */ DWORD length,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findILOffsetsByVA(
    /* [in] */ ULONGLONG va,
    /* [in] */ DWORD length,
    /* [out] */ IDiaEnumLineNumbers **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInputAssemblyFiles(
    /* [out] */ IDiaEnumInputAssemblyFiles **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInputAssembly(
    /* [in] */ DWORD index,
    /* [out] */ IDiaInputAssemblyFile **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInputAssemblyById(
    /* [in] */ DWORD uniqueId,
    /* [out] */ IDiaInputAssemblyFile **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP getFuncMDTokenMapSize(
    /* [out] */ DWORD *pcb) override { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE getFuncMDTokenMap(
    /* [in] */ DWORD cb,
    /* [out] */ DWORD *pcb,
    /* [size_is][out] */ BYTE *pb) { return E_NOTIMPL; }

  STDMETHODIMP getTypeMDTokenMapSize(
    /* [out] */ DWORD *pcb) override { return E_NOTIMPL; }

  STDMETHODIMP getTypeMDTokenMap(
    /* [in] */ DWORD cb,
    /* [out] */ DWORD *pcb,
    /* [size_is][out] */ BYTE *pb) override { return E_NOTIMPL; }

  STDMETHODIMP getNumberOfFunctionFragments_VA(
    /* [in] */ ULONGLONG vaFunc,
    /* [in] */ DWORD cbFunc,
    /* [out] */ DWORD *pNumFragments) override { return E_NOTIMPL; }

  STDMETHODIMP getNumberOfFunctionFragments_RVA(
    /* [in] */ DWORD rvaFunc,
    /* [in] */ DWORD cbFunc,
    /* [out] */ DWORD *pNumFragments) override { return E_NOTIMPL; }

  STDMETHODIMP getFunctionFragments_VA(
    /* [in] */ ULONGLONG vaFunc,
    /* [in] */ DWORD cbFunc,
    /* [in] */ DWORD cFragments,
    /* [size_is][out] */ ULONGLONG *pVaFragment,
    /* [size_is][out] */ DWORD *pLenFragment) override { return E_NOTIMPL; }

  STDMETHODIMP getFunctionFragments_RVA(
    /* [in] */ DWORD rvaFunc,
    /* [in] */ DWORD cbFunc,
    /* [in] */ DWORD cFragments,
    /* [size_is][out] */ DWORD *pRvaFragment,
    /* [size_is][out] */ DWORD *pLenFragment) override { return E_NOTIMPL; }

  STDMETHODIMP getExports(
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP getHeapAllocationSites(
    /* [out] */ IDiaEnumSymbols **ppResult) override { return E_NOTIMPL; }

  STDMETHODIMP findInputAssemblyFile(
    /* [in] */ IDiaSymbol *pSymbol,
    /* [out] */ IDiaInputAssemblyFile **ppResult) override { return E_NOTIMPL; }
private:
  CComPtr<IDiaEnumTables> m_pEnumTables;
};

class DxcDiaEnumTables : public IDiaEnumTables {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
protected:
  CComPtr<DxcDiaSession> m_pSession;
  unsigned m_next;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDiaEnumTables>(this, iid, ppvObject);
  }

  DxcDiaEnumTables(IMalloc *pMalloc, DxcDiaSession *pSession)
      : m_pMalloc(pMalloc), m_pSession(pSession), m_dwRef(0), m_next(0) {
    m_tables.fill(nullptr);
  }

  STDMETHODIMP get__NewEnum(
    /* [retval][out] */ IUnknown **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_Count(_Out_ LONG *pRetVal) override { 
    *pRetVal = ((unsigned)LastTableKind - (unsigned)FirstTableKind) + 1;
    return S_OK;
  }

  STDMETHODIMP Item(
    /* [in] */ VARIANT index,
    /* [retval][out] */ IDiaTable **table) override {
    // Avoid pulling in additional variant support (could have used VariantChangeType instead).
    DWORD indexVal;
    switch (index.vt) {
    case VT_UI4:
      indexVal = index.uintVal;
      break;
    case VT_I4:
      IFR(IntToDWord(index.intVal, &indexVal));
      break;
    default:
      return E_INVALIDARG;
    }
    if (indexVal > (unsigned)LastTableKind) {
      return E_INVALIDARG;
    }
    HRESULT hr = S_OK;
    if (!m_tables[indexVal]) {
      DxcThreadMalloc TM(m_pMalloc);
      hr = CreateDxcDiaTable(m_pSession, (DiaTableKind)indexVal, &m_tables[indexVal]);
    }
    m_tables[indexVal].p->AddRef();
    *table = m_tables[indexVal];
    return hr;
  }

  STDMETHODIMP Next(
    ULONG celt,
    IDiaTable **rgelt,
    ULONG *pceltFetched) override {
    DxcThreadMalloc TM(m_pMalloc);
    ULONG fetched = 0;
    while (fetched < celt && m_next <= (unsigned)LastTableKind) {
      HRESULT hr = S_OK;
      if (!m_tables[m_next]) {
        DxcThreadMalloc TM(m_pMalloc);
        hr = CreateDxcDiaTable(m_pSession, (DiaTableKind)m_next, &m_tables[m_next]);
        if (FAILED(hr)) {
          return hr; // TODO: this leaks prior tables.
        }
      }
      m_tables[m_next].p->AddRef();
      rgelt[fetched] = m_tables[m_next];
      ++m_next, ++fetched;
    }
    if (pceltFetched != nullptr)
      *pceltFetched = fetched;
    return (fetched == celt) ? S_OK : S_FALSE;
  }

  STDMETHODIMP Skip(
    /* [in] */ ULONG celt) override { return E_NOTIMPL; }

  STDMETHODIMP Reset(void) override { m_next = 0; return S_OK; }

  STDMETHODIMP Clone(
    /* [out] */ IDiaEnumTables **ppenum) override { return E_NOTIMPL; }
private:
  std::array<CComPtr<IDiaTable>, (int)LastTableKind+1> m_tables;
};

static HRESULT CreateDxcDiaEnumTables(DxcDiaSession *pSession, IDiaEnumTables **ppEnumTables) {
  *ppEnumTables = CreateOnMalloc<DxcDiaEnumTables>(pSession->GetMallocNoRef(), pSession);
  if (*ppEnumTables == nullptr)
    return E_OUTOFMEMORY;
  (*ppEnumTables)->AddRef();
  return S_OK;
}

template<typename T, typename TItem>
class DxcDiaTableBase : public IDiaTable, public T {
protected:
  DXC_MICROCOM_TM_REF_FIELDS()
  CComPtr<DxcDiaSession> m_pSession;
  unsigned m_next;
  unsigned m_count;
  DiaTableKind m_kind;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDiaTable, T, IEnumUnknown>(this, iid, ppvObject);
  }

  DxcDiaTableBase(IMalloc *pMalloc, DxcDiaSession *pSession, DiaTableKind kind) {
    m_pMalloc = pMalloc;
    m_pSession = pSession;
    m_kind = kind;
    m_next = 0;
    m_count = 0;
  }

  // IEnumUnknown implementation.
  STDMETHODIMP Next(
    _In_  ULONG celt,
    _Out_writes_to_(celt, *pceltFetched)  IUnknown **rgelt,
    _Out_opt_  ULONG *pceltFetched) override {
    DxcThreadMalloc TM(m_pMalloc);
    ULONG fetched = 0;
    while (fetched < celt && m_next < m_count) {
      HRESULT hr = Item(m_next, &rgelt[fetched]);
      if (FAILED(hr)) {
        return hr; // TODO: this leaks prior tables.
      }
      ++m_next, ++fetched;
    }
    if (pceltFetched != nullptr)
      *pceltFetched = fetched;
    return (fetched == celt) ? S_OK : S_FALSE;
  }

  STDMETHODIMP Skip(ULONG celt) override {
    if (celt + m_next <= m_count) {
      m_next += celt;
      return S_OK;
    }
    return S_FALSE;
  }

  STDMETHODIMP Reset(void) override {
    m_next = 0;
    return S_OK;
  }

  STDMETHODIMP Clone(IEnumUnknown **ppenum) override {
    return E_NOTIMPL;
  }

  // IDiaTable implementation.
  STDMETHODIMP get__NewEnum(IUnknown **pRetVal) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP get_name(BSTR *pRetVal) override {
    *pRetVal = SysAllocString(TableNames[(unsigned)m_kind]);
    return (*pRetVal) ? S_OK : E_OUTOFMEMORY;
  }

  STDMETHODIMP get_Count(_Out_ LONG *pRetVal) override {
    *pRetVal = m_count;
    return S_OK;
  }

  STDMETHODIMP Item(DWORD index, _COM_Outptr_ IUnknown **table) override {
    if (index >= m_count)
      return E_INVALIDARG;
    return GetItem(index, (TItem **)table);
  }

  // T implementation (partial).
  STDMETHODIMP Clone(_COM_Outptr_ T **ppenum) override {
    *ppenum = nullptr;
    return E_NOTIMPL;
  }
  STDMETHODIMP Next(
    /* [in] */ ULONG celt,
    /* [out] */ TItem **rgelt,
    /* [out] */ ULONG *pceltFetched) override {
    DxcThreadMalloc TM(m_pMalloc);
    ULONG fetched = 0;
    while (fetched < celt && m_next < m_count) {
      HRESULT hr = GetItem(m_next, &rgelt[fetched]);
      if (FAILED(hr)) {
        return hr; // TODO: this leaks prior items.
      }
      ++m_next, ++fetched;
    }
    if (pceltFetched != nullptr)
      *pceltFetched = fetched;
    return (fetched == celt) ? S_OK : S_FALSE;
  }
  STDMETHODIMP Item(
    /* [in] */ DWORD index,
    /* [retval][out] */ TItem **ppItem) override {
    DxcThreadMalloc TM(m_pMalloc);
    if (index >= m_count)
      return E_INVALIDARG;
    return GetItem(index, ppItem);
  }

  virtual HRESULT GetItem(DWORD index, TItem **ppItem) {
    UNREFERENCED_PARAMETER(index);
    *ppItem = nullptr;
    return E_NOTIMPL;
  }
};

class DxcDiaSymbol : public IDiaSymbol {
  DXC_MICROCOM_TM_REF_FIELDS()
  CComPtr<DxcDiaSession> m_pSession;
  DWORD m_index;
  DWORD m_symTag;
  DWORD m_lexicalParent = 0;
  DWORD m_dataKind = 0;
  CComBSTR m_sourceFileName;
  CComBSTR m_name;
  CComVariant m_value;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcDiaSymbol)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDiaSymbol>(this, iid, ppvObject);
  }

  static HRESULT Create(IMalloc *pMalloc, DxcDiaSession *pSession, DWORD index, DWORD symTag, DxcDiaSymbol **pSymbol) {
    *pSymbol = Alloc(pMalloc);
    if (*pSymbol == nullptr) return E_OUTOFMEMORY;
    (*pSymbol)->AddRef();
    (*pSymbol)->Init(pSession, index, symTag);
    return S_OK;
  }

  void Init(DxcDiaSession *pSession, DWORD index, DWORD symTag) {
    m_pSession = pSession;
    m_index = index;
    m_symTag = symTag;
  }

  void SetDataKind(DWORD value) { m_dataKind = value; }
  void SetLexicalParent(DWORD value) { m_lexicalParent = value; }
  void SetName(LPCWSTR value) { m_name = value; }
  void SetValue(LPCSTR value) { m_value = value; }
  void SetValue(VARIANT *pValue) { m_value.Copy(pValue); }
  void SetValue(unsigned value) { m_value = value; }
  void SetSourceFileName(BSTR value) { m_sourceFileName = value; }

#pragma region IDiaSymbol implementation.
  STDMETHODIMP get_symIndexId(
    /* [retval][out] */ DWORD *pRetVal) override { 
    *pRetVal = m_index;
    return S_OK;
  }

  STDMETHODIMP get_symTag(
    /* [retval][out] */ DWORD *pRetVal) override {
    *pRetVal = m_symTag;
    return S_OK;
  }

  STDMETHODIMP get_name(
    /* [retval][out] */ BSTR *pRetVal) override {
    return m_name.CopyTo(pRetVal);
  }

  STDMETHODIMP get_lexicalParent(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_classParent(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_type(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_dataKind(
    /* [retval][out] */ DWORD *pRetVal) override {
    *pRetVal = m_dataKind;
    return m_dataKind ? S_OK : S_FALSE;
  }

  STDMETHODIMP get_locationType(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_addressSection(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_addressOffset(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_relativeVirtualAddress(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_virtualAddress(
    /* [retval][out] */ ULONGLONG *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_registerId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_offset(
    /* [retval][out] */ LONG *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_length(
    /* [retval][out] */ ULONGLONG *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_slot(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_volatileType(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_constType(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_unalignedType(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_access(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_libraryName(
    /* [retval][out] */ BSTR *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_platform(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_language(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_editAndContinueEnabled(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_frontEndMajor(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_frontEndMinor(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_frontEndBuild(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_backEndMajor(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_backEndMinor(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_backEndBuild(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_sourceFileName(
    /* [retval][out] */ BSTR *pRetVal) override {
    if (pRetVal == nullptr) {
      return E_INVALIDARG;
    }
    *pRetVal = m_sourceFileName.Copy();
    return S_OK;
  }

  STDMETHODIMP get_unused(
    /* [retval][out] */ BSTR *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_thunkOrdinal(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_thisAdjust(
    /* [retval][out] */ LONG *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_virtualBaseOffset(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_virtual(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_intro(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_pure(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_callingConvention(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_value(
    /* [retval][out] */ VARIANT *pRetVal) override { 
    return VariantCopy(pRetVal, &m_value);
  }

  STDMETHODIMP get_baseType(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_token(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_timeStamp(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_guid(
    /* [retval][out] */ GUID *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_symbolsFileName(
    /* [retval][out] */ BSTR *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_reference(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_count(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_bitPosition(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_arrayIndexType(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_packed(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_constructor(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_overloadedOperator(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_nested(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasNestedTypes(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasAssignmentOperator(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasCastOperator(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_scoped(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_virtualBaseClass(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_indirectVirtualBaseClass(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_virtualBasePointerOffset(
    /* [retval][out] */ LONG *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_virtualTableShape(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_lexicalParentId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_classParentId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_typeId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_arrayIndexTypeId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_virtualTableShapeId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_code(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_function(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_managed(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_msil(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_virtualBaseDispIndex(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_undecoratedName(
    /* [retval][out] */ BSTR *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_age(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_signature(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_compilerGenerated(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_addressTaken(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_rank(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_lowerBound(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_upperBound(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_lowerBoundId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_upperBoundId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE get_dataBytes(
    /* [in] */ DWORD cbData,
    /* [out] */ DWORD *pcbData,
    /* [size_is][out] */ BYTE *pbData) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findChildren(
  /* [in] */ enum SymTagEnum symtag,
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD compareFlags,
    /* [out] */ IDiaEnumSymbols **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findChildrenEx(
  /* [in] */ enum SymTagEnum symtag,
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD compareFlags,
    /* [out] */ IDiaEnumSymbols **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findChildrenExByAddr(
  /* [in] */ enum SymTagEnum symtag,
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD compareFlags,
    /* [in] */ DWORD isect,
    /* [in] */ DWORD offset,
    /* [out] */ IDiaEnumSymbols **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findChildrenExByVA(
  /* [in] */ enum SymTagEnum symtag,
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD compareFlags,
    /* [in] */ ULONGLONG va,
    /* [out] */ IDiaEnumSymbols **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findChildrenExByRVA(
  /* [in] */ enum SymTagEnum symtag,
    /* [in] */ LPCOLESTR name,
    /* [in] */ DWORD compareFlags,
    /* [in] */ DWORD rva,
    /* [out] */ IDiaEnumSymbols **ppResult) { return E_NOTIMPL; }

  STDMETHODIMP get_targetSection(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_targetOffset(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_targetRelativeVirtualAddress(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_targetVirtualAddress(
    /* [retval][out] */ ULONGLONG *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_machineType(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_oemId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_oemSymbolId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE get_types(
    /* [in] */ DWORD cTypes,
    /* [out] */ DWORD *pcTypes,
    /* [size_is][size_is][out] */ IDiaSymbol **pTypes) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE get_typeIds(
    /* [in] */ DWORD cTypeIds,
    /* [out] */ DWORD *pcTypeIds,
    /* [size_is][out] */ DWORD *pdwTypeIds) { return E_NOTIMPL; }

  STDMETHODIMP get_objectPointerType(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_udtKind(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE get_undecoratedNameEx(
    /* [in] */ DWORD undecorateOptions,
    /* [out] */ BSTR *name) { return E_NOTIMPL; }

  STDMETHODIMP get_noReturn(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_customCallingConvention(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_noInline(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_optimizedCodeDebugInfo(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_notReached(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_interruptReturn(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_farReturn(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isStatic(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasDebugInfo(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isLTCG(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isDataAligned(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasSecurityChecks(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_compilerName(
    /* [retval][out] */ BSTR *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasAlloca(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasSetJump(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasLongJump(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasInlAsm(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasEH(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasSEH(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasEHa(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isNaked(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isAggregated(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isSplitted(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_container(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_inlSpec(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_noStackOrdering(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_virtualBaseTableType(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasManagedCode(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isHotpatchable(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isCVTCIL(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isMSILNetmodule(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isCTypes(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isStripped(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_frontEndQFE(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_backEndQFE(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_wasInlined(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_strictGSCheck(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isCxxReturnUdt(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isConstructorVirtualBase(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_RValueReference(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_unmodifiedType(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_framePointerPresent(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isSafeBuffers(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_intrinsic(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_sealed(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hfaFloat(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hfaDouble(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_liveRangeStartAddressSection(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_liveRangeStartAddressOffset(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_liveRangeStartRelativeVirtualAddress(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_countLiveRanges(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_liveRangeLength(
    /* [retval][out] */ ULONGLONG *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_offsetInUdt(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_paramBasePointerRegisterId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_localBasePointerRegisterId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isLocationControlFlowDependent(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_stride(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_numberOfRows(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_numberOfColumns(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isMatrixRowMajor(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE get_numericProperties(
    /* [in] */ DWORD cnt,
    /* [out] */ DWORD *pcnt,
    /* [size_is][out] */ DWORD *pProperties) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE get_modifierValues(
    /* [in] */ DWORD cnt,
    /* [out] */ DWORD *pcnt,
    /* [size_is][out] */ WORD *pModifiers) { return E_NOTIMPL; }

  STDMETHODIMP get_isReturnValue(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isOptimizedAway(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_builtInKind(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_registerType(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_baseDataSlot(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_baseDataOffset(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_textureSlot(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_samplerSlot(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_uavSlot(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_sizeInUdt(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_memorySpaceKind(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_unmodifiedTypeId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_subTypeId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_subType(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_numberOfModifiers(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_numberOfRegisterIndices(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isHLSLData(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isPointerToDataMember(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isPointerToMemberFunction(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isSingleInheritance(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isMultipleInheritance(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isVirtualInheritance(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_restrictedType(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isPointerBasedOnSymbolValue(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_baseSymbol(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_baseSymbolId(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_objectFileName(
    /* [retval][out] */ BSTR *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isAcceleratorGroupSharedLocal(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isAcceleratorPointerTagLiveRange(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isAcceleratorStubFunction(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_numberOfAcceleratorPointerTags(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isSdl(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isWinRTPointer(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isRefUdt(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isValueUdt(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isInterfaceUdt(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findInlineFramesByAddr(
    /* [in] */ DWORD isect,
    /* [in] */ DWORD offset,
    /* [out] */ IDiaEnumSymbols **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findInlineFramesByRVA(
    /* [in] */ DWORD rva,
    /* [out] */ IDiaEnumSymbols **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findInlineFramesByVA(
    /* [in] */ ULONGLONG va,
    /* [out] */ IDiaEnumSymbols **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findInlineeLines(
    /* [out] */ IDiaEnumLineNumbers **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findInlineeLinesByAddr(
    /* [in] */ DWORD isect,
    /* [in] */ DWORD offset,
    /* [in] */ DWORD length,
    /* [out] */ IDiaEnumLineNumbers **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findInlineeLinesByRVA(
    /* [in] */ DWORD rva,
    /* [in] */ DWORD length,
    /* [out] */ IDiaEnumLineNumbers **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findInlineeLinesByVA(
    /* [in] */ ULONGLONG va,
    /* [in] */ DWORD length,
    /* [out] */ IDiaEnumLineNumbers **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findSymbolsForAcceleratorPointerTag(
    /* [in] */ DWORD tagValue,
    /* [out] */ IDiaEnumSymbols **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findSymbolsByRVAForAcceleratorPointerTag(
    /* [in] */ DWORD tagValue,
    /* [in] */ DWORD rva,
    /* [out] */ IDiaEnumSymbols **ppResult) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE get_acceleratorPointerTags(
    /* [in] */ DWORD cnt,
    /* [out] */ DWORD *pcnt,
    /* [size_is][out] */ DWORD *pPointerTags) { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE getSrcLineOnTypeDefn(
    /* [out] */ IDiaLineNumber **ppResult) { return E_NOTIMPL; }

  STDMETHODIMP get_isPGO(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasValidPGOCounts(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_isOptimizedForSpeed(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_PGOEntryCount(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_PGOEdgeCount(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_PGODynamicInstructionCount(
    /* [retval][out] */ ULONGLONG *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_staticSize(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_finalLiveStaticSize(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_phaseName(
    /* [retval][out] */ BSTR *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_hasControlFlowCheck(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_constantExport(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_dataExport(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_privateExport(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_noNameExport(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_exportHasExplicitlyAssignedOrdinal(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_exportIsForwarder(
    /* [retval][out] */ BOOL *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_ordinal(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_frameSize(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_exceptionHandlerAddressSection(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_exceptionHandlerAddressOffset(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_exceptionHandlerRelativeVirtualAddress(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_exceptionHandlerVirtualAddress(
    /* [retval][out] */ ULONGLONG *pRetVal) override { return E_NOTIMPL; }

  virtual HRESULT STDMETHODCALLTYPE findInputAssemblyFile(
    /* [out] */ IDiaInputAssemblyFile **ppResult) { return E_NOTIMPL; }

  STDMETHODIMP get_characteristics(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_coffGroup(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  virtual STDMETHODIMP get_bindID(
    /* [retval][out] */ DWORD *pRetVal) { return E_NOTIMPL; }

  virtual STDMETHODIMP get_bindSpace(
    /* [retval][out] */ DWORD *pRetVal) { return E_NOTIMPL; }

  virtual STDMETHODIMP get_bindSlot(
    /* [retval][out] */ DWORD *pRetVal) { return E_NOTIMPL; }

#pragma endregion IDiaSymbol implementation.
};

class DxcDiaTableSymbols : public DxcDiaTableBase<IDiaEnumSymbols, IDiaSymbol> {
public:
  DxcDiaTableSymbols(IMalloc *pMalloc, DxcDiaSession *pSession) : DxcDiaTableBase(pMalloc, pSession, DiaTableKind::Symbols) {
    // The count is as follows:
    // One symbol for the program.
    // One Compiland per compilation unit.
    // One CompilandDetails per compilation unit.
    // Three CompilandEnv per Compliands: hlslFlags, hlslTarget, hlslEntry, hlslDefines, hlslArguments.
    // One Function/Data for each global.
    // One symbol for each type.
    const size_t SymbolsPerCU = 1 + 1 + 5;
    m_count = 1 + pSession->InfoRef().compile_unit_count() * SymbolsPerCU;
              //pSession->InfoRef().global_variable_count() +
              //pSession->InfoRef().type_count();
  }

  HRESULT GetItem(DWORD index, IDiaSymbol **ppItem) override {
    DxcThreadMalloc TM(m_pMalloc);

    // Ids are one-based, so adjust the index.
    ++index;

    // Program symbol.
    CComPtr<DxcDiaSymbol> item;
    if (index == HlslProgramId) {
      IFR(DxcDiaSymbol::Create(m_pMalloc, m_pSession, index, SymTagExe, &item));
      item->SetName(L"HLSL");
    }
    else if (index == HlslCompilandId) {
      IFR(DxcDiaSymbol::Create(m_pMalloc, m_pSession, index, SymTagCompiland, &item));
      item->SetName(L"main");
      item->SetLexicalParent(HlslProgramId);
      if (m_pSession->MainFileName()) {
        StringRef strRef = dyn_cast<MDString>(m_pSession->MainFileName()->getOperand(0)->getOperand(0))->getString();
        std::string str(strRef.begin(), strRef.size()); // To make sure str is null terminated
        item->SetSourceFileName(_bstr_t(Unicode::UTF8ToUTF16StringOrThrow(str.data()).c_str()));
      }
    }
    else if (index == HlslCompilandDetailsId) {
      IFR(DxcDiaSymbol::Create(m_pMalloc, m_pSession, index, SymTagCompilandDetails, &item));
      item->SetLexicalParent(HlslCompilandId);
      // TODO: complete the rest of the compiland details
      // platform: 256, language: 16, frontEndMajor: 6, frontEndMinor: 3, value: 0, hasDebugInfo: 1, compilerName: comiler string goes here
    }
    else if (index == HlslCompilandEnvFlagsId) {
      IFR(DxcDiaSymbol::Create(m_pMalloc, m_pSession, index, SymTagCompilandEnv, &item));
      item->SetLexicalParent(HlslCompilandId);
      item->SetName(L"hlslFlags");
      item->SetValue(m_pSession->DxilModuleRef().GetGlobalFlags());
    }
    else if (index == HlslCompilandEnvTargetId) {
      IFR(DxcDiaSymbol::Create(m_pMalloc, m_pSession, index, SymTagCompilandEnv, &item));
      item->SetLexicalParent(HlslCompilandId);
      item->SetName(L"hlslTarget");
      item->SetValue(m_pSession->DxilModuleRef().GetShaderModel()->GetName());
    }
    else if (index == HlslCompilandEnvEntryId) {
      IFR(DxcDiaSymbol::Create(m_pMalloc, m_pSession, index, SymTagCompilandEnv, &item));
      item->SetLexicalParent(HlslCompilandId);
      item->SetName(L"hlslEntry");
      item->SetValue(m_pSession->DxilModuleRef().GetEntryFunctionName().c_str());
    }
    else if (index == HlslCompilandEnvDefinesId) {
      IFR(DxcDiaSymbol::Create(m_pMalloc, m_pSession, index, SymTagCompilandEnv, &item));
      item->SetLexicalParent(HlslCompilandId);
      item->SetName(L"hlslDefines");
      UINT32 charSize = 0;
      llvm::MDNode *definesNode = m_pSession->Defines()->getOperand(0);
      // Construct a double null terminated string for defines with L"\0" as a delimiter
      CComBSTR pBSTR;
      for (llvm::MDNode::op_iterator it = definesNode->op_begin(); it != definesNode->op_end(); ++it) {
        StringRef strRef = dyn_cast<MDString>(*it)->getString();
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
    }
    else if (index == HlslCompilandEnvArgumentsId) {
      IFR(DxcDiaSymbol::Create(m_pMalloc, m_pSession, index, SymTagCompilandEnv, &item));
      item->SetLexicalParent(HlslCompilandId);
      item->SetName(L"hlslArguments");
      auto Arguments = m_pSession->Arguments()->getOperand(0);
      auto NumArguments = Arguments->getNumOperands();
      std::string args;
      for (unsigned i = 0; i < NumArguments; ++i) {
        StringRef strRef = dyn_cast<MDString>(Arguments->getOperand(i))->getString();
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
};

class DxcDiaSourceFile : public IDiaSourceFile {
  DXC_MICROCOM_TM_REF_FIELDS()
  CComPtr<DxcDiaSession> m_pSession;
  DWORD m_index;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDiaSourceFile>(this, iid, ppvObject);
  }

  DxcDiaSourceFile(IMalloc *pMalloc, DxcDiaSession *pSession, DWORD index)
    : m_pMalloc(pMalloc), m_pSession(pSession), m_index(index) {}

  llvm::MDTuple *NameContent() {
    return cast<llvm::MDTuple>(m_pSession->Contents()->getOperand(m_index));
  }
  llvm::StringRef Name() {
    return dyn_cast<llvm::MDString>(NameContent()->getOperand(0))->getString();
  }

  STDMETHODIMP get_uniqueId(
    /* [retval][out] */ DWORD *pRetVal) override {
    *pRetVal = m_index;
    return S_OK;
  }

  STDMETHODIMP get_fileName(
    /* [retval][out] */ BSTR *pRetVal) override {
    DxcThreadMalloc TM(m_pMalloc);
    return StringRefToBSTR(Name(), pRetVal);
  }

  STDMETHODIMP get_checksumType(
    /* [retval][out] */ DWORD *pRetVal) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP get_compilands(
    /* [retval][out] */ IDiaEnumSymbols **pRetVal) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP get_checksum(
    /* [in] */ DWORD cbData,
    /* [out] */ DWORD *pcbData,
    /* [size_is][out] */ BYTE *pbData) override {
    return E_NOTIMPL;
  }
};

class DxcDiaTableSourceFiles : public DxcDiaTableBase<IDiaEnumSourceFiles, IDiaSourceFile> {
public:
  DxcDiaTableSourceFiles(IMalloc *pMalloc, DxcDiaSession *pSession) : DxcDiaTableBase(pMalloc, pSession, DiaTableKind::SourceFiles) { 
    m_count =
      (m_pSession->Contents() == nullptr) ? 0 : m_pSession->Contents()->getNumOperands();
    m_items.assign(m_count, nullptr);
  }

  HRESULT GetItem(DWORD index, IDiaSourceFile **ppItem) override {
    if (!m_items[index]) {
      m_items[index] = CreateOnMalloc<DxcDiaSourceFile>(m_pMalloc, m_pSession, index);
      if (m_items[index] == nullptr)
        return E_OUTOFMEMORY;
    }
    m_items[index].p->AddRef();
    *ppItem = m_items[index];
    (*ppItem)->AddRef();
    return S_OK;
  }
private:
  std::vector<CComPtr<IDiaSourceFile>> m_items;
};

class DxcDiaLineNumber : public IDiaLineNumber {
  DXC_MICROCOM_TM_REF_FIELDS()
  CComPtr<DxcDiaSession> m_pSession;
  const Instruction *m_inst;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDiaLineNumber>(this, iid, ppvObject);
  }

  DxcDiaLineNumber(IMalloc *pMalloc, DxcDiaSession *pSession, const Instruction * inst)
    : m_pMalloc(pMalloc), m_pSession(pSession), m_inst(inst) {}

  const llvm::DebugLoc &DL() {
    DXASSERT(bool(m_inst->getDebugLoc()), "Trying to read line info from invalid debug location");
    return m_inst->getDebugLoc();
  }

  STDMETHODIMP get_compiland(
    /* [retval][out] */ IDiaSymbol **pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_sourceFile(
    /* [retval][out] */ IDiaSourceFile **pRetVal) override {
    DWORD id;
    HRESULT hr = get_sourceFileId(&id);
    if (hr != S_OK)
      return hr;
    return m_pSession->findFileById(id, pRetVal);
  }

  STDMETHODIMP get_lineNumber(
    /* [retval][out] */ DWORD *pRetVal) override {
    *pRetVal = DL().getLine();
    return S_OK;
  }

  STDMETHODIMP get_lineNumberEnd(
    /* [retval][out] */ DWORD *pRetVal) override {
    *pRetVal = DL().getLine();
    return S_OK;
  }

  STDMETHODIMP get_columnNumber(
    /* [retval][out] */ DWORD *pRetVal) override {
    *pRetVal = DL().getCol();
    return S_OK;
  }

  STDMETHODIMP get_columnNumberEnd(
    /* [retval][out] */ DWORD *pRetVal) override {
    *pRetVal = DL().getCol();
    return S_OK;
  }

  STDMETHODIMP get_addressSection(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_addressOffset(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_relativeVirtualAddress(
    /* [retval][out] */ DWORD *pRetVal) override { 
    *pRetVal = m_pSession->RvaMapRef()[m_inst];
    return S_OK;
  }

  STDMETHODIMP get_virtualAddress(
    /* [retval][out] */ ULONGLONG *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_length(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_sourceFileId(
    /* [retval][out] */ DWORD *pRetVal) override {
    MDNode *pScope = DL().getScope();
    DILexicalBlock *pBlock = dyn_cast_or_null<DILexicalBlock>(pScope);
    if (pBlock != nullptr) {
      return m_pSession->getSourceFileIdByName(pBlock->getFile()->getFilename(), pRetVal);
    }
    DISubprogram *pSubProgram= dyn_cast_or_null<DISubprogram>(pScope);
    if (pSubProgram != nullptr) {
      return m_pSession->getSourceFileIdByName(pSubProgram->getFile()->getFilename(), pRetVal);
    }
    *pRetVal = 0;
    return S_FALSE;
  }

  STDMETHODIMP get_statement(
    /* [retval][out] */ BOOL *pRetVal) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP get_compilandId(
    /* [retval][out] */ DWORD *pRetVal) override {
    // Single compiland for now, so pretty simple.
    *pRetVal = HlslCompilandId;
    return S_OK;
  }
};

// This class implements the line number table for dxc.
//
// It keeps a reference to the list of instructions that contain
// line number debug info. By default, it points to the full list
// of instructions that contain line info.
//
// It can also be passed a list of instructions that contain line info so
// that we can iterate over a subset of lines. When passed an explicit list
// it takes ownership of the list and points its reference to the internal
// copy of the list.
class DxcDiaTableLineNumbers : public DxcDiaTableBase<IDiaEnumLineNumbers, IDiaLineNumber> {
public:
  DxcDiaTableLineNumbers(IMalloc *pMalloc, DxcDiaSession *pSession) 
    : DxcDiaTableBase(pMalloc, pSession, DiaTableKind::LineNumbers)
    , m_instructions(pSession->InstructionLinesRef())
  {
    m_count = m_instructions.size();
  }
  
  DxcDiaTableLineNumbers(IMalloc *pMalloc, DxcDiaSession *pSession, std::vector<const Instruction*> &&instructions) 
    : DxcDiaTableBase(pMalloc, pSession, DiaTableKind::LineNumbers)
    , m_instructions(m_instructionsStorage)
    , m_instructionsStorage(std::move(instructions))
  {
    m_count = m_instructions.size();
  }
  

  HRESULT GetItem(DWORD index, IDiaLineNumber **ppItem) override {
    if (index >= m_instructions.size())
      return E_INVALIDARG;
    *ppItem = CreateOnMalloc<DxcDiaLineNumber>(m_pMalloc, m_pSession, m_instructions[index]);
    if (*ppItem == nullptr)
      return E_OUTOFMEMORY;
    (*ppItem)->AddRef();
    return S_OK;
  }

private:
  // Keep a reference to the instructions that contain the line numbers.
  const std::vector<const Instruction *> &m_instructions;
  
  // Provide storage space for instructions for when the table contains
  // a subset of all instructions.
  std::vector<const Instruction *> m_instructionsStorage;
};

static HRESULT DxcDiaFindLineNumbersByRVA(
  DxcDiaSession *pSession,
  DWORD rva,
  DWORD length,
  IDiaEnumLineNumbers **ppResult) 
{
  if (!ppResult)
    return E_POINTER;

  std::vector<const Instruction*> instructions;
  const std::vector<const Instruction*> &allInstructions = pSession->InstructionsRef();

  // Gather the list of insructions that map to the given rva range.
  for (DWORD i = rva; i < rva + length; ++i) {
    if (i >= allInstructions.size())
      return E_INVALIDARG;

    // Only include the instruction if it has debug info for line mappings.
    const Instruction *inst = allInstructions[i];
    if (inst->getDebugLoc())
      instructions.push_back(inst);
  }

  // Create line number table from explicit instruction list.
  IMalloc *pMalloc = pSession->GetMallocNoRef();
  *ppResult = CreateOnMalloc<DxcDiaTableLineNumbers>(pMalloc, pSession, std::move(instructions));
  if (*ppResult == nullptr)
    return E_OUTOFMEMORY;
  (*ppResult)->AddRef();
  return S_OK;
}

class DxcDiaTableSections : public DxcDiaTableBase<IDiaEnumSectionContribs, IDiaSectionContrib> {
public:
  DxcDiaTableSections(IMalloc *pMalloc, DxcDiaSession *pSession) : DxcDiaTableBase(pMalloc, pSession, DiaTableKind::Sections) { }
  HRESULT GetItem(DWORD index, IDiaSectionContrib **ppItem) override {
    *ppItem = nullptr;
    return E_FAIL;
  }
};

class DxcDiaTableSegmentMap : public DxcDiaTableBase<IDiaEnumSegments, IDiaSegment> {
public:
  DxcDiaTableSegmentMap(IMalloc *pMalloc, DxcDiaSession *pSession) : DxcDiaTableBase(pMalloc, pSession, DiaTableKind::SegmentMap) { }
  HRESULT GetItem(DWORD index, IDiaSegment **ppItem) override {
    *ppItem = nullptr;
    return E_FAIL;
  }
};

class DxcDiaInjectedSource : public IDiaInjectedSource {
  DXC_MICROCOM_TM_REF_FIELDS()
  CComPtr<DxcDiaSession> m_pSession;
  DWORD m_index;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDiaInjectedSource>(this, iid, ppvObject);
  }

  DxcDiaInjectedSource(IMalloc *pMalloc, DxcDiaSession *pSession, DWORD index)
    : m_pMalloc(pMalloc), m_pSession(pSession), m_index(index) {}

  llvm::MDTuple *NameContent() {
    return cast<llvm::MDTuple>(m_pSession->Contents()->getOperand(m_index));
  }
  llvm::StringRef Name() {
    return dyn_cast<llvm::MDString>(NameContent()->getOperand(0))->getString();
  }
  llvm::StringRef Content() {
    return dyn_cast<llvm::MDString>(NameContent()->getOperand(1))->getString();
  }

  STDMETHODIMP get_crc(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_length(_Out_ ULONGLONG *pRetVal) override {
    *pRetVal = Content().size();
    return S_OK;
  }

  STDMETHODIMP get_filename(BSTR *pRetVal) override {
    DxcThreadMalloc TM(m_pMalloc);
    return StringRefToBSTR(Name(), pRetVal);
  }

  STDMETHODIMP get_objectFilename(BSTR *pRetVal) override {
    *pRetVal = nullptr;
    return S_OK;
  }

  STDMETHODIMP get_virtualFilename(BSTR *pRetVal) override {
    return get_filename(pRetVal);
  }

  STDMETHODIMP get_sourceCompression(
    /* [retval][out] */ DWORD *pRetVal) override { return E_NOTIMPL; }

  STDMETHODIMP get_source(
    /* [in] */ DWORD cbData,
    /* [out] */ DWORD *pcbData,
    /* [size_is][out] */ BYTE *pbData) override {
    if (pbData == nullptr) {
      if (pcbData != nullptr) {
        *pcbData = Content().size();
      }
      return S_OK;
    }

    cbData = std::min((DWORD)Content().size(), cbData);
    memcpy(pbData, Content().begin(), cbData);
    if (pcbData) {
      *pcbData = cbData;
    }
    return S_OK;
  }
};

class DxcDiaTableInjectedSource : public DxcDiaTableBase<IDiaEnumInjectedSources, IDiaInjectedSource> {
public:
  DxcDiaTableInjectedSource(IMalloc *pMalloc, DxcDiaSession *pSession) : DxcDiaTableBase(pMalloc, pSession, DiaTableKind::InjectedSource) {
    // Count the number of source files available.
    // m_count = m_pSession->InfoRef().compile_unit_count();
    m_count =
      (m_pSession->Contents() == nullptr) ? 0 : m_pSession->Contents()->getNumOperands();
  }

  HRESULT GetItem(DWORD index, IDiaInjectedSource **ppItem) override {
    if (index >= m_count)
      return E_INVALIDARG;
    unsigned itemIndex = index;
    if (m_count == m_indexList.size())
      itemIndex = m_indexList[index];
    *ppItem = CreateOnMalloc<DxcDiaInjectedSource>(m_pMalloc, m_pSession, itemIndex);
    if (*ppItem == nullptr)
      return E_OUTOFMEMORY;
    (*ppItem)->AddRef();
    return S_OK;
  }
  void Init(StringRef filename) {
    for (unsigned i = 0; i < m_pSession->Contents()->getNumOperands(); ++i) {
      StringRef fn =
          dyn_cast<MDString>(m_pSession->Contents()->getOperand(i)->getOperand(0))
              ->getString();
      if (fn.equals(filename)) {
        m_indexList.emplace_back(i);
      }
    }
    m_count = m_indexList.size();
  }
private:
  std::vector<unsigned> m_indexList;
};

class DxcDiaTableFrameData : public DxcDiaTableBase<IDiaEnumFrameData, IDiaFrameData> {
public:
  DxcDiaTableFrameData(IMalloc *pMalloc, DxcDiaSession *pSession) : DxcDiaTableBase(pMalloc, pSession, DiaTableKind::FrameData) { }
  // HLSL inlines functions for a program, so no data to return.
  STDMETHODIMP frameByRVA(
    /* [in] */ DWORD relativeVirtualAddress,
    /* [retval][out] */ IDiaFrameData **frame) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP frameByVA(
    /* [in] */ ULONGLONG virtualAddress,
    /* [retval][out] */ IDiaFrameData **frame) override {
    return E_NOTIMPL;
  }
};

class DxcDiaTableInputAssemblyFile : public DxcDiaTableBase<IDiaEnumInputAssemblyFiles, IDiaInputAssemblyFile> {
public:
  DxcDiaTableInputAssemblyFile(IMalloc *pMalloc, DxcDiaSession *pSession) : DxcDiaTableBase(pMalloc, pSession, DiaTableKind::InputAssemblyFile) { }
  // HLSL is not based on IL, so no data to return.
};

STDMETHODIMP DxcDiaSession::findInjectedSource(
    /* [in] */ LPCOLESTR srcFile,
    /* [out] */ IDiaEnumInjectedSources **ppResult) {
  if (Contents() != nullptr) {
    CW2A pUtf8FileName(srcFile);
    DxcThreadMalloc TM(m_pMalloc);
    IDiaTable *pTable;
    IFT(CreateDxcDiaTable(this, DiaTableKind::InjectedSource, &pTable));
    DxcDiaTableInjectedSource *pInjectedSource =
        reinterpret_cast<DxcDiaTableInjectedSource *>(pTable);
    pInjectedSource->Init(pUtf8FileName.m_psz);
    *ppResult = pInjectedSource;
    return S_OK;
  }
  return S_FALSE;
}

static
HRESULT CreateDxcDiaTable(DxcDiaSession *pSession, DiaTableKind kind, IDiaTable **ppTable) {
  *ppTable = nullptr;
  IMalloc *pMalloc = pSession->GetMallocNoRef();
  switch (kind) {
  case DiaTableKind::Symbols: *ppTable = CreateOnMalloc<DxcDiaTableSymbols>(pMalloc, pSession); break;
  case DiaTableKind::SourceFiles: *ppTable = CreateOnMalloc<DxcDiaTableSourceFiles>(pMalloc, pSession); break;
  case DiaTableKind::LineNumbers: *ppTable = CreateOnMalloc<DxcDiaTableLineNumbers>(pMalloc, pSession); break;
  case DiaTableKind::Sections: *ppTable = CreateOnMalloc<DxcDiaTableSections>(pMalloc, pSession); break;
  case DiaTableKind::SegmentMap: *ppTable = CreateOnMalloc<DxcDiaTableSegmentMap>(pMalloc, pSession); break;
  case DiaTableKind::InjectedSource: *ppTable = CreateOnMalloc<DxcDiaTableInjectedSource>(pMalloc, pSession); break;
  case DiaTableKind::FrameData: *ppTable = CreateOnMalloc<DxcDiaTableFrameData>(pMalloc, pSession); break;
  case DiaTableKind::InputAssemblyFile: *ppTable = CreateOnMalloc<DxcDiaTableInputAssemblyFile>(pMalloc, pSession); break;
  default: return E_FAIL;
  }
  if (*ppTable == nullptr)
    return E_OUTOFMEMORY;
  (*ppTable)->AddRef();
  return S_OK;
}

class DxcDiaDataSource : public IDiaDataSource {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  std::shared_ptr<llvm::Module> m_module;
  std::shared_ptr<llvm::LLVMContext> m_context;
  std::shared_ptr<llvm::DebugInfoFinder> m_finder;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDiaDataSource>(this, iid, ppvObject);
  }

  DxcDiaDataSource(IMalloc *pMalloc) : m_pMalloc(pMalloc) {}
  ~DxcDiaDataSource() {
    // These are cross-referenced, so let's be explicit.
    m_finder.reset();
    m_module.reset();
    m_context.reset();
  }

  HRESULT STDMETHODCALLTYPE get_lastError(BSTR *pRetVal) override {
    *pRetVal = nullptr;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE loadDataFromPdb(_In_ LPCOLESTR pdbPath) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE loadAndValidateDataFromPdb(
    _In_ LPCOLESTR pdbPath,
    _In_ GUID *pcsig70,
    _In_ DWORD sig,
    _In_ DWORD age) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE loadDataForExe(
    _In_ LPCOLESTR executable,
    _In_ LPCOLESTR searchPath,
    _In_ IUnknown *pCallback) override {
    return E_NOTIMPL;
  }

  STDMETHODIMP loadDataFromIStream(_In_ IStream *pIStream) override {
    DxcThreadMalloc TM(m_pMalloc);
    if (m_module.get() != nullptr) {
      return E_FAIL;
    }
    m_context.reset();
    m_finder.reset();
    try {
      m_context = std::make_shared<LLVMContext>();
      MemoryBuffer *pBitcodeBuffer;
      std::unique_ptr<MemoryBuffer> pEmbeddedBuffer;
      std::unique_ptr<MemoryBuffer> pBuffer =
          getMemBufferFromStream(pIStream, "data");
      size_t bufferSize = pBuffer->getBufferSize();

      // The buffer can hold LLVM bitcode for a module, or the ILDB
      // part from a container.
      if (bufferSize < sizeof(UINT32)) {
        return DXC_E_MALFORMED_CONTAINER;
      }
      const UINT32 BC_C0DE = ((INT32)(INT8)'B' | (INT32)(INT8)'C' << 8 | (INT32)0xDEC0 << 16); // BC0xc0de in big endian
      if (BC_C0DE == *(const UINT32*)pBuffer->getBufferStart()) {
        pBitcodeBuffer = pBuffer.get();
      }
      else {
        if (bufferSize <= sizeof(hlsl::DxilProgramHeader)) {
          return DXC_E_MALFORMED_CONTAINER;
        }

        hlsl::DxilProgramHeader *pDxilProgramHeader = (hlsl::DxilProgramHeader *)pBuffer->getBufferStart();
        if (pDxilProgramHeader->BitcodeHeader.DxilMagic != DxilMagicValue) {
          return DXC_E_MALFORMED_CONTAINER;
        }

        UINT32 BlobSize;
        const char *pBitcode = nullptr;
        hlsl::GetDxilProgramBitcode(pDxilProgramHeader, &pBitcode, &BlobSize);
        UINT32 offset = (UINT32)(pBitcode - (const char *)pDxilProgramHeader);
        std::unique_ptr<MemoryBuffer> p = MemoryBuffer::getMemBuffer(
            StringRef(pBitcode, bufferSize - offset), "data");
        pEmbeddedBuffer.swap(p);
        pBitcodeBuffer = pEmbeddedBuffer.get();
      }

      std::string DiagStr;
      std::unique_ptr<llvm::Module> pModule = dxilutil::LoadModuleFromBitcode(
          pBitcodeBuffer, *m_context.get(), DiagStr);
      if (!pModule.get())
        return E_FAIL;
      m_finder = std::make_shared<DebugInfoFinder>();
      m_finder->processModule(*pModule.get());
      m_module.reset(pModule.release());
    }
    CATCH_CPP_RETURN_HRESULT();
    return S_OK;
  }

  STDMETHODIMP openSession(_COM_Outptr_ IDiaSession **ppSession) override {
    DxcThreadMalloc TM(m_pMalloc);
    *ppSession = nullptr;
    if (m_module.get() == nullptr)
      return E_FAIL;
    CComPtr<DxcDiaSession> pSession = DxcDiaSession::Alloc(DxcGetThreadMallocNoRef());
    IFROOM(pSession.p);
    pSession->Init(m_context, m_module, m_finder);
    *ppSession = pSession.Detach();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE loadDataFromCodeViewInfo(
    _In_ LPCOLESTR executable,
    _In_ LPCOLESTR searchPath,
    _In_ DWORD cbCvInfo,
    _In_ BYTE *pbCvInfo,
    _In_ IUnknown *pCallback) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE loadDataFromMiscInfo(
    _In_ LPCOLESTR executable,
    _In_ LPCOLESTR searchPath,
    _In_ DWORD timeStampExe,
    _In_ DWORD timeStampDbg,
    _In_ DWORD sizeOfExe,
    _In_ DWORD cbMiscInfo,
    _In_ BYTE *pbMiscInfo,
    _In_ IUnknown *pCallback) override {
    return E_NOTIMPL;
  }
};

HRESULT CreateDxcDiaDataSource(_In_ REFIID riid, _Out_ LPVOID* ppv) {
  CComPtr<DxcDiaDataSource> result = CreateOnMalloc<DxcDiaDataSource>(DxcGetThreadMallocNoRef());
  if (result == nullptr) {
    *ppv = nullptr;
    return E_OUTOFMEMORY;
  }

  return result.p->QueryInterface(riid, ppv);
}
