///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CompilerTest.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides tests for the compiler API.                                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef UNICODE
#define UNICODE
#endif

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <cassert>
#include <sstream>
#include <algorithm>
#include <cfloat>
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"
#include "dxc/dxcpix.h"
#ifdef _WIN32
#include <atlfile.h>
#include "dia2.h"
#endif

#include "dxc/Test/HLSLTestData.h"
#include "dxc/Test/HlslTestUtils.h"
#include "dxc/Test/DxcTestUtils.h"

#include "llvm/Support/raw_os_ostream.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/Unicode.h"

#include <fstream>
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringSwitch.h"

using namespace std;
using namespace hlsl_test;

// Aligned to SymTagEnum.
const char *SymTagEnumText[] =
{
  "Null", // SymTagNull
  "Exe", // SymTagExe
  "Compiland", // SymTagCompiland
  "CompilandDetails", // SymTagCompilandDetails
  "CompilandEnv", // SymTagCompilandEnv
  "Function", // SymTagFunction
  "Block", // SymTagBlock
  "Data", // SymTagData
  "Annotation", // SymTagAnnotation
  "Label", // SymTagLabel
  "PublicSymbol", // SymTagPublicSymbol
  "UDT", // SymTagUDT
  "Enum", // SymTagEnum
  "FunctionType", // SymTagFunctionType
  "PointerType", // SymTagPointerType
  "ArrayType", // SymTagArrayType
  "BaseType", // SymTagBaseType
  "Typedef", // SymTagTypedef
  "BaseClass", // SymTagBaseClass
  "Friend", // SymTagFriend
  "FunctionArgType", // SymTagFunctionArgType
  "FuncDebugStart", // SymTagFuncDebugStart
  "FuncDebugEnd", // SymTagFuncDebugEnd
  "UsingNamespace", // SymTagUsingNamespace
  "VTableShape", // SymTagVTableShape
  "VTable", // SymTagVTable
  "Custom", // SymTagCustom
  "Thunk", // SymTagThunk
  "CustomType", // SymTagCustomType
  "ManagedType", // SymTagManagedType
  "Dimension", // SymTagDimension
  "CallSite", // SymTagCallSite
  "InlineSite", // SymTagInlineSite
  "BaseInterface", // SymTagBaseInterface
  "VectorType", // SymTagVectorType
  "MatrixType", // SymTagMatrixType
  "HLSLType", // SymTagHLSLType
  "Caller", // SymTagCaller
  "Callee", // SymTagCallee
  "Export", // SymTagExport
  "HeapAllocationSite", // SymTagHeapAllocationSite
  "CoffGroup", // SymTagCoffGroup
};

// Aligned to LocationType.
const char *LocationTypeText[] =
{
  "Null",
  "Static",
  "TLS",
  "RegRel",
  "ThisRel",
  "Enregistered",
  "BitField",
  "Slot",
  "IlRel",
  "MetaData",
  "Constant",
};

// Aligned to DataKind.
const char *DataKindText[] =
{
  "Unknown",
  "Local",
  "StaticLocal",
  "Param",
  "ObjectPtr",
  "FileStatic",
  "Global",
  "Member",
  "StaticMember",
  "Constant",
};

// Aligned to UdtKind.
const char *UdtKindText[] =
{
  "Struct",
  "Class",
  "Union",
  "Interface",
};

class TestIncludeHandler : public IDxcIncludeHandler {
  DXC_MICROCOM_REF_FIELD(m_dwRef)
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  dxc::DxcDllSupport &m_dllSupport;
  HRESULT m_defaultErrorCode = E_FAIL;
  TestIncludeHandler(dxc::DxcDllSupport &dllSupport) : m_dwRef(0), m_dllSupport(dllSupport), callIndex(0) { }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override {
    return DoBasicQueryInterface<IDxcIncludeHandler>(this,  iid, ppvObject);
  }

  struct LoadSourceCallInfo {
    std::wstring Filename;     // Filename as written in #include statement
    LoadSourceCallInfo(LPCWSTR pFilename) :
      Filename(pFilename) { }
  };
  std::vector<LoadSourceCallInfo> CallInfos;
  std::wstring GetAllFileNames() const {
    std::wstringstream s;
    for (size_t i = 0; i < CallInfos.size(); ++i) {
      s << CallInfos[i].Filename << ';';
    }
    return s.str();
  }
  struct LoadSourceCallResult {
    HRESULT hr;
    std::string source;
    UINT32 codePage;
    LoadSourceCallResult() : hr(E_FAIL), codePage(0) { }
    LoadSourceCallResult(const char *pSource, UINT32 codePage = CP_UTF8) : hr(S_OK), source(pSource), codePage(codePage) { }
  };
  std::vector<LoadSourceCallResult> CallResults;
  size_t callIndex;

  HRESULT STDMETHODCALLTYPE LoadSource(
    _In_ LPCWSTR pFilename,                   // Filename as written in #include statement
    _COM_Outptr_ IDxcBlob **ppIncludeSource   // Resultant source object for included file
    ) override {
    CallInfos.push_back(LoadSourceCallInfo(pFilename));

    *ppIncludeSource = nullptr;
    if (callIndex >= CallResults.size()) {
      return m_defaultErrorCode;
    }
    if (FAILED(CallResults[callIndex].hr)) {
      return CallResults[callIndex++].hr;
    }
    MultiByteStringToBlob(m_dllSupport, CallResults[callIndex].source,
                          CallResults[callIndex].codePage, ppIncludeSource);
    return CallResults[callIndex++].hr;
  }
};

#ifdef _WIN32
class CompilerTest {
#else
class CompilerTest : public ::testing::Test {
#endif
public:
  BEGIN_TEST_CLASS(CompilerTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  TEST_METHOD(CompileWhenDebugThenDIPresent)
  TEST_METHOD(CompileDebugLines)
  TEST_METHOD(CompileDebugPDB)
  TEST_METHOD(CompileDebugDisasmPDB)

  TEST_METHOD(CompileWhenDefinesThenApplied)
  TEST_METHOD(CompileWhenDefinesManyThenApplied)
  TEST_METHOD(CompileWhenEmptyThenFails)
  TEST_METHOD(CompileWhenIncorrectThenFails)
  TEST_METHOD(CompileWhenWorksThenDisassembleWorks)
  TEST_METHOD(CompileWhenDebugWorksThenStripDebug)
  TEST_METHOD(CompileWhenWorksThenAddRemovePrivate)
  TEST_METHOD(CompileThenAddCustomDebugName)
  TEST_METHOD(CompileWithRootSignatureThenStripRootSignature)

  TEST_METHOD(CompileWhenIncludeThenLoadInvoked)
  TEST_METHOD(CompileWhenIncludeThenLoadUsed)
  TEST_METHOD(CompileWhenIncludeAbsoluteThenLoadAbsolute)
  TEST_METHOD(CompileWhenIncludeLocalThenLoadRelative)
  TEST_METHOD(CompileWhenIncludeSystemThenLoadNotRelative)
  TEST_METHOD(CompileWhenIncludeSystemMissingThenLoadAttempt)
  TEST_METHOD(CompileWhenIncludeFlagsThenIncludeUsed)
  TEST_METHOD(CompileWhenIncludeMissingThenFail)
  TEST_METHOD(CompileWhenIncludeHasPathThenOK)
  TEST_METHOD(CompileWhenIncludeEmptyThenOK)

  TEST_METHOD(CompileWhenODumpThenPassConfig)
  TEST_METHOD(CompileWhenODumpThenOptimizerMatch)
  TEST_METHOD(CompileWhenVdThenProducesDxilContainer)

#if _ITERATOR_DEBUG_LEVEL==0 
  // CompileWhenNoMemThenOOM can properly detect leaks only when debug iterators are disabled
  BEGIN_TEST_METHOD(CompileWhenNoMemThenOOM)
    // Disabled because there are problems where we try to allocate memory in destructors,
    // which causes more bad_alloc() throws while unwinding bad_alloc(), which asserts
    // If only failing one allocation, there are allocations where failing them is lost,
    // such as in ~raw_string_ostream(), where it flushes, then eats bad_alloc(), if thrown.
    TEST_METHOD_PROPERTY(L"Ignore", L"true")
  END_TEST_METHOD()
#endif
  TEST_METHOD(CompileWhenShaderModelMismatchAttributeThenFail)
  TEST_METHOD(CompileBadHlslThenFail)
  TEST_METHOD(CompileLegacyShaderModelThenFail)
  TEST_METHOD(CompileWhenRecursiveAlbeitStaticTermThenFail)

  TEST_METHOD(CompileWhenRecursiveThenFail)

  TEST_METHOD(CompileHlsl2015ThenFail)
  TEST_METHOD(CompileHlsl2016ThenOK)
  TEST_METHOD(CompileHlsl2017ThenOK)
  TEST_METHOD(CompileHlsl2018ThenOK)
  TEST_METHOD(CompileHlsl2019ThenFail)

  TEST_METHOD(DiaLoadBadBitcodeThenFail)
  TEST_METHOD(DiaLoadDebugThenOK)
  TEST_METHOD(DiaTableIndexThenOK)
  TEST_METHOD(DiaLoadDebugSubrangeNegativeThenOK)
  TEST_METHOD(DiaLoadRelocatedBitcode)
  TEST_METHOD(DiaLoadBitcodePlusExtraData)
  TEST_METHOD(DiaCompileArgs)

  TEST_METHOD(PixDebugCompileInfo)

  TEST_METHOD(CodeGenFloatingPointEnvironment)
  TEST_METHOD(CodeGenInclude)
  TEST_METHOD(CodeGenLibCsEntry)
  TEST_METHOD(CodeGenLibCsEntry2)
  TEST_METHOD(CodeGenLibCsEntry3)
  TEST_METHOD(CodeGenLibEntries)
  TEST_METHOD(CodeGenLibEntries2)
  TEST_METHOD(CodeGenLibNoAlias)
  TEST_METHOD(CodeGenLibResource)
  TEST_METHOD(CodeGenLibUnusedFunc)

  TEST_METHOD(CodeGenRootSigProfile)
  TEST_METHOD(CodeGenRootSigProfile2)
  TEST_METHOD(CodeGenRootSigProfile5)
  TEST_METHOD(PreprocessWhenValidThenOK)
  TEST_METHOD(LibGVStore)
  TEST_METHOD(PreprocessWhenExpandTokenPastingOperandThenAccept)
  TEST_METHOD(WhenSigMismatchPCFunctionThenFail)

  TEST_METHOD(BatchSamples)
  TEST_METHOD(BatchD3DReflect)
  TEST_METHOD(BatchDxil)
  TEST_METHOD(BatchHLSL)
  TEST_METHOD(BatchInfra)
  TEST_METHOD(BatchPasses)
  TEST_METHOD(BatchShaderTargets)
  TEST_METHOD(BatchValidation)

  TEST_METHOD(SubobjectCodeGenErrors)
  BEGIN_TEST_METHOD(ManualFileCheckTest)
    TEST_METHOD_PROPERTY(L"Ignore", L"true")
  END_TEST_METHOD()

  // Batch directories
  BEGIN_TEST_METHOD(CodeGenHashStability)
      TEST_METHOD_PROPERTY(L"Priority", L"2")
  END_TEST_METHOD()

  dxc::DxcDllSupport m_dllSupport;
  VersionSupportInfo m_ver;

  void CreateBlobPinned(_In_bytecount_(size) LPCVOID data, SIZE_T size,
                        UINT32 codePage, _Outptr_ IDxcBlobEncoding **ppBlob) {
    CComPtr<IDxcLibrary> library;
    IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
    IFT(library->CreateBlobWithEncodingFromPinned(data, size, codePage,
                                                  ppBlob));
  }

  void CreateBlobFromFile(LPCWSTR name, _Outptr_ IDxcBlobEncoding **ppBlob) {
    CComPtr<IDxcLibrary> library;
    IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
    const std::wstring path = hlsl_test::GetPathToHlslDataFile(name);
    IFT(library->CreateBlobFromFile(path.c_str(), nullptr, ppBlob));
  }

  void CreateBlobFromText(_In_z_ const char *pText,
                          _Outptr_ IDxcBlobEncoding **ppBlob) {
    CreateBlobPinned(pText, strlen(pText) + 1, CP_UTF8, ppBlob);
  }

  HRESULT CreateCompiler(IDxcCompiler **ppResult) {
    return m_dllSupport.CreateInstance(CLSID_DxcCompiler, ppResult);
  }

#ifdef _WIN32 // No ContainerBuilder support yet
  HRESULT CreateContainerBuilder(IDxcContainerBuilder **ppResult) {
    return m_dllSupport.CreateInstance(CLSID_DxcContainerBuilder, ppResult);
  }
#endif

  template <typename T, typename TDefault, typename TIface>
  void WriteIfValue(TIface *pSymbol, std::wstringstream &o,
                    TDefault defaultValue, LPCWSTR valueLabel,
                    HRESULT (__stdcall TIface::*pFn)(T *)) {
    T value;
    HRESULT hr = (pSymbol->*(pFn))(&value);
    if (SUCCEEDED(hr) && value != defaultValue) {
      o << L", " << valueLabel << L": " << value;
    }
  }
#ifdef _WIN32 // exclude dia stuff
  template <typename TIface>
  void WriteIfValue(TIface *pSymbol, std::wstringstream &o,
    LPCWSTR valueLabel, HRESULT(__stdcall TIface::*pFn)(BSTR *)) {
    CComBSTR value;
    HRESULT hr = (pSymbol->*(pFn))(&value);
    if (SUCCEEDED(hr) && value.Length()) {
      o << L", " << valueLabel << L": " << (LPCWSTR)value;
    }
  }
  template <typename TIface>
  void WriteIfValue(TIface *pSymbol, std::wstringstream &o,
    LPCWSTR valueLabel, HRESULT(__stdcall TIface::*pFn)(VARIANT *)) {
    CComVariant value;
    HRESULT hr = (pSymbol->*(pFn))(&value);
    if (SUCCEEDED(hr) && value.vt != VT_NULL && value.vt != VT_EMPTY) {
      if (SUCCEEDED(value.ChangeType(VT_BSTR))) {
        o << L", " << valueLabel << L": " << (LPCWSTR)value.bstrVal;
      }
    }
  }
  template <typename TIface>
  void WriteIfValue(TIface *pSymbol, std::wstringstream &o,
    LPCWSTR valueLabel, HRESULT(__stdcall TIface::*pFn)(IDiaSymbol **)) {
    CComPtr<IDiaSymbol> value;
    HRESULT hr = (pSymbol->*(pFn))(&value);
    if (SUCCEEDED(hr) && value.p != nullptr) {
      DWORD symId;
      value->get_symIndexId(&symId);
      o << L", " << valueLabel << L": id=" << symId;
    }
  }

  std::wstring GetDebugInfoAsText(_In_ IDiaDataSource* pDataSource) {
    CComPtr<IDiaSession> pSession;
    CComPtr<IDiaTable> pTable;
    CComPtr<IDiaEnumTables> pEnumTables;
    std::wstringstream o;

    VERIFY_SUCCEEDED(pDataSource->openSession(&pSession));
    VERIFY_SUCCEEDED(pSession->getEnumTables(&pEnumTables));
    LONG count;
    VERIFY_SUCCEEDED(pEnumTables->get_Count(&count));
    for (LONG i = 0; i < count; ++i) {
      pTable.Release();
      ULONG fetched;
      VERIFY_SUCCEEDED(pEnumTables->Next(1, &pTable, &fetched));
      VERIFY_ARE_EQUAL(fetched, 1);
      CComBSTR tableName;
      VERIFY_SUCCEEDED(pTable->get_name(&tableName));
      o << L"Table: " << (LPWSTR)tableName << std::endl;
      LONG rowCount;
      IFT(pTable->get_Count(&rowCount));
      o << L" Row count: " << rowCount << std::endl;

      for (LONG rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
        CComPtr<IUnknown> item;
        o << L'#' << rowIndex;
        IFT(pTable->Item(rowIndex, &item));
        CComPtr<IDiaSymbol> pSymbol;
        if (SUCCEEDED(item.QueryInterface(&pSymbol))) {
          DWORD symTag;
          DWORD dataKind;
          DWORD locationType;
          DWORD registerId;
          pSymbol->get_symTag(&symTag);
          pSymbol->get_dataKind(&dataKind);
          pSymbol->get_locationType(&locationType);
          pSymbol->get_registerId(&registerId);
          //pSymbol->get_value(&value);

          WriteIfValue(pSymbol.p, o, 0, L"symIndexId", &IDiaSymbol::get_symIndexId);
          o << L", " << SymTagEnumText[symTag];
          if (dataKind != 0) o << L", " << DataKindText[dataKind];
          WriteIfValue(pSymbol.p, o, L"name", &IDiaSymbol::get_name);
          WriteIfValue(pSymbol.p, o, L"lexicalParent", &IDiaSymbol::get_lexicalParent);
          WriteIfValue(pSymbol.p, o, L"type", &IDiaSymbol::get_type);
          WriteIfValue(pSymbol.p, o, 0, L"slot", &IDiaSymbol::get_slot);
          WriteIfValue(pSymbol.p, o, 0, L"platform", &IDiaSymbol::get_platform);
          WriteIfValue(pSymbol.p, o, 0, L"language", &IDiaSymbol::get_language);
          WriteIfValue(pSymbol.p, o, 0, L"frontEndMajor", &IDiaSymbol::get_frontEndMajor);
          WriteIfValue(pSymbol.p, o, 0, L"frontEndMinor", &IDiaSymbol::get_frontEndMinor);
          WriteIfValue(pSymbol.p, o, 0, L"token", &IDiaSymbol::get_token);
          WriteIfValue(pSymbol.p, o,    L"value", &IDiaSymbol::get_value);
          WriteIfValue(pSymbol.p, o, 0, L"code", &IDiaSymbol::get_code);
          WriteIfValue(pSymbol.p, o, 0, L"function", &IDiaSymbol::get_function);
          WriteIfValue(pSymbol.p, o, 0, L"udtKind", &IDiaSymbol::get_udtKind);
          WriteIfValue(pSymbol.p, o, 0, L"hasDebugInfo", &IDiaSymbol::get_hasDebugInfo);
          WriteIfValue(pSymbol.p, o,    L"compilerName", &IDiaSymbol::get_compilerName);
          WriteIfValue(pSymbol.p, o, 0, L"isLocationControlFlowDependent", &IDiaSymbol::get_isLocationControlFlowDependent);
          WriteIfValue(pSymbol.p, o, 0, L"numberOfRows", &IDiaSymbol::get_numberOfRows);
          WriteIfValue(pSymbol.p, o, 0, L"numberOfColumns", &IDiaSymbol::get_numberOfColumns);
          WriteIfValue(pSymbol.p, o, 0, L"length", &IDiaSymbol::get_length);
          WriteIfValue(pSymbol.p, o, 0, L"isMatrixRowMajor", &IDiaSymbol::get_isMatrixRowMajor);
          WriteIfValue(pSymbol.p, o, 0, L"builtInKind", &IDiaSymbol::get_builtInKind);
          WriteIfValue(pSymbol.p, o, 0, L"textureSlot", &IDiaSymbol::get_textureSlot);
          WriteIfValue(pSymbol.p, o, 0, L"memorySpaceKind", &IDiaSymbol::get_memorySpaceKind);
          WriteIfValue(pSymbol.p, o, 0, L"isHLSLData", &IDiaSymbol::get_isHLSLData);
        }

        CComPtr<IDiaSourceFile> pSourceFile;
        if (SUCCEEDED(item.QueryInterface(&pSourceFile))) {
          WriteIfValue(pSourceFile.p, o, 0, L"uniqueId", &IDiaSourceFile::get_uniqueId);
          WriteIfValue(pSourceFile.p, o, L"fileName", &IDiaSourceFile::get_fileName);
        }

        CComPtr<IDiaLineNumber> pLineNumber;
        if (SUCCEEDED(item.QueryInterface(&pLineNumber))) {
          WriteIfValue(pLineNumber.p, o, L"compiland", &IDiaLineNumber::get_compiland);
          //WriteIfValue(pLineNumber.p, o, L"sourceFile", &IDiaLineNumber::get_sourceFile);
          WriteIfValue(pLineNumber.p, o, 0, L"lineNumber", &IDiaLineNumber::get_lineNumber);
          WriteIfValue(pLineNumber.p, o, 0, L"lineNumberEnd", &IDiaLineNumber::get_lineNumberEnd);
          WriteIfValue(pLineNumber.p, o, 0, L"columnNumber", &IDiaLineNumber::get_columnNumber);
          WriteIfValue(pLineNumber.p, o, 0, L"columnNumberEnd", &IDiaLineNumber::get_columnNumberEnd);
          WriteIfValue(pLineNumber.p, o, 0, L"addressSection", &IDiaLineNumber::get_addressSection);
          WriteIfValue(pLineNumber.p, o, 0, L"addressOffset", &IDiaLineNumber::get_addressOffset);
          WriteIfValue(pLineNumber.p, o, 0, L"relativeVirtualAddress", &IDiaLineNumber::get_relativeVirtualAddress);
          WriteIfValue(pLineNumber.p, o, 0, L"virtualAddress", &IDiaLineNumber::get_virtualAddress);
          WriteIfValue(pLineNumber.p, o, 0, L"length", &IDiaLineNumber::get_length);
          WriteIfValue(pLineNumber.p, o, 0, L"sourceFileId", &IDiaLineNumber::get_sourceFileId);
          WriteIfValue(pLineNumber.p, o, 0, L"statement", &IDiaLineNumber::get_statement);
          WriteIfValue(pLineNumber.p, o, 0, L"compilandId", &IDiaLineNumber::get_compilandId);
        }

        CComPtr<IDiaSectionContrib> pSectionContrib;
        if (SUCCEEDED(item.QueryInterface(&pSectionContrib))) {
          WriteIfValue(pSectionContrib.p, o, L"compiland", &IDiaSectionContrib::get_compiland);
          WriteIfValue(pSectionContrib.p, o, 0, L"addressSection", &IDiaSectionContrib::get_addressSection);
          WriteIfValue(pSectionContrib.p, o, 0, L"addressOffset", &IDiaSectionContrib::get_addressOffset);
          WriteIfValue(pSectionContrib.p, o, 0, L"relativeVirtualAddress", &IDiaSectionContrib::get_relativeVirtualAddress);
          WriteIfValue(pSectionContrib.p, o, 0, L"virtualAddress", &IDiaSectionContrib::get_virtualAddress);
          WriteIfValue(pSectionContrib.p, o, 0, L"length", &IDiaSectionContrib::get_length);
          WriteIfValue(pSectionContrib.p, o, 0, L"notPaged", &IDiaSectionContrib::get_notPaged);
          WriteIfValue(pSectionContrib.p, o, 0, L"code", &IDiaSectionContrib::get_code);
          WriteIfValue(pSectionContrib.p, o, 0, L"initializedData", &IDiaSectionContrib::get_initializedData);
          WriteIfValue(pSectionContrib.p, o, 0, L"uninitializedData", &IDiaSectionContrib::get_uninitializedData);
          WriteIfValue(pSectionContrib.p, o, 0, L"remove", &IDiaSectionContrib::get_remove);
          WriteIfValue(pSectionContrib.p, o, 0, L"comdat", &IDiaSectionContrib::get_comdat);
          WriteIfValue(pSectionContrib.p, o, 0, L"discardable", &IDiaSectionContrib::get_discardable);
          WriteIfValue(pSectionContrib.p, o, 0, L"notCached", &IDiaSectionContrib::get_notCached);
          WriteIfValue(pSectionContrib.p, o, 0, L"share", &IDiaSectionContrib::get_share);
          WriteIfValue(pSectionContrib.p, o, 0, L"execute", &IDiaSectionContrib::get_execute);
          WriteIfValue(pSectionContrib.p, o, 0, L"read", &IDiaSectionContrib::get_read);
          WriteIfValue(pSectionContrib.p, o, 0, L"write", &IDiaSectionContrib::get_write);
          WriteIfValue(pSectionContrib.p, o, 0, L"dataCrc", &IDiaSectionContrib::get_dataCrc);
          WriteIfValue(pSectionContrib.p, o, 0, L"relocationsCrc", &IDiaSectionContrib::get_relocationsCrc);
          WriteIfValue(pSectionContrib.p, o, 0, L"compilandId", &IDiaSectionContrib::get_compilandId);
        }

        CComPtr<IDiaSegment> pSegment;
        if (SUCCEEDED(item.QueryInterface(&pSegment))) {
          WriteIfValue(pSegment.p, o, 0, L"frame", &IDiaSegment::get_frame);
          WriteIfValue(pSegment.p, o, 0, L"offset", &IDiaSegment::get_offset);
          WriteIfValue(pSegment.p, o, 0, L"length", &IDiaSegment::get_length);
          WriteIfValue(pSegment.p, o, 0, L"read", &IDiaSegment::get_read);
          WriteIfValue(pSegment.p, o, 0, L"write", &IDiaSegment::get_write);
          WriteIfValue(pSegment.p, o, 0, L"execute", &IDiaSegment::get_execute);
          WriteIfValue(pSegment.p, o, 0, L"addressSection", &IDiaSegment::get_addressSection);
          WriteIfValue(pSegment.p, o, 0, L"relativeVirtualAddress", &IDiaSegment::get_relativeVirtualAddress);
          WriteIfValue(pSegment.p, o, 0, L"virtualAddress", &IDiaSegment::get_virtualAddress);
        }

        CComPtr<IDiaInjectedSource> pInjectedSource;
        if (SUCCEEDED(item.QueryInterface(&pInjectedSource))) {
          WriteIfValue(pInjectedSource.p, o, 0, L"crc", &IDiaInjectedSource::get_crc);
          WriteIfValue(pInjectedSource.p, o, 0, L"length", &IDiaInjectedSource::get_length);
          WriteIfValue(pInjectedSource.p, o, L"filename", &IDiaInjectedSource::get_filename);
          WriteIfValue(pInjectedSource.p, o, L"objectFilename", &IDiaInjectedSource::get_objectFilename);
          WriteIfValue(pInjectedSource.p, o, L"virtualFilename", &IDiaInjectedSource::get_virtualFilename);
          WriteIfValue(pInjectedSource.p, o, 0, L"sourceCompression", &IDiaInjectedSource::get_sourceCompression);
          // get_source is also available
        }

        CComPtr<IDiaFrameData> pFrameData;
        if (SUCCEEDED(item.QueryInterface(&pFrameData))) {
        }

        o << std::endl;
      }
    }

    return o.str();
  }
  std::wstring GetDebugFileContent(_In_ IDiaDataSource *pDataSource) {
    CComPtr<IDiaSession> pSession;
    CComPtr<IDiaTable> pTable;

    CComPtr<IDiaTable> pSourcesTable;

    CComPtr<IDiaEnumTables> pEnumTables;
    std::wstringstream o;

    VERIFY_SUCCEEDED(pDataSource->openSession(&pSession));
    VERIFY_SUCCEEDED(pSession->getEnumTables(&pEnumTables));

    ULONG fetched = 0;
    while (pEnumTables->Next(1, &pTable, &fetched) == S_OK && fetched == 1) {
      CComBSTR name;
      IFT(pTable->get_name(&name));

      if (wcscmp(name, L"SourceFiles") == 0) {
        pSourcesTable = pTable.Detach();
        continue;
      }

      pTable.Release();
    }

    if (!pSourcesTable) {
      return L"cannot find source";
    }

    // Get source file contents.
    // NOTE: "SourceFiles" has the root file first while "InjectedSources" is in
    // alphabetical order.
    //       It is important to keep the root file first for recompilation, so
    //       iterate "SourceFiles" and look up the corresponding injected
    //       source.
    LONG count;
    IFT(pSourcesTable->get_Count(&count));

    CComPtr<IDiaSourceFile> pSourceFile;
    CComBSTR pName;
    CComPtr<IUnknown> pSymbolUnk;
    CComPtr<IDiaEnumInjectedSources> pEnumInjectedSources;
    CComPtr<IDiaInjectedSource> pInjectedSource;
    std::wstring sourceText, sourceFilename;

    while (SUCCEEDED(pSourcesTable->Next(1, &pSymbolUnk, &fetched)) &&
           fetched == 1) {
      sourceText = sourceFilename = L"";

      IFT(pSymbolUnk->QueryInterface(&pSourceFile));
      IFT(pSourceFile->get_fileName(&pName));

      IFT(pSession->findInjectedSource(pName, &pEnumInjectedSources));

      if (SUCCEEDED(pEnumInjectedSources->get_Count(&count)) && count == 1) {
        IFT(pEnumInjectedSources->Item(0, &pInjectedSource));

        DWORD cbData = 0;
        std::string tempString;
        CComBSTR bstr;
        IFT(pInjectedSource->get_filename(&bstr));
        IFT(pInjectedSource->get_source(0, &cbData, nullptr));

        tempString.resize(cbData);
        IFT(pInjectedSource->get_source(
            cbData, &cbData, reinterpret_cast<BYTE *>(&tempString[0])));

        CA2W tempWString(tempString.data());
        o << tempWString.m_psz;
      }
      pSymbolUnk.Release();
    }

    return o.str();
  }

  struct LineNumber { DWORD line; DWORD rva; };
  std::vector<LineNumber> ReadLineNumbers(IDiaEnumLineNumbers *pEnumLineNumbers) {
    std::vector<LineNumber> lines;
    CComPtr<IDiaLineNumber> pLineNumber;
    DWORD lineCount;
    while (SUCCEEDED(pEnumLineNumbers->Next(1, &pLineNumber, &lineCount)) && lineCount == 1)
    {
      DWORD line;
      DWORD rva;
      VERIFY_SUCCEEDED(pLineNumber->get_lineNumber(&line));
      VERIFY_SUCCEEDED(pLineNumber->get_relativeVirtualAddress(&rva));
      lines.push_back({ line, rva });
      pLineNumber.Release();
    }
    return lines;
  }
#endif //  _WIN32 - exclude dia stuff
 
  std::string GetOption(std::string &cmd, char *opt) {
    std::string option = cmd.substr(cmd.find(opt));
    option = option.substr(option.find_first_of(' '));
    option = option.substr(option.find_first_not_of(' '));
    return option.substr(0, option.find_first_of(' '));
  }

  void CodeGenTest(std::wstring name) {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;

    name.insert(0, L"..\\CodeGenHLSL\\");

    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
    CreateBlobFromFile(name.c_str(), &pSource);

    std::string cmdLine = GetFirstLine(name.c_str());

    llvm::StringRef argsRef = cmdLine;
    llvm::SmallVector<llvm::StringRef, 8> splitArgs;
    argsRef.split(splitArgs, " ");
    hlsl::options::MainArgs argStrings(splitArgs);
    std::string errorString;
    llvm::raw_string_ostream errorStream(errorString);
    hlsl::options::DxcOpts opts;
    IFT(ReadDxcOpts(hlsl::options::getHlslOptTable(), /*flagsToInclude*/ 0,
                    argStrings, opts, errorStream));
    std::wstring entry =
        Unicode::UTF8ToUTF16StringOrThrow(opts.EntryPoint.str().c_str());
    std::wstring profile =
        Unicode::UTF8ToUTF16StringOrThrow(opts.TargetProfile.str().c_str());

    std::vector<std::wstring> argLists;
    CopyArgsToWStrings(opts.Args, hlsl::options::CoreOption, argLists);

    std::vector<LPCWSTR> args;
    args.reserve(argLists.size());
    for (const std::wstring &a : argLists)
      args.push_back(a.data());

    VERIFY_SUCCEEDED(pCompiler->Compile(
        pSource, name.c_str(), entry.c_str(), profile.c_str(), args.data(), args.size(),
        opts.Defines.data(), opts.Defines.size(), nullptr, &pResult));
    VERIFY_IS_NOT_NULL(pResult, L"Failed to compile - pResult NULL");
    HRESULT result;
    VERIFY_SUCCEEDED(pResult->GetStatus(&result));
    if (FAILED(result)) {
      CComPtr<IDxcBlobEncoding> pErr;
      IFT(pResult->GetErrorBuffer(&pErr));
      std::string errString(BlobToUtf8(pErr));
      CA2W errStringW(errString.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(L"Failed to compile - errors follow");
      WEX::Logging::Log::Comment(errStringW);
    }
    VERIFY_SUCCEEDED(result);

    CComPtr<IDxcBlob> pProgram;
    VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
    if (opts.IsRootSignatureProfile())
      return;

    CComPtr<IDxcBlobEncoding> pDisassembleBlob;
    VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembleBlob));

    std::string disassembleString(BlobToUtf8(pDisassembleBlob));
    VERIFY_ARE_NOT_EQUAL(0U, disassembleString.size());
  }
  
  void CodeGenTestHashFullPath(LPCWSTR fullPath) {
    FileRunTestResult t = FileRunTestResult::RunHashTestFromFileCommands(fullPath);
    if (t.RunResult != 0) {
      CA2W commentWide(t.ErrorMessage.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(commentWide);
      WEX::Logging::Log::Error(L"Run result is not zero");
    }
  }

  void CodeGenTestHash(LPCWSTR name, bool implicitDir) {
    std::wstring path = name;
    if (implicitDir) {
      path.insert(0, L"..\\CodeGenHLSL\\");
      path = hlsl_test::GetPathToHlslDataFile(path.c_str());
    }
    CodeGenTestHashFullPath(path.c_str());
  }

  void CodeGenTestCheckBatchHash(std::wstring suitePath, bool implicitDir = true) {
    using namespace llvm;
    using namespace WEX::TestExecution;

    if (implicitDir) suitePath.insert(0, L"..\\HLSLFileCheck\\");

    ::llvm::sys::fs::MSFileSystem *msfPtr;
    VERIFY_SUCCEEDED(CreateMSFileSystemForDisk(&msfPtr));
    std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
    ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    IFTLLVM(pts.error_code());

    CW2A pUtf8Filename(suitePath.c_str());
    if (!llvm::sys::path::is_absolute(pUtf8Filename.m_psz)) {
      suitePath = hlsl_test::GetPathToHlslDataFile(suitePath.c_str());
    }

    CW2A utf8SuitePath(suitePath.c_str());

    unsigned numTestsRun = 0;

    std::error_code EC;
    llvm::SmallString<128> DirNative;
    llvm::sys::path::native(utf8SuitePath.m_psz, DirNative);
    for (llvm::sys::fs::recursive_directory_iterator Dir(DirNative, EC), DirEnd;
         Dir != DirEnd && !EC; Dir.increment(EC)) {
      // Check whether this entry has an extension typically associated with
      // headers.
      if (!llvm::StringSwitch<bool>(llvm::sys::path::extension(Dir->path()))
          .Cases(".hlsl", ".ll", true).Default(false))
        continue;
      StringRef filename = Dir->path();
      std::string filetag = Dir->path();
      filetag += "<HASH>";

      CA2W wRelTag(filetag.data());
      CA2W wRelPath(filename.data());

      WEX::Logging::Log::StartGroup(wRelTag);
      CodeGenTestHash(wRelPath, /*implicitDir*/ false);
      WEX::Logging::Log::EndGroup(wRelTag);

      numTestsRun++;
    }

    VERIFY_IS_GREATER_THAN(numTestsRun, (unsigned)0, L"No test files found in batch directory.");
  }

  void CodeGenTestCheckFullPath(LPCWSTR fullPath) {
    FileRunTestResult t = FileRunTestResult::RunFromFileCommands(fullPath);
    if (t.RunResult != 0) {
      CA2W commentWide(t.ErrorMessage.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(commentWide);
      WEX::Logging::Log::Error(L"Run result is not zero");
    }
  }

  void CodeGenTestCheck(LPCWSTR name, bool implicitDir = true) {
    std::wstring path = name;
    if (implicitDir) {
      path.insert(0, L"..\\CodeGenHLSL\\");
      path = hlsl_test::GetPathToHlslDataFile(path.c_str());
    }
    CodeGenTestCheckFullPath(path.c_str());
  }

  void CodeGenTestCheckBatchDir(std::wstring suitePath, bool implicitDir = true) {
    using namespace llvm;
    using namespace WEX::TestExecution;

    if (implicitDir) suitePath.insert(0, L"..\\HLSLFileCheck\\");

    ::llvm::sys::fs::MSFileSystem *msfPtr;
    VERIFY_SUCCEEDED(CreateMSFileSystemForDisk(&msfPtr));
    std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
    ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    IFTLLVM(pts.error_code());

    CW2A pUtf8Filename(suitePath.c_str());
    if (!llvm::sys::path::is_absolute(pUtf8Filename.m_psz)) {
      suitePath = hlsl_test::GetPathToHlslDataFile(suitePath.c_str());
    }

    CW2A utf8SuitePath(suitePath.c_str());

    unsigned numTestsRun = 0;

    std::error_code EC;
    llvm::SmallString<128> DirNative;
    llvm::sys::path::native(utf8SuitePath.m_psz, DirNative);
    for (llvm::sys::fs::recursive_directory_iterator Dir(DirNative, EC), DirEnd;
         Dir != DirEnd && !EC; Dir.increment(EC)) {
      // Check whether this entry has an extension typically associated with
      // headers.
      if (!llvm::StringSwitch<bool>(llvm::sys::path::extension(Dir->path()))
          .Cases(".hlsl", ".ll", true).Default(false))
        continue;
      StringRef filename = Dir->path();
      CA2W wRelPath(filename.data());

      WEX::Logging::Log::StartGroup(wRelPath);
      CodeGenTestCheck(wRelPath, /*implicitDir*/ false);
      WEX::Logging::Log::EndGroup(wRelPath);

      numTestsRun++;
    }

    VERIFY_IS_GREATER_THAN(numTestsRun, (unsigned)0, L"No test files found in batch directory.");
  }

  std::string VerifyCompileFailed(LPCSTR pText, LPCWSTR pTargetProfile, LPCSTR pErrorMsg) {
    return VerifyCompileFailed(pText, pTargetProfile, pErrorMsg, L"main");
  }

  std::string VerifyCompileFailed(LPCSTR pText, LPCWSTR pTargetProfile, LPCSTR pErrorMsg, LPCWSTR pEntryPoint) {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcBlobEncoding> pErrors;

    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
    CreateBlobFromText(pText, &pSource);

    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", pEntryPoint,
      pTargetProfile, nullptr, 0, nullptr, 0, nullptr, &pResult));

    HRESULT status;
    VERIFY_SUCCEEDED(pResult->GetStatus(&status));
    VERIFY_FAILED(status);
    VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
    if (pErrorMsg && *pErrorMsg) {
      CheckOperationResultMsgs(pResult, &pErrorMsg, 1, false, false);
    }
    return BlobToUtf8(pErrors);
  }

  void VerifyOperationSucceeded(IDxcOperationResult *pResult) {
    HRESULT result;
    VERIFY_SUCCEEDED(pResult->GetStatus(&result));
    if (FAILED(result)) {
      CComPtr<IDxcBlobEncoding> pErrors;
      VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
      CA2W errorsWide(BlobToUtf8(pErrors).c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(errorsWide);
    }
    VERIFY_SUCCEEDED(result);
  }

  std::string VerifyOperationFailed(IDxcOperationResult *pResult) {
    HRESULT result;
    VERIFY_SUCCEEDED(pResult->GetStatus(&result));
    VERIFY_FAILED(result);
    CComPtr<IDxcBlobEncoding> pErrors;
    VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
    return BlobToUtf8(pErrors);
  }

#ifdef _WIN32 // - exclude dia stuff
  HRESULT CreateDiaSourceForCompile(const char *hlsl, IDiaDataSource **ppDiaSource)
  {
    if (!ppDiaSource)
      return E_POINTER;

    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcBlob> pProgram;

    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
    CreateBlobFromText(hlsl, &pSource);
    LPCWSTR args[] = { L"/Zi", L"/Qembed_debug" };
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
      L"ps_6_0", args, _countof(args), nullptr, 0, nullptr, &pResult));
    VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));

    // Disassemble the compiled (stripped) program.
    {
      CComPtr<IDxcBlobEncoding> pDisassembly;
      VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembly));
      std::string disText = BlobToUtf8(pDisassembly);
      CA2W disTextW(disText.c_str(), CP_UTF8);
      //WEX::Logging::Log::Comment(disTextW);
    }

    // CONSIDER: have the dia data source look for the part if passed a whole container.
    CComPtr<IDiaDataSource> pDiaSource;
    CComPtr<IStream> pProgramStream;
    CComPtr<IDxcLibrary> pLib;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));
    const hlsl::DxilContainerHeader *pContainer = hlsl::IsDxilContainerLike(
        pProgram->GetBufferPointer(), pProgram->GetBufferSize());
    VERIFY_IS_NOT_NULL(pContainer);
    hlsl::DxilPartIterator partIter =
        std::find_if(hlsl::begin(pContainer), hlsl::end(pContainer),
                     hlsl::DxilPartIsType(hlsl::DFCC_ShaderDebugInfoDXIL));
    const hlsl::DxilProgramHeader *pProgramHeader =
        (const hlsl::DxilProgramHeader *)hlsl::GetDxilPartData(*partIter);
    uint32_t bitcodeLength;
    const char *pBitcode;
    CComPtr<IDxcBlob> pProgramPdb;
    hlsl::GetDxilProgramBitcode(pProgramHeader, &pBitcode, &bitcodeLength);
    VERIFY_SUCCEEDED(pLib->CreateBlobFromBlob(
        pProgram, pBitcode - (char *)pProgram->GetBufferPointer(), bitcodeLength,
        &pProgramPdb));

    // Disassemble the program with debug information.
    {
      CComPtr<IDxcBlobEncoding> pDbgDisassembly;
      VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgramPdb, &pDbgDisassembly));
      std::string disText = BlobToUtf8(pDbgDisassembly);
      CA2W disTextW(disText.c_str(), CP_UTF8);
      //WEX::Logging::Log::Comment(disTextW);
    }

    // Create a short text dump of debug information.
    VERIFY_SUCCEEDED(pLib->CreateStreamFromBlobReadOnly(pProgramPdb, &pProgramStream));
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcDiaDataSource, &pDiaSource));
    VERIFY_SUCCEEDED(pDiaSource->loadDataFromIStream(pProgramStream));
    *ppDiaSource = pDiaSource.Detach();
    return S_OK;
  }
#endif // _WIN32 - exclude dia stuff
};

// Useful for debugging.
#if SUPPORT_FXC_PDB
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
HRESULT GetBlobPdb(IDxcBlob *pBlob, IDxcBlob **ppDebugInfo) {
  return D3DGetBlobPart(pBlob->GetBufferPointer(), pBlob->GetBufferSize(),
    D3D_BLOB_PDB, 0, (ID3DBlob **)ppDebugInfo);
}

std::string FourCCStr(uint32_t val) {
  std::stringstream o;
  char c[5];
  c[0] = val & 0xFF;
  c[1] = (val & 0xFF00) >> 8;
  c[2] = (val & 0xFF0000) >> 16;
  c[3] = (val & 0xFF000000) >> 24;
  c[4] = '\0';
  o << c << " (" << std::hex << val << std::dec << ")";
  return o.str();
}
std::string DumpParts(IDxcBlob *pBlob) {
  std::stringstream o;

  hlsl::DxilContainerHeader *pContainer = (hlsl::DxilContainerHeader *)pBlob->GetBufferPointer();
  o << "Container:" << std::endl
    << " Size: " << pContainer->ContainerSizeInBytes << std::endl
    << " FourCC: " << FourCCStr(pContainer->HeaderFourCC) << std::endl
    << " Part count: " << pContainer->PartCount << std::endl;
  for (uint32_t i = 0; i < pContainer->PartCount; ++i) {
    hlsl::DxilPartHeader *pPart = hlsl::GetDxilContainerPart(pContainer, i);
    o << "Part " << i << std::endl
      << " FourCC: " << FourCCStr(pPart->PartFourCC) << std::endl
      << " Size: " << pPart->PartSize << std::endl;
  }
  return o.str();
}

HRESULT CreateDiaSourceFromDxbcBlob(IDxcLibrary *pLib, IDxcBlob *pDxbcBlob,
                                    IDiaDataSource **ppDiaSource) {
  HRESULT hr = S_OK;
  CComPtr<IDxcBlob> pdbBlob;
  CComPtr<IStream> pPdbStream;
  CComPtr<IDiaDataSource> pDiaSource;
  IFR(GetBlobPdb(pDxbcBlob, &pdbBlob));
  IFR(pLib->CreateStreamFromBlobReadOnly(pdbBlob, &pPdbStream));
  IFR(CoCreateInstance(CLSID_DiaSource, NULL, CLSCTX_INPROC_SERVER,
                       __uuidof(IDiaDataSource), (void **)&pDiaSource));
  IFR(pDiaSource->loadDataFromIStream(pPdbStream));
  *ppDiaSource = pDiaSource.Detach();
  return hr;
}
#endif

bool CompilerTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    m_ver.Initialize(m_dllSupport);
  }
  return true;
}

#if _WIN32 // - exclude dia stuff
TEST_F(CompilerTest, CompileWhenDebugThenDIPresent) {
  // BUG: the first test written was of this form:
  // float4 local = 0; return local;
  //
  // However we get no numbers because of the _wrapper form
  // that exports the zero initialization from main into
  // a global can't be attributed to any particular location
  // within main, and everything in main is eventually folded away.
  //
  // Making the function do a bit more work by calling an intrinsic
  // helps this case.
  CComPtr<IDiaDataSource> pDiaSource;
  VERIFY_SUCCEEDED(CreateDiaSourceForCompile(
    "float4 main(float4 pos : SV_Position) : SV_Target {\r\n"
    "  float4 local = abs(pos);\r\n"
    "  return local;\r\n"
    "}", &pDiaSource));
  std::wstring diaDump = GetDebugInfoAsText(pDiaSource).c_str();
  //WEX::Logging::Log::Comment(GetDebugInfoAsText(pDiaSource).c_str());

  // Very basic tests - we have basic symbols, line numbers, and files with sources.
  VERIFY_IS_NOT_NULL(wcsstr(diaDump.c_str(), L"symIndexId: 5, CompilandEnv, name: hlslTarget, lexicalParent: id=2, value: ps_6_0"));
  VERIFY_IS_NOT_NULL(wcsstr(diaDump.c_str(), L"lineNumber: 2"));
  VERIFY_IS_NOT_NULL(wcsstr(diaDump.c_str(), L"length: 99, filename: source.hlsl"));
  std::wstring diaFileContent = GetDebugFileContent(pDiaSource).c_str();
  VERIFY_IS_NOT_NULL(wcsstr(diaFileContent.c_str(), L"loat4 main(float4 pos : SV_Position) : SV_Target"));
#if SUPPORT_FXC_PDB
  // Now, fake it by loading from a .pdb!
  VERIFY_SUCCEEDED(CoInitializeEx(0, COINITBASE_MULTITHREADED));
  const wchar_t path[] = L"path-to-fxc-blob.bin";
  pDiaSource.Release();
  pProgramStream.Release();
  CComPtr<IDxcBlobEncoding> fxcBlob;
  CComPtr<IDxcBlob> pdbBlob;
  VERIFY_SUCCEEDED(pLib->CreateBlobFromFile(path, nullptr, &fxcBlob));
  std::string s = DumpParts(fxcBlob);
  CA2W sW(s.c_str(), CP_UTF8);
  WEX::Logging::Log::Comment(sW);
  VERIFY_SUCCEEDED(CreateDiaSourceFromDxbcBlob(pLib, fxcBlob, &pDiaSource));
  WEX::Logging::Log::Comment(GetDebugInfoAsText(pDiaSource).c_str());
#endif
}

TEST_F(CompilerTest, CompileDebugDisasmPDB) {
  const char *hlsl = R"(
    [RootSignature("")]
    float main(float pos : A) : SV_Target {
      float x = abs(pos);
      float y = sin(pos);
      float z = x + y;
      return z;
    }
  )";
  CComPtr<IDxcLibrary> pLib;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));

  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcCompiler2> pCompiler2;

  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  CComPtr<IDxcBlob> pPdbBlob;
  WCHAR *pDebugName = nullptr;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  VERIFY_SUCCEEDED(pCompiler.QueryInterface(&pCompiler2));
  CreateBlobFromText(hlsl, &pSource);
  LPCWSTR args[] = { L"/Zi", L"/Qembed_debug" };
  VERIFY_SUCCEEDED(pCompiler2->CompileWithDebug(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, _countof(args), nullptr, 0, nullptr, &pResult, &pDebugName, &pPdbBlob));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));

  // Test that disassembler can consume a PDB container
  CComPtr<IDxcBlobEncoding> pDisasm;
  VERIFY_SUCCEEDED(pCompiler->Disassemble(pPdbBlob, &pDisasm));
}

// Test that the new PDB format still works with Dia
TEST_F(CompilerTest, CompileDebugPDB) {
  const char *hlsl = R"(
    [RootSignature("")]
    float main(float pos : A) : SV_Target {
      float x = abs(pos);
      float y = sin(pos);
      float z = x + y;
      return z;
    }
  )";
  CComPtr<IDxcLibrary> pLib;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));

  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcCompiler2> pCompiler2;

  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  CComPtr<IDxcBlob> pPdbBlob;
  WCHAR *pDebugName = nullptr;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  VERIFY_SUCCEEDED(pCompiler.QueryInterface(&pCompiler2));
  CreateBlobFromText(hlsl, &pSource);
  LPCWSTR args[] = { L"/Zi", L"/Qembed_debug" };
  VERIFY_SUCCEEDED(pCompiler2->CompileWithDebug(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, _countof(args), nullptr, 0, nullptr, &pResult, &pDebugName, &pPdbBlob));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));

  CComPtr<IDiaDataSource> pDiaSource;
  CComPtr<IStream> pProgramStream;

  VERIFY_SUCCEEDED(pLib->CreateStreamFromBlobReadOnly(pPdbBlob, &pProgramStream));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcDiaDataSource, &pDiaSource));
  VERIFY_SUCCEEDED(pDiaSource->loadDataFromIStream(pProgramStream));

  // Test that IDxcContainerReflection can consume a PDB container
  CComPtr<IDxcContainerReflection> pReflection;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));
  VERIFY_SUCCEEDED(pReflection->Load(pPdbBlob));

  UINT32 uDebugInfoIndex = 0;
  VERIFY_SUCCEEDED(pReflection->FindFirstPartKind(hlsl::DFCC_ShaderDebugInfoDXIL, &uDebugInfoIndex));
}

TEST_F(CompilerTest, CompileDebugLines) {
  CComPtr<IDiaDataSource> pDiaSource;
  VERIFY_SUCCEEDED(CreateDiaSourceForCompile(
    "float main(float pos : A) : SV_Target {\r\n"
    "  float x = abs(pos);\r\n"
    "  float y = sin(pos);\r\n"
    "  float z = x + y;\r\n"
    "  return z;\r\n"
    "}", &pDiaSource));
    
  const uint32_t numExpectedVAs = 18;
  const uint32_t numExpectedLineEntries = 6;

  auto verifyLines = [=](const std::vector<LineNumber> lines) {
    VERIFY_ARE_EQUAL(lines.size(), numExpectedLineEntries);

    // loadInput
    VERIFY_ARE_EQUAL(lines[0].line, 1);
    VERIFY_ARE_EQUAL(lines[0].rva,  4);

    // abs
    VERIFY_ARE_EQUAL(lines[1].line, 2);
    VERIFY_ARE_EQUAL(lines[1].rva, 7);

    // sin
    VERIFY_ARE_EQUAL(lines[2].line, 3);
    VERIFY_ARE_EQUAL(lines[2].rva, 10);

    // fadd
    VERIFY_ARE_EQUAL(lines[3].line, 4);
    VERIFY_ARE_EQUAL(lines[3].rva, 13);

    // storeOutput
    VERIFY_ARE_EQUAL(lines[4].line, 5);
    VERIFY_ARE_EQUAL(lines[4].rva, 16);

    // ret
    VERIFY_ARE_EQUAL(lines[5].line, 5);
    VERIFY_ARE_EQUAL(lines[5].rva, 17);
  };
  
  CComPtr<IDiaSession> pSession;
  CComPtr<IDiaEnumLineNumbers> pEnumLineNumbers;

  // Verify lines are ok when getting one RVA at a time.
  std::vector<LineNumber> linesOneByOne;
  VERIFY_SUCCEEDED(pDiaSource->openSession(&pSession));
  for (int i = 0; i < numExpectedVAs; ++i) {
    VERIFY_SUCCEEDED(pSession->findLinesByRVA(i, 1, &pEnumLineNumbers));
    std::vector<LineNumber> lines = ReadLineNumbers(pEnumLineNumbers);
    std::copy(lines.begin(), lines.end(), std::back_inserter(linesOneByOne));
    pEnumLineNumbers.Release();
  }
  verifyLines(linesOneByOne);

  // Verify lines are ok when getting all RVAs at once.
  std::vector<LineNumber> linesAllAtOnce;
  pEnumLineNumbers.Release();
  VERIFY_SUCCEEDED(pSession->findLinesByRVA(0, numExpectedVAs, &pEnumLineNumbers));
  linesAllAtOnce = ReadLineNumbers(pEnumLineNumbers);
  verifyLines(linesAllAtOnce);

  // Verify lines are ok when getting all lines through enum tables.
  std::vector<LineNumber> linesFromTable;
  pEnumLineNumbers.Release();
  CComPtr<IDiaEnumTables> pTables;
  CComPtr<IDiaTable> pTable;
  VERIFY_SUCCEEDED(pSession->getEnumTables(&pTables));
  DWORD celt;
  while (SUCCEEDED(pTables->Next(1, &pTable, &celt)) && celt == 1)
  {
    if (SUCCEEDED(pTable->QueryInterface(&pEnumLineNumbers))) {
      linesFromTable = ReadLineNumbers(pEnumLineNumbers);
      break;
    }
    pTable.Release();
  }
  verifyLines(linesFromTable);
  
  // Verify lines are ok when getting by address.
  std::vector<LineNumber> linesByAddr;
  pEnumLineNumbers.Release();
  VERIFY_SUCCEEDED(pSession->findLinesByAddr(0, 0, numExpectedVAs, &pEnumLineNumbers));
  linesByAddr = ReadLineNumbers(pEnumLineNumbers);
  verifyLines(linesByAddr);

  // Verify findFileById.
  CComPtr<IDiaSourceFile> pFile;
  VERIFY_SUCCEEDED(pSession->findFileById(0, &pFile));
  CComBSTR pName;
  VERIFY_SUCCEEDED(pFile->get_fileName(&pName));
  VERIFY_ARE_EQUAL_WSTR(pName, L"source.hlsl");
}
#endif // _WIN32 - exclude dia stuff

TEST_F(CompilerTest, CompileWhenDefinesThenApplied) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  DxcDefine defines[] = {{L"F4", L"float4"}};

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("F4 main() : SV_Target { return 0; }", &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", nullptr, 0, defines,
                                      _countof(defines), nullptr, &pResult));
}

TEST_F(CompilerTest, CompileWhenDefinesManyThenApplied) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  LPCWSTR args[] = {L"/DVAL1=1",  L"/DVAL2=2",  L"/DVAL3=3",  L"/DVAL4=2",
                    L"/DVAL5=4",  L"/DNVAL1",   L"/DNVAL2",   L"/DNVAL3",
                    L"/DNVAL4",   L"/DNVAL5",   L"/DCVAL1=1", L"/DCVAL2=2",
                    L"/DCVAL3=3", L"/DCVAL4=2", L"/DCVAL5=4", L"/DCVALNONE="};

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target {\r\n"
                     "#ifndef VAL1\r\n"
                     "#error VAL1 not defined\r\n"
                     "#endif\r\n"
                     "#ifndef NVAL5\r\n"
                     "#error NVAL5 not defined\r\n"
                     "#endif\r\n"
                     "#ifndef CVALNONE\r\n"
                     "#error CVALNONE not defined\r\n"
                     "#endif\r\n"
                     "return 0; }",
                     &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", args, _countof(args), nullptr,
                                      0, nullptr, &pResult));
  HRESULT compileStatus;
  VERIFY_SUCCEEDED(pResult->GetStatus(&compileStatus));
  if (FAILED(compileStatus)) {
    CComPtr<IDxcBlobEncoding> pErrors;
    VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
    OutputDebugStringA((LPCSTR)pErrors->GetBufferPointer());
  }
  VERIFY_SUCCEEDED(compileStatus);
}

TEST_F(CompilerTest, CompileWhenEmptyThenFails) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pSourceBad;
  LPCWSTR pProfile = L"ps_6_0";
  LPCWSTR pEntryPoint = L"main";

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);
  CreateBlobFromText("float4 main() : SV_Target { return undef; }", &pSourceBad);

  // correct version
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", pEntryPoint,
                                      pProfile, nullptr, 0, nullptr, 0, nullptr,
                                      &pResult));
  pResult.Release();

  // correct version with compilation errors
  VERIFY_SUCCEEDED(pCompiler->Compile(pSourceBad, L"source.hlsl", pEntryPoint,
                                      pProfile, nullptr, 0, nullptr, 0, nullptr,
                                      &pResult));
  pResult.Release();

  // null source
  VERIFY_FAILED(pCompiler->Compile(nullptr, L"source.hlsl", pEntryPoint, pProfile,
                                   nullptr, 0, nullptr, 0, nullptr, &pResult));

  // null profile
  VERIFY_FAILED(pCompiler->Compile(pSourceBad, L"source.hlsl", pEntryPoint,
                                   nullptr, nullptr, 0, nullptr, 0, nullptr,
                                   &pResult));

  // null source name succeeds
  VERIFY_SUCCEEDED(pCompiler->Compile(pSourceBad, nullptr, pEntryPoint, pProfile,
                                   nullptr, 0, nullptr, 0, nullptr, &pResult));
  pResult.Release();

  // empty source name (as opposed to null) also suceeds
  VERIFY_SUCCEEDED(pCompiler->Compile(pSourceBad, L"", pEntryPoint, pProfile,
                                      nullptr, 0, nullptr, 0, nullptr,
                                      &pResult));
  pResult.Release();

  // null result
  VERIFY_FAILED(pCompiler->Compile(pSource, L"source.hlsl", pEntryPoint,
                                   pProfile, nullptr, 0, nullptr, 0, nullptr,
                                   nullptr));
}

TEST_F(CompilerTest, CompileWhenIncorrectThenFails) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4_undefined main() : SV_Target { return 0; }",
                     &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main", L"ps_6_0",
                                      nullptr, 0, nullptr, 0, nullptr,
                                      &pResult));
  HRESULT result;
  VERIFY_SUCCEEDED(pResult->GetStatus(&result));
  VERIFY_FAILED(result);

  CComPtr<IDxcBlobEncoding> pErrorBuffer;
  VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrorBuffer));
  std::string errorString(BlobToUtf8(pErrorBuffer));
  VERIFY_ARE_NOT_EQUAL(0U, errorString.size());
  // Useful for examining actual error message:
  // CA2W errorStringW(errorString.c_str(), CP_UTF8);
  // WEX::Logging::Log::Comment(errorStringW.m_psz);
}

TEST_F(CompilerTest, CompileWhenWorksThenDisassembleWorks) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", nullptr, 0, nullptr, 0,
                                      nullptr, &pResult));
  HRESULT result;
  VERIFY_SUCCEEDED(pResult->GetStatus(&result));
  VERIFY_SUCCEEDED(result);

  CComPtr<IDxcBlob> pProgram;
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));

  CComPtr<IDxcBlobEncoding> pDisassembleBlob;
  VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembleBlob));

  std::string disassembleString(BlobToUtf8(pDisassembleBlob));
  VERIFY_ARE_NOT_EQUAL(0U, disassembleString.size());
  // Useful for examining disassembly:
  // CA2W disassembleStringW(disassembleString.c_str(), CP_UTF8);
  // WEX::Logging::Log::Comment(disassembleStringW.m_psz);
}

#ifdef _WIN32 // Container builder unsupported

TEST_F(CompilerTest, CompileWhenDebugWorksThenStripDebug) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target {\r\n"
                     "  float4 local = abs(pos);\r\n"
                     "  return local;\r\n"
                     "}",
                     &pSource);
  LPCWSTR args[] = {L"/Zi", L"/Qembed_debug"};

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", args, _countof(args), nullptr,
                                      0, nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  // Check if it contains debug blob
  hlsl::DxilContainerHeader *pHeader = 
      hlsl::IsDxilContainerLike(pProgram->GetBufferPointer(), pProgram->GetBufferSize());
  VERIFY_SUCCEEDED(hlsl::IsValidDxilContainer(pHeader, pProgram->GetBufferSize()));
  hlsl::DxilPartHeader *pPartHeader = hlsl::GetDxilPartByType(
      pHeader, hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL);
  VERIFY_IS_NOT_NULL(pPartHeader);
  // Check debug info part does not exist after strip debug info

  CComPtr<IDxcBlob> pNewProgram;
  CComPtr<IDxcContainerBuilder> pBuilder;
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));
  VERIFY_SUCCEEDED(pBuilder->Load(pProgram));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  pHeader = hlsl::IsDxilContainerLike(pNewProgram->GetBufferPointer(), pNewProgram->GetBufferSize());
  VERIFY_SUCCEEDED(hlsl::IsValidDxilContainer(pHeader, pNewProgram->GetBufferSize()));
  pPartHeader = hlsl::GetDxilPartByType(
      pHeader, hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL);
  VERIFY_IS_NULL(pPartHeader);
}

TEST_F(CompilerTest, CompileWhenWorksThenAddRemovePrivate) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target {\r\n"
    "  return 0;\r\n"
    "}",
    &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0,
    nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  // Append private data blob
  CComPtr<IDxcContainerBuilder> pBuilder;
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));

  std::string privateTxt("private data");
  CComPtr<IDxcBlobEncoding> pPrivate;
  CreateBlobFromText(privateTxt.c_str(), &pPrivate);
  VERIFY_SUCCEEDED(pBuilder->Load(pProgram));
  VERIFY_SUCCEEDED(pBuilder->AddPart(hlsl::DxilFourCC::DFCC_PrivateData, pPrivate));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));

  CComPtr<IDxcBlob> pNewProgram;
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  hlsl::DxilContainerHeader *pContainerHeader = hlsl::IsDxilContainerLike(pNewProgram->GetBufferPointer(), pNewProgram->GetBufferSize());
  VERIFY_SUCCEEDED(hlsl::IsValidDxilContainer(pContainerHeader, pNewProgram->GetBufferSize()));
  hlsl::DxilPartHeader *pPartHeader = hlsl::GetDxilPartByType(
    pContainerHeader, hlsl::DxilFourCC::DFCC_PrivateData);
  VERIFY_IS_NOT_NULL(pPartHeader);
  // compare data
  std::string privatePart((const char *)(pPartHeader + 1), privateTxt.size());
  VERIFY_IS_TRUE(strcmp(privatePart.c_str(), privateTxt.c_str()) == 0);

  // Remove private data blob
  pBuilder.Release();
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));
  VERIFY_SUCCEEDED(pBuilder->Load(pNewProgram));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_PrivateData));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));

  pNewProgram.Release();
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  pContainerHeader = hlsl::IsDxilContainerLike(pNewProgram->GetBufferPointer(), pNewProgram->GetBufferSize());
  VERIFY_SUCCEEDED(hlsl::IsValidDxilContainer(pContainerHeader, pNewProgram->GetBufferSize()));
  pPartHeader = hlsl::GetDxilPartByType(
    pContainerHeader, hlsl::DxilFourCC::DFCC_PrivateData);
  VERIFY_IS_NULL(pPartHeader);
}

TEST_F(CompilerTest, CompileThenAddCustomDebugName) {
  // container builders prior to 1.3 did not support adding debug name parts
  if (m_ver.SkipDxilVersion(1, 3)) return;
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target {\r\n"
    "  return 0;\r\n"
    "}",
    &pSource);

  LPCWSTR args[] = { L"/Zi", L"/Qembed_debug", L"/Zss" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, _countof(args), nullptr, 0,
    nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  // Append private data blob
  CComPtr<IDxcContainerBuilder> pBuilder;
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));

  const char pNewName[] = "MyOwnUniqueName.lld";
  //include null terminator:
  size_t nameBlobPartSize = sizeof(hlsl::DxilShaderDebugName) + _countof(pNewName);
  // round up to four-byte size:
  size_t allocatedSize = (nameBlobPartSize + 3) & ~3;
  auto pNameBlobContent = reinterpret_cast<hlsl::DxilShaderDebugName*>(malloc(allocatedSize));
  ZeroMemory(pNameBlobContent, allocatedSize); //just to make sure trailing nulls are nulls.
  pNameBlobContent->Flags = 0;
  pNameBlobContent->NameLength = _countof(pNewName) - 1; //this is not supposed to include null terminator
  memcpy(pNameBlobContent + 1, pNewName, _countof(pNewName));

  CComPtr<IDxcBlobEncoding> pDebugName;

  CreateBlobPinned(pNameBlobContent, allocatedSize, CP_UTF8, &pDebugName);


  VERIFY_SUCCEEDED(pBuilder->Load(pProgram));
  // should fail since it already exists:
  VERIFY_FAILED(pBuilder->AddPart(hlsl::DxilFourCC::DFCC_ShaderDebugName, pDebugName));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_ShaderDebugName));
  VERIFY_SUCCEEDED(pBuilder->AddPart(hlsl::DxilFourCC::DFCC_ShaderDebugName, pDebugName));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));

  CComPtr<IDxcBlob> pNewProgram;
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  hlsl::DxilContainerHeader *pContainerHeader = hlsl::IsDxilContainerLike(pNewProgram->GetBufferPointer(), pNewProgram->GetBufferSize());
  VERIFY_SUCCEEDED(hlsl::IsValidDxilContainer(pContainerHeader, pNewProgram->GetBufferSize()));
  hlsl::DxilPartHeader *pPartHeader = hlsl::GetDxilPartByType(
    pContainerHeader, hlsl::DxilFourCC::DFCC_ShaderDebugName);
  VERIFY_IS_NOT_NULL(pPartHeader);
  // compare data
  VERIFY_IS_TRUE(memcmp(pPartHeader + 1, pNameBlobContent, allocatedSize) == 0);

  free(pNameBlobContent);

  // Remove private data blob
  pBuilder.Release();
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));
  VERIFY_SUCCEEDED(pBuilder->Load(pNewProgram));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_ShaderDebugName));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));

  pNewProgram.Release();
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  pContainerHeader = hlsl::IsDxilContainerLike(pNewProgram->GetBufferPointer(), pNewProgram->GetBufferSize());
  VERIFY_SUCCEEDED(hlsl::IsValidDxilContainer(pContainerHeader, pNewProgram->GetBufferSize()));
  pPartHeader = hlsl::GetDxilPartByType(
    pContainerHeader, hlsl::DxilFourCC::DFCC_ShaderDebugName);
  VERIFY_IS_NULL(pPartHeader);
}

TEST_F(CompilerTest, CompileWithRootSignatureThenStripRootSignature) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("[RootSignature(\"\")] \r\n"
                     "float4 main(float a : A) : SV_Target {\r\n"
                     "  return a;\r\n"
                     "}",
                     &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", nullptr, 0, nullptr,
                                      0, nullptr, &pResult));
  VERIFY_IS_NOT_NULL(pResult);
  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  VERIFY_IS_NOT_NULL(pProgram);
  hlsl::DxilContainerHeader *pContainerHeader = hlsl::IsDxilContainerLike(pProgram->GetBufferPointer(), pProgram->GetBufferSize());
  VERIFY_SUCCEEDED(hlsl::IsValidDxilContainer(pContainerHeader, pProgram->GetBufferSize()));
  hlsl::DxilPartHeader *pPartHeader = hlsl::GetDxilPartByType(
      pContainerHeader, hlsl::DxilFourCC::DFCC_RootSignature);
  VERIFY_IS_NOT_NULL(pPartHeader);
  pResult.Release();
  
  // Remove root signature
  CComPtr<IDxcBlob> pProgramRootSigRemoved;
  CComPtr<IDxcContainerBuilder> pBuilder;
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));
  VERIFY_SUCCEEDED(pBuilder->Load(pProgram));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_RootSignature));
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgramRootSigRemoved));
  pContainerHeader = hlsl::IsDxilContainerLike(pProgramRootSigRemoved->GetBufferPointer(), pProgramRootSigRemoved->GetBufferSize());
  VERIFY_SUCCEEDED(hlsl::IsValidDxilContainer(pContainerHeader, pProgramRootSigRemoved->GetBufferSize()));
  hlsl::DxilPartHeader *pPartHeaderShouldBeNull = hlsl::GetDxilPartByType(pContainerHeader,
                                        hlsl::DxilFourCC::DFCC_RootSignature);
  VERIFY_IS_NULL(pPartHeaderShouldBeNull);
  pBuilder.Release();
  pResult.Release();

  // Add root signature back
  CComPtr<IDxcBlobEncoding> pRootSignatureBlob;
  CComPtr<IDxcLibrary> pLibrary;
  CComPtr<IDxcBlob> pProgramRootSigAdded;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
  VERIFY_SUCCEEDED(pLibrary->CreateBlobWithEncodingFromPinned(
    hlsl::GetDxilPartData(pPartHeader), pPartHeader->PartSize, 0, &pRootSignatureBlob));
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));
  VERIFY_SUCCEEDED(pBuilder->Load(pProgramRootSigRemoved));
  pBuilder->AddPart(hlsl::DxilFourCC::DFCC_RootSignature, pRootSignatureBlob);
  pBuilder->SerializeContainer(&pResult);
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgramRootSigAdded));
  pContainerHeader = hlsl::IsDxilContainerLike(pProgramRootSigAdded->GetBufferPointer(), pProgramRootSigAdded->GetBufferSize());
  VERIFY_SUCCEEDED(hlsl::IsValidDxilContainer(pContainerHeader, pProgramRootSigAdded->GetBufferSize()));
  pPartHeader = hlsl::GetDxilPartByType(pContainerHeader,
                                        hlsl::DxilFourCC::DFCC_RootSignature);
  VERIFY_IS_NOT_NULL(pPartHeader);
}
#endif // Container builder unsupported

TEST_F(CompilerTest, CompileWhenIncludeThenLoadInvoked) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"helper.h\"\r\n"
    "float4 main() : SV_Target { return 0; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_ARE_EQUAL_WSTR(L"./helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeThenLoadUsed) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"helper.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_ARE_EQUAL_WSTR(L"./helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeAbsoluteThenLoadAbsolute) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
#ifdef _WIN32 // OS-specific root
  CreateBlobFromText(
    "#include \"C:\\helper.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);
#else
  CreateBlobFromText(
    "#include \"/helper.h\"\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);
#endif


  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
#ifdef _WIN32 // OS-specific root
  VERIFY_ARE_EQUAL_WSTR(L"C:\\helper.h;", pInclude->GetAllFileNames().c_str());
#else
  VERIFY_ARE_EQUAL_WSTR(L"/helper.h;", pInclude->GetAllFileNames().c_str());
#endif
}

TEST_F(CompilerTest, CompileWhenIncludeLocalThenLoadRelative) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"..\\helper.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
#ifdef _WIN32 // OS-specific directory dividers
  VERIFY_ARE_EQUAL_WSTR(L"./..\\helper.h;", pInclude->GetAllFileNames().c_str());
#else
  VERIFY_ARE_EQUAL_WSTR(L"./../helper.h;", pInclude->GetAllFileNames().c_str());
#endif
}

TEST_F(CompilerTest, CompileWhenIncludeSystemThenLoadNotRelative) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"subdir/other/file.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  LPCWSTR args[] = {
    L"-Ifoo"
  };
  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#include <helper.h>");
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, _countof(args), nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
#ifdef _WIN32 // OS-specific directory dividers
  VERIFY_ARE_EQUAL_WSTR(L"./subdir/other/file.h;./foo\\helper.h;", pInclude->GetAllFileNames().c_str());
#else
  VERIFY_ARE_EQUAL_WSTR(L"./subdir/other/file.h;./foo/helper.h;", pInclude->GetAllFileNames().c_str());
#endif
}

TEST_F(CompilerTest, CompileWhenIncludeSystemMissingThenLoadAttempt) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"subdir/other/file.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#include <helper.h>");
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  std::string failLog(VerifyOperationFailed(pResult));
  VERIFY_ARE_NOT_EQUAL(std::string::npos, failLog.find("<angled>")); // error message should prompt to use <angled> rather than "quotes"
  VERIFY_ARE_EQUAL_WSTR(L"./subdir/other/file.h;./subdir/other/helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeFlagsThenIncludeUsed) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include <helper.h>\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#define ZERO 0");

#ifdef _WIN32  // OS-specific root
  LPCWSTR args[] = { L"-I\\\\server\\share" };
#else
  LPCWSTR args[] = { L"-I/server/share" };
#endif
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, _countof(args), nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
#ifdef _WIN32  // OS-specific root
  VERIFY_ARE_EQUAL_WSTR(L"\\\\server\\share\\helper.h;", pInclude->GetAllFileNames().c_str());
#else
  VERIFY_ARE_EQUAL_WSTR(L"/server/share/helper.h;", pInclude->GetAllFileNames().c_str());
#endif
}

TEST_F(CompilerTest, CompileWhenIncludeMissingThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"file.h\"\r\n"
    "float4 main() : SV_Target { return 0; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  HRESULT hr;
  VERIFY_SUCCEEDED(pResult->GetStatus(&hr));
  VERIFY_FAILED(hr);
}

TEST_F(CompilerTest, CompileWhenIncludeHasPathThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  LPCWSTR Source = L"c:\\temp\\OddIncludes\\main.hlsl";
  LPCWSTR Args[] = { L"/I", L"c:\\temp" };
  LPCWSTR ArgsUp[] = { L"/I", L"c:\\Temp" };
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  bool useUpValues[] = { false, true };
  for (bool useUp : useUpValues) {
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
#if TEST_ON_DISK
    CComPtr<IDxcLibrary> pLibrary;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    VERIFY_SUCCEEDED(pLibrary->CreateIncludeHandler(&pInclude));
    VERIFY_SUCCEEDED(pLibrary->CreateBlobFromFile(Source, nullptr, &pSource));
#else
    CComPtr<TestIncludeHandler> pInclude;
    pInclude = new TestIncludeHandler(m_dllSupport);
    pInclude->CallResults.emplace_back("// Empty");
    CreateBlobFromText("#include \"include.hlsl\"\r\n"
                       "float4 main() : SV_Target { return 0; }",
                       &pSource);
#endif

    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, Source, L"main",
      L"ps_6_0", useUp ? ArgsUp : Args, _countof(Args), nullptr, 0, pInclude, &pResult));
    HRESULT hr;
    VERIFY_SUCCEEDED(pResult->GetStatus(&hr));
    VERIFY_SUCCEEDED(hr);
 }
}

TEST_F(CompilerTest, CompileWhenIncludeEmptyThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("#include \"empty.h\"\r\n"
                     "float4 main() : SV_Target { return 0; }",
                     &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("", CP_ACP); // An empty file would get detected as ACP code page

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", nullptr, 0, nullptr, 0,
                                      pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_ARE_EQUAL_WSTR(L"./empty.h;", pInclude->GetAllFileNames().c_str());
}

static const char EmptyCompute[] = "[numthreads(8,8,1)] void main() { }";

TEST_F(CompilerTest, CompileWhenODumpThenPassConfig) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(EmptyCompute, &pSource);

  LPCWSTR Args[] = { L"/Odump" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"cs_6_0", Args, _countof(Args), nullptr, 0, nullptr, &pResult));
  VerifyOperationSucceeded(pResult);
  CComPtr<IDxcBlob> pResultBlob;
  VERIFY_SUCCEEDED(pResult->GetResult(&pResultBlob));
  wstring passes = BlobToUtf16(pResultBlob);
  VERIFY_ARE_NOT_EQUAL(wstring::npos, passes.find(L"inline"));
}

TEST_F(CompilerTest, CompileWhenVdThenProducesDxilContainer) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(EmptyCompute, &pSource);

  LPCWSTR Args[] = { L"/Vd" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"cs_6_0", Args, _countof(Args), nullptr, 0, nullptr, &pResult));
  VerifyOperationSucceeded(pResult);
  CComPtr<IDxcBlob> pResultBlob;
  VERIFY_SUCCEEDED(pResult->GetResult(&pResultBlob));
  VERIFY_IS_TRUE(hlsl::IsValidDxilContainer(reinterpret_cast<hlsl::DxilContainerHeader *>(pResultBlob->GetBufferPointer()), pResultBlob->GetBufferSize()));
}

TEST_F(CompilerTest, CompileWhenODumpThenOptimizerMatch) {
  LPCWSTR OptLevels[] = { L"/Od", L"/O1", L"/O2" };
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOptimizer> pOptimizer;
  CComPtr<IDxcAssembler> pAssembler;
  CComPtr<IDxcValidator> pValidator;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcOptimizer, &pOptimizer));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcValidator, &pValidator));
  for (LPCWSTR OptLevel : OptLevels) {
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcBlob> pHighLevelBlob;
    CComPtr<IDxcBlob> pOptimizedModule;
    CComPtr<IDxcBlob> pAssembledBlob;

    // Could use EmptyCompute and cs_6_0, but there is an issue where properties
    // don't round-trip properly at high-level, so validation fails because
    // dimensions are set to zero. Workaround by using pixel shader instead.
    LPCWSTR Target = L"ps_6_0";
    CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);

    LPCWSTR Args[2] = { OptLevel, L"/Odump" };

    // Get the passes for this optimization level.
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
      Target, Args, _countof(Args), nullptr, 0, nullptr, &pResult));
    VerifyOperationSucceeded(pResult);
    CComPtr<IDxcBlob> pResultBlob;
    VERIFY_SUCCEEDED(pResult->GetResult(&pResultBlob));
    wstring passes = BlobToUtf16(pResultBlob);

    // Get wchar_t version and prepend hlsl-hlensure, to do a split high-level/opt compilation pass.
    std::vector<LPCWSTR> Options;
    SplitPassList(const_cast<LPWSTR>(passes.data()), Options);

    // Now compile directly.
    pResult.Release();
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
      Target, Args, 1, nullptr, 0, nullptr, &pResult));
    VerifyOperationSucceeded(pResult);

    // Now compile via a high-level compile followed by the optimization passes.
    pResult.Release();
    Args[_countof(Args)-1] = L"/fcgl";
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
      Target, Args, _countof(Args), nullptr, 0, nullptr, &pResult));
    VerifyOperationSucceeded(pResult);
    VERIFY_SUCCEEDED(pResult->GetResult(&pHighLevelBlob));
    VERIFY_SUCCEEDED(pOptimizer->RunOptimizer(pHighLevelBlob, Options.data(),
                                              Options.size(), &pOptimizedModule,
                                              nullptr));

    string text = DisassembleProgram(m_dllSupport, pOptimizedModule);
    WEX::Logging::Log::Comment(L"Final program:");
    WEX::Logging::Log::Comment(CA2W(text.c_str()));

    // At the very least, the module should be valid.
    pResult.Release();
    VERIFY_SUCCEEDED(pAssembler->AssembleToContainer(pOptimizedModule, &pResult));
    VerifyOperationSucceeded(pResult);
    VERIFY_SUCCEEDED(pResult->GetResult(&pAssembledBlob));
    pResult.Release();
    VERIFY_SUCCEEDED(pValidator->Validate(pAssembledBlob, DxcValidatorFlags_Default, &pResult));
    VerifyOperationSucceeded(pResult);
  }
}

static const UINT CaptureStacks = 0; // Set to 1 to enable captures
static const UINT StackFrameCount = 12;

struct InstrumentedHeapMalloc : public IMalloc {
private:
  HANDLE m_Handle;        // Heap handle.
  ULONG m_RefCount = 0;   // Reference count. Used for reference leaks, not for lifetime.
  ULONG m_AllocCount = 0; // Total # of alloc and realloc requests.
  ULONG m_AllocSize = 0;  // Total # of alloc and realloc bytes.
  ULONG m_Size = 0;       // Current # of alloc'ed bytes.
  ULONG m_FailAlloc = 0;  // If nonzero, the alloc/realloc call to fail.
  // Each allocation also tracks the following information:
  // - allocation callstack
  // - deallocation callstack
  // - prior/next blocks in a list of allocated blocks
  LIST_ENTRY AllocList;
  struct PtrData {
    LIST_ENTRY Entry;
    LPVOID AllocFrames[CaptureStacks ? StackFrameCount * CaptureStacks : 1];
    LPVOID FreeFrames[CaptureStacks ? StackFrameCount * CaptureStacks : 1];
    UINT64 AllocAtCount;
    DWORD AllocFrameCount;
    DWORD FreeFrameCount;
    SIZE_T Size;
    PtrData *Self;
  };
  PtrData *DataFromPtr(void *p) {
    if (p == nullptr) return nullptr;
    PtrData *R = ((PtrData *)p) - 1;
    if (R != R->Self) {
      VERIFY_FAIL(); // p is invalid or underrun
    }
    return R;
  }
public:
  InstrumentedHeapMalloc() : m_Handle(nullptr) {
    ResetCounts();
  }
  ~InstrumentedHeapMalloc() {
    if (m_Handle)
      HeapDestroy(m_Handle);
  }
  void ResetHeap() {
    if (m_Handle) {
      HeapDestroy(m_Handle);
      m_Handle = nullptr;
    }
    m_Handle = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
  }
  ULONG GetRefCount() const { return m_RefCount; }
  ULONG GetAllocCount() const { return m_AllocCount; }
  ULONG GetAllocSize() const { return m_AllocSize; }
  ULONG GetSize() const { return m_Size; }

  void ResetCounts() {
    m_RefCount = m_AllocCount = m_AllocSize = m_Size = 0;
    AllocList.Blink = AllocList.Flink = &AllocList;
  }
  void SetFailAlloc(ULONG index) {
    m_FailAlloc = index;
  }

  ULONG STDMETHODCALLTYPE AddRef() {
    return ++m_RefCount;
  }
  ULONG STDMETHODCALLTYPE Release() {
    if (m_RefCount == 0) VERIFY_FAIL();
    return --m_RefCount;
  }
  STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) {
    return DoBasicQueryInterface<IMalloc>(this, iid, ppvObject);
  }
  virtual void *STDMETHODCALLTYPE Alloc(_In_ SIZE_T cb) {
    ++m_AllocCount;
    if (m_FailAlloc && m_AllocCount >= m_FailAlloc) {
      return nullptr; // breakpoint for i failure - m_FailAlloc == 1+VAL
    }
    m_AllocSize += cb;
    m_Size += cb;
    PtrData *P = (PtrData *)HeapAlloc(m_Handle, HEAP_ZERO_MEMORY, sizeof(PtrData) + cb);
    P->Entry.Flink = AllocList.Flink;
    P->Entry.Blink = &AllocList;
    AllocList.Flink->Blink = &(P->Entry);
    AllocList.Flink = &(P->Entry);
    // breakpoint for i failure on NN alloc - m_FailAlloc == 1+VAL && m_AllocCount == NN
    // breakpoint for happy path for NN alloc - m_AllocCount == NN
    P->AllocAtCount = m_AllocCount;
    if (CaptureStacks)
      P->AllocFrameCount = CaptureStackBackTrace(1, StackFrameCount, P->AllocFrames, nullptr);
    P->Size = cb;
    P->Self = P;
    return P + 1;
  }

  virtual void *STDMETHODCALLTYPE Realloc(_In_opt_ void *pv, _In_ SIZE_T cb) {
    SIZE_T priorSize = pv == nullptr ? (SIZE_T)0 : GetSize(pv);
    void *R = Alloc(cb);
    if (!R)
      return nullptr;
    SIZE_T copySize = std::min(cb, priorSize);
    memcpy(R, pv, copySize);
    Free(pv);
    return R;
  }

  virtual void STDMETHODCALLTYPE Free(_In_opt_ void *pv) {
    if (!pv)
      return;
    PtrData *P = DataFromPtr(pv);
    if (P->FreeFrameCount)
      VERIFY_FAIL(); // double-free detected
    m_Size -= P->Size;
    P->Entry.Flink->Blink = P->Entry.Blink;
    P->Entry.Blink->Flink = P->Entry.Flink;
    if (CaptureStacks)
      P->FreeFrameCount =
          CaptureStackBackTrace(1, StackFrameCount, P->FreeFrames, nullptr);
  }

  virtual SIZE_T STDMETHODCALLTYPE GetSize(
    /* [annotation][in] */
    _In_opt_ _Post_writable_byte_size_(return)  void *pv)
  {
    if (pv == nullptr) return 0;
    return DataFromPtr(pv)->Size;
  }

  virtual int STDMETHODCALLTYPE DidAlloc(
      _In_opt_ void *pv) {
    return -1; // don't know
  }

  virtual void STDMETHODCALLTYPE HeapMinimize(void) {}

  void DumpLeaks() {
    PtrData *ptr = (PtrData*)AllocList.Flink;;
    PtrData *end = (PtrData*)AllocList.Blink;;

    WEX::Logging::Log::Comment(FormatToWString(L"Leaks total size: %d", (signed int)m_Size).data());
    while (ptr != end) {
      WEX::Logging::Log::Comment(FormatToWString(L"Memory leak at 0x0%X, size %d, alloc# %d", ptr + 1, ptr->Size, ptr->AllocAtCount).data());
      ptr = (PtrData*)ptr->Entry.Flink;
    }
  }
};

#if _ITERATOR_DEBUG_LEVEL==0
// CompileWhenNoMemThenOOM can properly detect leaks only when debug iterators are disabled
TEST_F(CompilerTest, CompileWhenNoMemThenOOM) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  CComPtr<IDxcBlobEncoding> pSource;
  CreateBlobFromText(EmptyCompute, &pSource);

  InstrumentedHeapMalloc InstrMalloc;
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  ULONG allocCount = 0;
  ULONG allocSize = 0;
  ULONG initialRefCount;

  InstrMalloc.ResetHeap();

  VERIFY_IS_TRUE(m_dllSupport.HasCreateWithMalloc());

  // Verify a simple object creation.
  initialRefCount = InstrMalloc.GetRefCount();
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance2(&InstrMalloc, CLSID_DxcCompiler, &pCompiler));
  pCompiler.Release();
  VERIFY_IS_TRUE(0 == InstrMalloc.GetSize());
  VERIFY_ARE_EQUAL(initialRefCount, InstrMalloc.GetRefCount());
  InstrMalloc.ResetCounts();
  InstrMalloc.ResetHeap();

  // First time, run to completion and capture stats.
  initialRefCount = InstrMalloc.GetRefCount();
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance2(&InstrMalloc, CLSID_DxcCompiler, &pCompiler));
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"cs_6_0", nullptr, 0, nullptr, 0, nullptr, &pResult));
  allocCount = InstrMalloc.GetAllocCount();
  allocSize = InstrMalloc.GetAllocSize();

  HRESULT hrWithMemory;
  VERIFY_SUCCEEDED(pResult->GetStatus(&hrWithMemory));
  VERIFY_SUCCEEDED(hrWithMemory);

  pCompiler.Release();
  pResult.Release();

  VERIFY_IS_TRUE(allocSize > allocCount);

  // Ensure that after all resources are released, there are no outstanding
  // allocations or references.
  //
  // First leak is in ((InstrumentedHeapMalloc::PtrData *)InstrMalloc.AllocList.Flink)
  if (InstrMalloc.GetSize() != 0) {
    WEX::Logging::Log::Comment(L"Memory leak(s) detected");
    InstrMalloc.DumpLeaks();
    VERIFY_IS_TRUE(0 == InstrMalloc.GetSize());
  }

  VERIFY_ARE_EQUAL(initialRefCount, InstrMalloc.GetRefCount());

  // In Debug, without /D_ITERATOR_DEBUG_LEVEL=0, debug iterators will be used;
  // this causes a problem where std::string is specified as noexcept, and yet
  // a sentinel is allocated that may fail and throw.
  if (m_ver.SkipOutOfMemoryTest()) return;

  // Now, fail each allocation and make sure we get an error.
  for (ULONG i = 0; i <= allocCount; ++i) {
    // LogCommentFmt(L"alloc fail %u", i);
    bool isLast = i == allocCount;
    InstrMalloc.ResetCounts();
    InstrMalloc.ResetHeap();
    InstrMalloc.SetFailAlloc(i + 1);
    HRESULT hrOp = m_dllSupport.CreateInstance2(&InstrMalloc, CLSID_DxcCompiler, &pCompiler);
    if (SUCCEEDED(hrOp)) {
      hrOp = pCompiler->Compile(pSource, L"source.hlsl", L"main", L"cs_6_0",
                                nullptr, 0, nullptr, 0, nullptr, &pResult);
      if (SUCCEEDED(hrOp)) {
        pResult->GetStatus(&hrOp);
      }
    }
    if (FAILED(hrOp)) {
      // This is true in *almost* every case. When the OOM happens during stream
      // handling, there is no specific error set; by the time it's detected,
      // it propagates as E_FAIL.
      //VERIFY_ARE_EQUAL(hrOp, E_OUTOFMEMORY);
      VERIFY_IS_TRUE(hrOp == E_OUTOFMEMORY || hrOp == E_FAIL);
    }
    if (isLast)
      VERIFY_SUCCEEDED(hrOp);
    else
      VERIFY_FAILED(hrOp);
    pCompiler.Release();
    pResult.Release();
    
    if (InstrMalloc.GetSize() != 0) {
      WEX::Logging::Log::Comment(FormatToWString(L"Memory leak(s) detected, allocCount = %d", i).data()); 
      InstrMalloc.DumpLeaks();
      VERIFY_IS_TRUE(0 == InstrMalloc.GetSize());
    }
    VERIFY_ARE_EQUAL(initialRefCount, InstrMalloc.GetRefCount());
  }
}
#endif

TEST_F(CompilerTest, CompileWhenShaderModelMismatchAttributeThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(EmptyCompute, &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, nullptr, &pResult));
  std::string failLog(VerifyOperationFailed(pResult));
  VERIFY_ARE_NOT_EQUAL(string::npos, failLog.find("attribute numthreads only valid for CS"));
}

TEST_F(CompilerTest, CompileBadHlslThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "bad hlsl", &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_FAILED(status);
}

TEST_F(CompilerTest, CompileLegacyShaderModelThenFail) {
  VerifyCompileFailed(
    "float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", L"ps_5_1", nullptr);
}

TEST_F(CompilerTest, CompileWhenRecursiveAlbeitStaticTermThenFail) {
  // This shader will compile under fxc because if execution is
  // simulated statically, it does terminate. dxc changes this behavior
  // to avoid imposing the requirement on the compiler.
  const char ShaderText[] =
    "static int i = 10;\r\n"
    "float4 f(); // Forward declaration\r\n"
    "float4 g() { if (i > 10) { i--; return f(); } else return 0; } // Recursive call to 'f'\r\n"
    "float4 f() { return g(); } // First call to 'g'\r\n"
    "float4 VS() : SV_Position{\r\n"
    "  return f(); // First call to 'f'\r\n"
    "}\r\n";
  VerifyCompileFailed(ShaderText, L"vs_6_0", "recursive functions not allowed", L"VS");
}

TEST_F(CompilerTest, CompileWhenRecursiveThenFail) {
  const char ShaderTextSimple[] =
    "float4 f(); // Forward declaration\r\n"
    "float4 g() { return f(); } // Recursive call to 'f'\r\n"
    "float4 f() { return g(); } // First call to 'g'\r\n"
    "float4 main() : SV_Position{\r\n"
    "  return f(); // First call to 'f'\r\n"
    "}\r\n";
  VerifyCompileFailed(ShaderTextSimple, L"vs_6_0", "recursive functions not allowed");

  const char ShaderTextIndirect[] =
    "float4 f(); // Forward declaration\r\n"
    "float4 g() { return f(); } // Recursive call to 'f'\r\n"
    "float4 f() { return g(); } // First call to 'g'\r\n"
    "float4 main() : SV_Position{\r\n"
    "  return f(); // First call to 'f'\r\n"
    "}\r\n";
  VerifyCompileFailed(ShaderTextIndirect, L"vs_6_0", "recursive functions not allowed");

  const char ShaderTextSelf[] =
    "float4 main() : SV_Position{\r\n"
    "  return main();\r\n"
    "}\r\n";
  VerifyCompileFailed(ShaderTextSelf, L"vs_6_0", "recursive functions not allowed");

  const char ShaderTextMissing[] =
    "float4 mainz() : SV_Position{\r\n"
    "  return 1;\r\n"
    "}\r\n";
  VerifyCompileFailed(ShaderTextMissing, L"vs_6_0", "missing entry point definition");
}

TEST_F(CompilerTest, CompileHlsl2015ThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pErrors;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", &pSource);

  LPCWSTR args[2] = { L"-HV", L"2015" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, 2, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_ARE_EQUAL(status, E_INVALIDARG);
  VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
  LPCSTR pErrorMsg = "HLSL Version 2015 is only supported for language services";
  CheckOperationResultMsgs(pResult, &pErrorMsg, 1, false, false);
}

TEST_F(CompilerTest, CompileHlsl2016ThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pErrors;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", &pSource);

  LPCWSTR args[2] = { L"-HV", L"2016" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, 2, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
}

TEST_F(CompilerTest, CompileHlsl2017ThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pErrors;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", &pSource);

  LPCWSTR args[2] = { L"-HV", L"2017" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, 2, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
}

TEST_F(CompilerTest, CompileHlsl2018ThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pErrors;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", &pSource);

  LPCWSTR args[2] = { L"-HV", L"2018" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, 2, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
}

TEST_F(CompilerTest, CompileHlsl2019ThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pErrors;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", &pSource);

  LPCWSTR args[2] = { L"-HV", L"2019" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, 2, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_ARE_EQUAL(status, E_INVALIDARG);
  VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
  LPCSTR pErrorMsg = "Unknown HLSL version";
  CheckOperationResultMsgs(pResult, &pErrorMsg, 1, false, false);
}

#ifdef _WIN32 // - exclude dia stuff
TEST_F(CompilerTest, DiaLoadBadBitcodeThenFail) {
  CComPtr<IDxcBlob> pBadBitcode;
  CComPtr<IDiaDataSource> pDiaSource;
  CComPtr<IStream> pStream;
  CComPtr<IDxcLibrary> pLib;

  Utf8ToBlob(m_dllSupport, "badcode", &pBadBitcode);
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));
  VERIFY_SUCCEEDED(pLib->CreateStreamFromBlobReadOnly(pBadBitcode, &pStream));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcDiaDataSource, &pDiaSource));
  VERIFY_FAILED(pDiaSource->loadDataFromIStream(pStream));
}

static void CompileAndGetDebugPart(dxc::DxcDllSupport &dllSupport, const char *source, wchar_t *profile, IDxcBlob **ppDebugPart) {
  CComPtr<IDxcBlob> pContainer;
  CComPtr<IDxcLibrary> pLib;
  CComPtr<IDxcContainerReflection> pReflection;
  UINT32 index;
  std::vector<LPCWSTR> args;
  args.push_back(L"/Zi");
  args.push_back(L"/Qembed_debug");

  VerifyCompileOK(dllSupport, source, profile, args, &pContainer);
  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));
  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));
  VERIFY_SUCCEEDED(pReflection->Load(pContainer));
  VERIFY_SUCCEEDED(pReflection->FindFirstPartKind(hlsl::DFCC_ShaderDebugInfoDXIL, &index));
  VERIFY_SUCCEEDED(pReflection->GetPartContent(index, ppDebugPart));
}

static void CompileTestAndLoadDiaSource(dxc::DxcDllSupport &dllSupport, const char *source, wchar_t *profile, IDiaDataSource **ppDataSource) {
  CComPtr<IDxcBlob> pDebugContent;
  CComPtr<IStream> pStream;
  CComPtr<IDiaDataSource> pDiaSource;
  CComPtr<IDxcLibrary> pLib;
  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));

  CompileAndGetDebugPart(dllSupport, source, profile, &pDebugContent);
  VERIFY_SUCCEEDED(pLib->CreateStreamFromBlobReadOnly(pDebugContent, &pStream));
  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcDiaDataSource, &pDiaSource));
  VERIFY_SUCCEEDED(pDiaSource->loadDataFromIStream(pStream));
  if (ppDataSource) {
    *ppDataSource = pDiaSource.Detach();
  }
}

static void CompileTestAndLoadDia(dxc::DxcDllSupport &dllSupport, IDiaDataSource **ppDataSource) {
  CompileTestAndLoadDiaSource(dllSupport, EmptyCompute, L"cs_6_0", ppDataSource);
}

TEST_F(CompilerTest, DiaLoadDebugSubrangeNegativeThenOK) {
  static const char source[] = R"(
    SamplerState  samp0 : register(s0);
    Texture2DArray tex0 : register(t0);

    float4 foo(Texture2DArray textures[], int idx, SamplerState samplerState, float3 uvw) {
      return textures[NonUniformResourceIndex(idx)].Sample(samplerState, uvw);
    }

    [RootSignature( "DescriptorTable(SRV(t0)), DescriptorTable(Sampler(s0)) " )]
    float4 main(int index : INDEX, float3 uvw : TEXCOORD) : SV_Target {
      Texture2DArray textures[] = {
        tex0,
      };
      return foo(textures, index, samp0, uvw);
    }
  )";

  CComPtr<IDiaDataSource> pDiaDataSource;
  CComPtr<IDiaSession> pDiaSession;
  CompileTestAndLoadDiaSource(m_dllSupport, source, L"ps_6_0", &pDiaDataSource);

  VERIFY_SUCCEEDED(pDiaDataSource->openSession(&pDiaSession));
}

TEST_F(CompilerTest, DiaLoadRelocatedBitcode) {

  static const char source[] = R"(
    SamplerState  samp0 : register(s0);
    Texture2DArray tex0 : register(t0);

    float4 foo(Texture2DArray textures[], int idx, SamplerState samplerState, float3 uvw) {
      return textures[NonUniformResourceIndex(idx)].Sample(samplerState, uvw);
    }

    [RootSignature( "DescriptorTable(SRV(t0)), DescriptorTable(Sampler(s0)) " )]
    float4 main(int index : INDEX, float3 uvw : TEXCOORD) : SV_Target {
      Texture2DArray textures[] = {
        tex0,
      };
      return foo(textures, index, samp0, uvw);
    }
  )";

  CComPtr<IDxcBlob> pPart;
  CComPtr<IDiaDataSource> pDiaSource;
  CComPtr<IStream> pStream;

  CComPtr<IDxcLibrary> pLib;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));

  CompileAndGetDebugPart(m_dllSupport, source, L"ps_6_0", &pPart);
  const char *pPartData = (char *)pPart->GetBufferPointer();
  const size_t uPartSize = pPart->GetBufferSize();

  // Get program header
  const hlsl::DxilProgramHeader *programHeader = (hlsl::DxilProgramHeader *)pPartData;

  const char *pBitcode = nullptr;
  uint32_t uBitcodeSize = 0;
  hlsl::GetDxilProgramBitcode(programHeader, &pBitcode, &uBitcodeSize);
  VERIFY_IS_TRUE(uBitcodeSize % sizeof(UINT32) == 0);

  size_t uNewGapSize = 4 * 10; // Size of some bytes between program header and bitcode
  size_t uNewSuffixeBytes = 4 * 10; // Size of some random bytes after the program

  hlsl::DxilProgramHeader newProgramHeader = {};
  memcpy(&newProgramHeader, programHeader, sizeof(newProgramHeader));
  newProgramHeader.BitcodeHeader.BitcodeOffset = uNewGapSize + sizeof(newProgramHeader.BitcodeHeader);

  unsigned uNewSizeInBytes = sizeof(newProgramHeader) + uNewGapSize + uBitcodeSize + uNewSuffixeBytes;
  VERIFY_IS_TRUE(uNewSizeInBytes % sizeof(UINT32) == 0);
  newProgramHeader.SizeInUint32 = uNewSizeInBytes / sizeof(UINT32);

  llvm::SmallVector<char, 0> buffer;
  llvm::raw_svector_ostream OS(buffer);

  // Write the header
  OS.write((char *)&newProgramHeader, sizeof(newProgramHeader));

  // Write some garbage between the header and the bitcode
  for (unsigned i = 0; i < uNewGapSize; i++) {
    OS.write(0xFF);
  }

  // Write the actual bitcode
  OS.write(pBitcode, uBitcodeSize);

  // Write some garbage after the bitcode
  for (unsigned i = 0; i < uNewSuffixeBytes; i++) {
    OS.write(0xFF);
  }
  OS.flush();

  // Try to load this new program, make sure dia is still okay.
  CComPtr<IDxcBlobEncoding> pNewProgramBlob;
  VERIFY_SUCCEEDED(pLib->CreateBlobWithEncodingFromPinned(buffer.data(), buffer.size(), CP_ACP, &pNewProgramBlob));

  CComPtr<IStream> pNewProgramStream;
  VERIFY_SUCCEEDED(pLib->CreateStreamFromBlobReadOnly(pNewProgramBlob, &pNewProgramStream));

  CComPtr<IDiaDataSource> pDiaDataSource;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcDiaDataSource, &pDiaDataSource));

  VERIFY_SUCCEEDED(pDiaDataSource->loadDataFromIStream(pNewProgramStream));
}

TEST_F(CompilerTest, DiaCompileArgs) {
  static const char source[] = R"(
    SamplerState  samp0 : register(s0);
    Texture2DArray tex0 : register(t0);

    float4 foo(Texture2DArray textures[], int idx, SamplerState samplerState, float3 uvw) {
      return textures[NonUniformResourceIndex(idx)].Sample(samplerState, uvw);
    }

    [RootSignature( "DescriptorTable(SRV(t0)), DescriptorTable(Sampler(s0)) " )]
    float4 main(int index : INDEX, float3 uvw : TEXCOORD) : SV_Target {
      Texture2DArray textures[] = {
        tex0,
      };
      return foo(textures, index, samp0, uvw);
    }
  )";

  CComPtr<IDxcBlob> pPart;
  CComPtr<IDiaDataSource> pDiaSource;
  CComPtr<IStream> pStream;

  CComPtr<IDxcLibrary> pLib;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));

  const WCHAR *FlagList[] = {
    L"/Zi",
    L"-Zpr",
    L"/Qembed_debug",
    L"/Fd", L"F:\\my dir\\",
    L"-Fo", L"F:\\my dir\\file.dxc",
  };
  const WCHAR *DefineList[] = {
    L"MY_SPECIAL_DEFINE",
    L"MY_OTHER_SPECIAL_DEFINE=\"MY_STRING\"",
  };

  std::vector<LPCWSTR> args;
  for (unsigned i = 0; i < _countof(FlagList); i++) {
    args.push_back(FlagList[i]);
  }
  for (unsigned i = 0; i < _countof(DefineList); i++) {
    args.push_back(L"/D");
    args.push_back(DefineList[i]);
  }

  auto CompileAndGetDebugPart = [&args](dxc::DxcDllSupport &dllSupport, const char *source, wchar_t *profile, IDxcBlob **ppDebugPart) {
    CComPtr<IDxcBlob> pContainer;
    CComPtr<IDxcLibrary> pLib;
    CComPtr<IDxcContainerReflection> pReflection;
    UINT32 index;

    VerifyCompileOK(dllSupport, source, profile, args, &pContainer);
    VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));
    VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));
    VERIFY_SUCCEEDED(pReflection->Load(pContainer));
    VERIFY_SUCCEEDED(pReflection->FindFirstPartKind(hlsl::DFCC_ShaderDebugInfoDXIL, &index));
    VERIFY_SUCCEEDED(pReflection->GetPartContent(index, ppDebugPart));
  };

  CompileAndGetDebugPart(m_dllSupport, source, L"ps_6_0", &pPart);

  CComPtr<IStream> pNewProgramStream;
  VERIFY_SUCCEEDED(pLib->CreateStreamFromBlobReadOnly(pPart, &pNewProgramStream));

  CComPtr<IDiaDataSource> pDiaDataSource;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcDiaDataSource, &pDiaDataSource));

  VERIFY_SUCCEEDED(pDiaDataSource->loadDataFromIStream(pNewProgramStream));

  CComPtr<IDiaSession> pSession;
  VERIFY_SUCCEEDED(pDiaDataSource->openSession(&pSession));

  CComPtr<IDiaEnumTables> pEnumTables;
  VERIFY_SUCCEEDED(pSession->getEnumTables(&pEnumTables));

  CComPtr<IDiaTable> pSymbolTable;

  LONG uCount = 0;
  VERIFY_SUCCEEDED(pEnumTables->get_Count(&uCount));
  for (int i = 0; i < uCount; i++) {
    CComPtr<IDiaTable> pTable;
    VARIANT index = {};
    index.vt = VT_I4;
    index.intVal = i;
    VERIFY_SUCCEEDED(pEnumTables->Item(index, &pTable));

    CComBSTR pName;
    VERIFY_SUCCEEDED(pTable->get_name(&pName));

    if (pName == "Symbols") {
      pSymbolTable = pTable;
      break;
    }
  }

  std::wstring Args;
  std::wstring Entry;
  std::wstring Target;
  std::vector<std::wstring> Defines;
  std::vector<std::wstring> Flags;

  auto ReadNullSeparatedTokens = [](BSTR Str) -> std::vector<std::wstring> {
    std::vector<std::wstring> Result;
    while (*Str) {
      Result.push_back(std::wstring(Str));
      Str += wcslen(Str)+1;
    }
    return Result;
  };

  VERIFY_SUCCEEDED(pSymbolTable->get_Count(&uCount));
  for (int i = 0; i < uCount; i++) {
    CComPtr<IUnknown> pSymbolUnk;
    CComPtr<IDiaSymbol> pSymbol;
    CComVariant pValue;
    CComBSTR pName;
    VERIFY_SUCCEEDED(pSymbolTable->Item(i, &pSymbolUnk));
    VERIFY_SUCCEEDED(pSymbolUnk->QueryInterface(&pSymbol));
    VERIFY_SUCCEEDED(pSymbol->get_name(&pName));
    VERIFY_SUCCEEDED(pSymbol->get_value(&pValue));
    if (pName == "hlslTarget") {
      if (pValue.vt == VT_BSTR)
        Target = pValue.bstrVal;
    }
    else if (pName == "hlslEntry") {
      if (pValue.vt == VT_BSTR)
        Entry = pValue.bstrVal;
    }
    else if (pName == "hlslFlags") {
      if (pValue.vt == VT_BSTR)
        Flags = ReadNullSeparatedTokens(pValue.bstrVal);
    }
    else if (pName == "hlslArguments") {
      if (pValue.vt == VT_BSTR)
        Args = pValue.bstrVal;
    }
    else if (pName == "hlslDefines") {
      if (pValue.vt == VT_BSTR)
        Defines = ReadNullSeparatedTokens(pValue.bstrVal);
    }
  }

  auto VectorContains = [](std::vector<std::wstring> &Tokens, std::wstring Sub) {
    for (unsigned i = 0; i < Tokens.size(); i++) {
      if (Tokens[i].find(Sub) != std::wstring::npos)
        return true;
    }
    return false;
  };

  VERIFY_IS_TRUE(Target == L"ps_6_0");
  VERIFY_IS_TRUE(Entry == L"main");

  VERIFY_IS_TRUE(_countof(FlagList) == Flags.size());
  for (unsigned i = 0; i < _countof(FlagList); i++) {
    VERIFY_IS_TRUE(Flags[i] == FlagList[i]);
  }
  for (unsigned i = 0; i < _countof(DefineList); i++) {
    VERIFY_IS_TRUE(VectorContains(Defines, DefineList[i]));
  }
}

TEST_F(CompilerTest, DiaLoadBitcodePlusExtraData) {
  // Test that dia doesn't crash when bitcode has unused extra data at the end

  static const char source[] = R"(
    SamplerState  samp0 : register(s0);
    Texture2DArray tex0 : register(t0);

    float4 foo(Texture2DArray textures[], int idx, SamplerState samplerState, float3 uvw) {
      return textures[NonUniformResourceIndex(idx)].Sample(samplerState, uvw);
    }

    [RootSignature( "DescriptorTable(SRV(t0)), DescriptorTable(Sampler(s0)) " )]
    float4 main(int index : INDEX, float3 uvw : TEXCOORD) : SV_Target {
      Texture2DArray textures[] = {
        tex0,
      };
      return foo(textures, index, samp0, uvw);
    }
  )";

  CComPtr<IDxcBlob> pPart;
  CComPtr<IDiaDataSource> pDiaSource;
  CComPtr<IStream> pStream;

  CComPtr<IDxcLibrary> pLib;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));

  CompileAndGetDebugPart(m_dllSupport, source, L"ps_6_0", &pPart);
  const char *pPartData = (char *)pPart->GetBufferPointer();
  const size_t uPartSize = pPart->GetBufferSize();

  // Get program header
  const hlsl::DxilProgramHeader *programHeader = (hlsl::DxilProgramHeader *)pPartData;

  const char *pBitcode = nullptr;
  uint32_t uBitcodeSize = 0;
  hlsl::GetDxilProgramBitcode(programHeader, &pBitcode, &uBitcodeSize);

  llvm::SmallVector<char, 0> buffer;
  llvm::raw_svector_ostream OS(buffer);

  // Write the bitcode
  OS.write(pBitcode, uBitcodeSize);
  for (unsigned i = 0; i < 12; i++) {
    OS.write(0xFF);
  }
  OS.flush();

  // Try to load this new program, make sure dia is still okay.
  CComPtr<IDxcBlobEncoding> pNewProgramBlob;
  VERIFY_SUCCEEDED(pLib->CreateBlobWithEncodingFromPinned(buffer.data(), buffer.size(), CP_ACP, &pNewProgramBlob));

  CComPtr<IStream> pNewProgramStream;
  VERIFY_SUCCEEDED(pLib->CreateStreamFromBlobReadOnly(pNewProgramBlob, &pNewProgramStream));

  CComPtr<IDiaDataSource> pDiaDataSource;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcDiaDataSource, &pDiaDataSource));

  VERIFY_SUCCEEDED(pDiaDataSource->loadDataFromIStream(pNewProgramStream));
}

TEST_F(CompilerTest, DiaLoadDebugThenOK) {
  CompileTestAndLoadDia(m_dllSupport, nullptr);
}

TEST_F(CompilerTest, DiaTableIndexThenOK) {
  CComPtr<IDiaDataSource> pDiaSource;
  CComPtr<IDiaSession> pDiaSession;
  CComPtr<IDiaEnumTables> pEnumTables;
  CComPtr<IDiaTable> pTable;
  VARIANT vtIndex;
  CompileTestAndLoadDia(m_dllSupport, &pDiaSource);
  VERIFY_SUCCEEDED(pDiaSource->openSession(&pDiaSession));
  VERIFY_SUCCEEDED(pDiaSession->getEnumTables(&pEnumTables));

  vtIndex.vt = VT_EMPTY;
  VERIFY_FAILED(pEnumTables->Item(vtIndex, &pTable));

  vtIndex.vt = VT_I4;
  vtIndex.intVal = 1;
  VERIFY_SUCCEEDED(pEnumTables->Item(vtIndex, &pTable));
  VERIFY_IS_NOT_NULL(pTable.p);
  pTable.Release();

  vtIndex.vt = VT_UI4;
  vtIndex.uintVal = 1;
  VERIFY_SUCCEEDED(pEnumTables->Item(vtIndex, &pTable));
  VERIFY_IS_NOT_NULL(pTable.p);
  pTable.Release();

  vtIndex.uintVal = 100;
  VERIFY_FAILED(pEnumTables->Item(vtIndex, &pTable));
}
#endif // _WIN32 - exclude dia stuff

#ifdef _WIN32
TEST_F(CompilerTest, PixDebugCompileInfo) {
  static const char source[] = R"(
    SamplerState  samp0 : register(s0);
    Texture2DArray tex0 : register(t0);

    float4 foo(Texture2DArray textures[], int idx, SamplerState samplerState, float3 uvw) {
      return textures[NonUniformResourceIndex(idx)].Sample(samplerState, uvw);
    }

    [RootSignature( "DescriptorTable(SRV(t0)), DescriptorTable(Sampler(s0)) " )]
    float4 main(int index : INDEX, float3 uvw : TEXCOORD) : SV_Target {
      Texture2DArray textures[] = {
        tex0,
      };
      return foo(textures, index, samp0, uvw);
    }
  )";

  CComPtr<IDxcBlob> pPart;
  CComPtr<IDiaDataSource> pDiaSource;
  CComPtr<IStream> pStream;

  CComPtr<IDxcLibrary> pLib;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));

  const WCHAR *FlagList[] = {
      L"/Zi",          L"-Zpr", L"/Qembed_debug",        L"/Fd",
      L"F:\\my dir\\", L"-Fo",  L"F:\\my dir\\file.dxc",
  };
  const WCHAR *DefineList[] = {
      L"MY_SPECIAL_DEFINE",
      L"MY_OTHER_SPECIAL_DEFINE=\"MY_STRING\"",
  };

  std::vector<LPCWSTR> args;
  for (unsigned i = 0; i < _countof(FlagList); i++) {
    args.push_back(FlagList[i]);
  }
  for (unsigned i = 0; i < _countof(DefineList); i++) {
    args.push_back(L"/D");
    args.push_back(DefineList[i]);
  }

  auto CompileAndGetDebugPart = [&args](dxc::DxcDllSupport &dllSupport,
                                        const char *source, wchar_t *profile,
                                        IDxcBlob **ppDebugPart) {
    CComPtr<IDxcBlob> pContainer;
    CComPtr<IDxcLibrary> pLib;
    CComPtr<IDxcContainerReflection> pReflection;
    UINT32 index;

    VerifyCompileOK(dllSupport, source, profile, args, &pContainer);
    VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));
    VERIFY_SUCCEEDED(
        dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));
    VERIFY_SUCCEEDED(pReflection->Load(pContainer));
    VERIFY_SUCCEEDED(
        pReflection->FindFirstPartKind(hlsl::DFCC_ShaderDebugInfoDXIL, &index));
    VERIFY_SUCCEEDED(pReflection->GetPartContent(index, ppDebugPart));
  };

  constexpr wchar_t *profile = L"ps_6_0";
  CompileAndGetDebugPart(m_dllSupport, source, profile, &pPart);

  CComPtr<IStream> pNewProgramStream;
  VERIFY_SUCCEEDED(
      pLib->CreateStreamFromBlobReadOnly(pPart, &pNewProgramStream));

  CComPtr<IDiaDataSource> pDiaDataSource;
  VERIFY_SUCCEEDED(
      m_dllSupport.CreateInstance(CLSID_DxcDiaDataSource, &pDiaDataSource));

  VERIFY_SUCCEEDED(pDiaDataSource->loadDataFromIStream(pNewProgramStream));

  CComPtr<IDiaSession> pSession;
  VERIFY_SUCCEEDED(pDiaDataSource->openSession(&pSession));

  CComPtr<IDxcPixDxilDebugInfoFactory> factory;
  VERIFY_SUCCEEDED(pSession->QueryInterface(IID_PPV_ARGS(&factory)));

  CComPtr<IDxcPixCompilationInfo> compilationInfo;
  VERIFY_SUCCEEDED(factory->NewDxcPixCompilationInfo(&compilationInfo));

  CComBSTR arguments;
  VERIFY_SUCCEEDED(compilationInfo->GetArguments(&arguments));
  for (unsigned i = 0; i < _countof(FlagList); i++) {
    VERIFY_IS_TRUE(nullptr != wcsstr(arguments, FlagList[i]));
  }

  CComBSTR macros;
  VERIFY_SUCCEEDED(compilationInfo->GetMacroDefinitions(&macros));
  for (unsigned i = 0; i < _countof(DefineList); i++) {
    std::wstring MacroDef = std::wstring(L"-D") + DefineList[i];
    VERIFY_IS_TRUE(nullptr != wcsstr(macros, MacroDef.c_str()));
  }

  CComBSTR entryPointFile;
  VERIFY_SUCCEEDED(compilationInfo->GetEntryPointFile(&entryPointFile));
  VERIFY_ARE_EQUAL(std::wstring(L"source.hlsl"), std::wstring(entryPointFile));

  CComBSTR entryPointFunction;
  VERIFY_SUCCEEDED(compilationInfo->GetEntryPoint(&entryPointFunction));
  VERIFY_ARE_EQUAL(std::wstring(L"main"), std::wstring(entryPointFunction));

  CComBSTR hlslTarget;
  VERIFY_SUCCEEDED(compilationInfo->GetHlslTarget(&hlslTarget));
  VERIFY_ARE_EQUAL(std::wstring(profile), std::wstring(hlslTarget));
}
#endif // _WIN32 - exclude PIX stuff

#ifdef _WIN32

#pragma fenv_access(on)
#pragma optimize("", off)
#pragma warning(disable : 4723)

// Define test state as something weird that we can verify was restored
static const unsigned int fpTestState =
    (_MCW_EM & (~_EM_ZERODIVIDE)) |   // throw on div by zero
    _DN_FLUSH_OPERANDS_SAVE_RESULTS | // denorm flush operands & save results
    _RC_UP;                           // round up
static const unsigned int fpTestMask = _MCW_EM | _MCW_DN | _MCW_RC;

struct FPTestScope
{
  // _controlfp_s is non-standard and <cfenv> doesn't have a function to enable exceptions
  unsigned int fpSavedState;
  FPTestScope() {
    VERIFY_IS_TRUE(_controlfp_s(&fpSavedState, 0, 0) == 0);
    unsigned int newValue;
    VERIFY_IS_TRUE(_controlfp_s(&newValue, fpTestState, fpTestMask) == 0);
  }
  ~FPTestScope() {
    unsigned int newValue;
    errno_t error = _controlfp_s(&newValue, fpSavedState, fpTestMask);
    DXASSERT_LOCALVAR(error, error == 0, "Failed to restore floating-point environment.");
  }
};

void VerifyDivByZeroThrows() {
  bool bCaughtExpectedException = false;
  __try {
    float one = 1.0;
    float zero = 0.0;
    float val = one / zero;
    (void)val;
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    bCaughtExpectedException = true;
  }
  VERIFY_IS_TRUE(bCaughtExpectedException);
}

TEST_F(CompilerTest, CodeGenFloatingPointEnvironment) {
  unsigned int fpOriginal;
  VERIFY_IS_TRUE(_controlfp_s(&fpOriginal, 0, 0) == 0);

  {
    FPTestScope fpTestScope;
    // Get state before/after compilation, making sure it's our test state,
    // and that it is restored after the compile.
    unsigned int fpBeforeCompile;
    VERIFY_IS_TRUE(_controlfp_s(&fpBeforeCompile, 0, 0) == 0);
    VERIFY_ARE_EQUAL((fpBeforeCompile & fpTestMask), fpTestState);

    CodeGenTestCheck(L"fpexcept.hlsl");

    // Verify excpetion environment was restored
    unsigned int fpAfterCompile;
    VERIFY_IS_TRUE(_controlfp_s(&fpAfterCompile, 0, 0) == 0);
    VERIFY_ARE_EQUAL((fpBeforeCompile & fpTestMask), (fpAfterCompile & fpTestMask));

    // Make sure round up is set
    VERIFY_ARE_EQUAL(rint(12.25), 13);

    // Make sure we actually enabled div-by-zero exception
    VerifyDivByZeroThrows();
  }

  // Verify original state has been restored
  unsigned int fpLocal;
  VERIFY_IS_TRUE(_controlfp_s(&fpLocal, 0, 0) == 0);
  VERIFY_ARE_EQUAL(fpLocal, fpOriginal);
}

#pragma optimize("", on)

#else   // _WIN32

// Only implemented on Win32
TEST_F(CompilerTest, CodeGenFloatingPointEnvironment) {
  VERIFY_IS_TRUE(true);
}

#endif  // _WIN32

TEST_F(CompilerTest, CodeGenInclude) {
  CodeGenTestCheck(L"Include.hlsl");
}

TEST_F(CompilerTest, CodeGenLibCsEntry) {
  CodeGenTestCheck(L"lib_cs_entry.hlsl");
}

TEST_F(CompilerTest, CodeGenLibCsEntry2) {
  CodeGenTestCheck(L"lib_cs_entry2.hlsl");
}

TEST_F(CompilerTest, CodeGenLibCsEntry3) {
  CodeGenTestCheck(L"lib_cs_entry3.hlsl");
}

TEST_F(CompilerTest, CodeGenLibEntries) {
  CodeGenTestCheck(L"lib_entries.hlsl");
}

TEST_F(CompilerTest, CodeGenLibEntries2) {
  CodeGenTestCheck(L"lib_entries2.hlsl");
}

TEST_F(CompilerTest, CodeGenLibNoAlias) {
  CodeGenTestCheck(L"lib_no_alias.hlsl");
}

TEST_F(CompilerTest, CodeGenLibResource) {
  CodeGenTestCheck(L"lib_resource.hlsl");
}

TEST_F(CompilerTest, CodeGenLibUnusedFunc) {
  CodeGenTestCheck(L"lib_unused_func.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigProfile) {
  if (m_ver.SkipDxilVersion(1, 5)) return;
  CodeGenTest(L"rootSigProfile.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigProfile2) {
  if (m_ver.SkipDxilVersion(1, 5)) return;
  // TODO: Verify the result when reflect the structures.
  CodeGenTest(L"rootSigProfile2.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigProfile5) {
  if (m_ver.SkipDxilVersion(1, 5)) return;
  CodeGenTest(L"rootSigProfile5.hlsl");
}

TEST_F(CompilerTest, LibGVStore) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcContainerReflection> pReflection;
  CComPtr<IDxcAssembler> pAssembler;

  VERIFY_SUCCEEDED(this->m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));
  VERIFY_SUCCEEDED(this->m_dllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    R"(
      struct T {
      RWByteAddressBuffer outputBuffer;
      RWByteAddressBuffer outputBuffer2;
      };

      struct D {
        float4 a;
        int4 b;
      };

      struct T2 {
         RWStructuredBuffer<D> uav;
      };

      T2 resStruct(T t, uint2 id);

      RWByteAddressBuffer outputBuffer;
      RWByteAddressBuffer outputBuffer2;

      [numthreads(8, 8, 1)]
      void main( uint2 id : SV_DispatchThreadID )
      {
          T t = {outputBuffer,outputBuffer2};
          T2 t2 = resStruct(t, id);
          uint counter = t2.uav.IncrementCounter();
          t2.uav[counter].b.xy = id;
      }
    )", &pSource);

  const WCHAR *pArgs[] = {
    L"/Zi",
  };
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"file.hlsl", L"", L"lib_6_x",
                                         pArgs, _countof(pArgs), nullptr, 0, nullptr,
                                         &pResult));

  CComPtr<IDxcBlob> pShader;
  VERIFY_SUCCEEDED(pResult->GetResult(&pShader));
  VERIFY_SUCCEEDED(pReflection->Load(pShader));

  UINT32 index = 0;
  VERIFY_SUCCEEDED(pReflection->FindFirstPartKind(hlsl::DFCC_DXIL, &index));

  CComPtr<IDxcBlob> pBitcode;
  VERIFY_SUCCEEDED(pReflection->GetPartContent(index, &pBitcode));

  const char *bitcode = hlsl::GetDxilBitcodeData((hlsl::DxilProgramHeader *)pBitcode->GetBufferPointer());
  unsigned bitcode_size = hlsl::GetDxilBitcodeSize((hlsl::DxilProgramHeader *)pBitcode->GetBufferPointer());

  CComPtr<IDxcBlobEncoding> pBitcodeBlob;
  CreateBlobPinned(bitcode, bitcode_size, CP_UTF8, &pBitcodeBlob);

  CComPtr<IDxcBlob> pReassembled;
  CComPtr<IDxcOperationResult> pReassembleResult;
  VERIFY_SUCCEEDED(pAssembler->AssembleToContainer(pBitcodeBlob, &pReassembleResult));
  VERIFY_SUCCEEDED(pReassembleResult->GetResult(&pReassembled));

  CComPtr<IDxcBlobEncoding> pTextBlob;
  VERIFY_SUCCEEDED(pCompiler->Disassemble(pReassembled, &pTextBlob));

  std::wstring Text = BlobToUtf16(pTextBlob);
  VERIFY_ARE_NOT_EQUAL(std::wstring::npos, Text.find(L"store"));
}

TEST_F(CompilerTest, PreprocessWhenValidThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  DxcDefine defines[2];
  defines[0].Name = L"MYDEF";
  defines[0].Value = L"int";
  defines[1].Name = L"MYOTHERDEF";
  defines[1].Value = L"123";
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "// First line\r\n"
    "MYDEF g_int = MYOTHERDEF;\r\n"
    "#define FOO BAR\r\n"
    "int FOO;", &pSource);
  VERIFY_SUCCEEDED(pCompiler->Preprocess(pSource, L"file.hlsl", nullptr, 0,
                                         defines, _countof(defines), nullptr,
                                         &pResult));
  HRESULT hrOp;
  VERIFY_SUCCEEDED(pResult->GetStatus(&hrOp));
  VERIFY_SUCCEEDED(hrOp);

  CComPtr<IDxcBlob> pOutText;
  VERIFY_SUCCEEDED(pResult->GetResult(&pOutText));
  std::string text(BlobToUtf8(pOutText));
  VERIFY_ARE_EQUAL_STR(
    "#line 1 \"file.hlsl\"\n"
    "\n"
    "int g_int = 123;\n"
    "\n"
    "int BAR;\n", text.c_str());
}

TEST_F(CompilerTest, PreprocessWhenExpandTokenPastingOperandThenAccept) {
  // Tests that we can turn on fxc's behavior (pre-expanding operands before
  // performing token-pasting) using -flegacy-macro-expansion

  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  LPCWSTR expandOption = L"-flegacy-macro-expansion";

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));

  CreateBlobFromText(R"(
#define SET_INDEX0                10
#define BINDING_INDEX0            5

#define SET(INDEX)                SET_INDEX##INDEX
#define BINDING(INDEX)            BINDING_INDEX##INDEX

#define SET_BIND(NAME,SET,BIND)   resource_set_##SET##_bind_##BIND##_##NAME

#define RESOURCE(NAME,INDEX)      SET_BIND(NAME, SET(INDEX), BINDING(INDEX))

    Texture2D<float4> resource_set_10_bind_5_tex;

  float4 main() : SV_Target{
    return RESOURCE(tex, 0)[uint2(1, 2)];
  }
)",
                     &pSource);
  VERIFY_SUCCEEDED(pCompiler->Preprocess(pSource, L"file.hlsl", &expandOption,
                                         1, nullptr, 0, nullptr, &pResult));
  HRESULT hrOp;
  VERIFY_SUCCEEDED(pResult->GetStatus(&hrOp));
  VERIFY_SUCCEEDED(hrOp);

  CComPtr<IDxcBlob> pOutText;
  VERIFY_SUCCEEDED(pResult->GetResult(&pOutText));
  std::string text(BlobToUtf8(pOutText));
  VERIFY_ARE_EQUAL_STR(R"(#line 1 "file.hlsl"
#line 12 "file.hlsl"
    Texture2D<float4> resource_set_10_bind_5_tex;

  float4 main() : SV_Target{
    return resource_set_10_bind_5_tex[uint2(1, 2)];
  }
)",
                       text.c_str());
}

TEST_F(CompilerTest, WhenSigMismatchPCFunctionThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "struct PSSceneIn \n\
    { \n\
      float4 pos  : SV_Position; \n\
      float2 tex  : TEXCOORD0; \n\
      float3 norm : NORMAL; \n\
    }; \n"
    "struct HSPerPatchData {  \n\
      float edges[ 3 ] : SV_TessFactor; \n\
      float inside : SV_InsideTessFactor; \n\
      float foo : FOO; \n\
    }; \n"
    "HSPerPatchData HSPerPatchFunc( InputPatch< PSSceneIn, 3 > points, \n\
      OutputPatch<PSSceneIn, 3> outpoints) { \n\
      HSPerPatchData d = (HSPerPatchData)0; \n\
      d.edges[ 0 ] = points[0].tex.x + outpoints[0].tex.x; \n\
      d.edges[ 1 ] = 1; \n\
      d.edges[ 2 ] = 1; \n\
      d.inside = 1; \n\
      return d; \n\
    } \n"
    "[domain(\"tri\")] \n\
    [partitioning(\"fractional_odd\")] \n\
    [outputtopology(\"triangle_cw\")] \n\
    [patchconstantfunc(\"HSPerPatchFunc\")] \n\
    [outputcontrolpoints(3)] \n"
    "void main(const uint id : SV_OutputControlPointID, \n\
               const InputPatch< PSSceneIn, 3 > points ) { \n\
    } \n"
    , &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"hs_6_0", nullptr, 0, nullptr, 0, nullptr, &pResult));
  std::string failLog(VerifyOperationFailed(pResult));
  VERIFY_ARE_NOT_EQUAL(string::npos, failLog.find(
    "Signature element SV_Position, referred to by patch constant function, is not found in corresponding hull shader output."));
}

TEST_F(CompilerTest, SubobjectCodeGenErrors) {
  struct SubobjectErrorTestCase {
    const char *shaderText;
    const char *expectedError;
  };
  SubobjectErrorTestCase testCases[] = {
    { "GlobalRootSignature grs;",           "1:1: error: subobject needs to be initialized" },
    { "StateObjectConfig soc;",             "1:1: error: subobject needs to be initialized" },
    { "LocalRootSignature lrs;",            "1:1: error: subobject needs to be initialized" },
    { "SubobjectToExportsAssociation sea;", "1:1: error: subobject needs to be initialized" },
    { "RaytracingShaderConfig rsc;",        "1:1: error: subobject needs to be initialized" },
    { "RaytracingPipelineConfig rpc;",      "1:1: error: subobject needs to be initialized" },
    { "RaytracingPipelineConfig1 rpc1;",    "1:1: error: subobject needs to be initialized" },
    { "TriangleHitGroup hitGt;",            "1:1: error: subobject needs to be initialized" },
    { "ProceduralPrimitiveHitGroup hitGt;", "1:1: error: subobject needs to be initialized" },
    { "GlobalRootSignature grs2 = {\"\"};", "1:29: error: empty string not expected here" },
    { "LocalRootSignature lrs2 = {\"\"};",  "1:28: error: empty string not expected here" },
    { "SubobjectToExportsAssociation sea2 = { \"\", \"x\" };", "1:40: error: empty string not expected here" },
    { "string s; SubobjectToExportsAssociation sea4 = { \"x\", s };", "1:55: error: cannot convert to constant string" },
    { "extern int v; RaytracingPipelineConfig rpc2 = { v + 16 };", "1:49: error: cannot convert to constant unsigned int" },
    { "string s; TriangleHitGroup trHitGt2_8 = { s, \"foo\" };", "1:43: error: cannot convert to constant string" },
    { "string s; ProceduralPrimitiveHitGroup ppHitGt2_8 = { s, \"\", s };", "1:54: error: cannot convert to constant string" },
    { "ProceduralPrimitiveHitGroup ppHitGt2_9 = { \"a\", \"b\", \"\"};", "1:54: error: empty string not expected here" }
  };

  for (unsigned i = 0; i < _countof(testCases); i++) {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));

    CreateBlobFromText(testCases[i].shaderText, &pSource);
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"", L"lib_6_4", nullptr, 0, nullptr, 0, nullptr, &pResult));
    std::string failLog(VerifyOperationFailed(pResult));
    VERIFY_ARE_NOT_EQUAL(string::npos, failLog.find(testCases[i].expectedError));
  }
}

#ifdef _WIN32
TEST_F(CompilerTest, ManualFileCheckTest) {
#else
TEST_F(CompilerTest, DISABLED_ManualFileCheckTest) {
#endif
  using namespace llvm;
  using namespace WEX::TestExecution;

  WEX::Common::String value;
  VERIFY_SUCCEEDED(RuntimeParameters::TryGetValue(L"InputPath", value));

  std::wstring path = value;
  if (!llvm::sys::path::is_absolute(CW2A(path.c_str()).m_psz)) {
    path = hlsl_test::GetPathToHlslDataFile(path.c_str());
  }

  bool isDirectory;
  {
    // Temporarily setup the filesystem for testing whether the path is a directory.
    // If it is, CodeGenTestCheckBatchDir will create its own instance.
    llvm::sys::fs::MSFileSystem *msfPtr;
    VERIFY_SUCCEEDED(CreateMSFileSystemForDisk(&msfPtr));
    std::unique_ptr<llvm::sys::fs::MSFileSystem> msf(msfPtr);
    llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    IFTLLVM(pts.error_code());
    isDirectory = llvm::sys::fs::is_directory(CW2A(path.c_str()).m_psz);
  }

  if (isDirectory) {
    CodeGenTestCheckBatchDir(path, /*implicitDir*/ false);
  } else {
    CodeGenTestCheck(path.c_str(), /*implicitDir*/ false);
  }
}


TEST_F(CompilerTest, CodeGenHashStability) {
  CodeGenTestCheckBatchHash(L"");
}

TEST_F(CompilerTest, BatchD3DReflect) {
  CodeGenTestCheckBatchDir(L"d3dreflect");
}

TEST_F(CompilerTest, BatchDxil) {
  CodeGenTestCheckBatchDir(L"dxil");
}

TEST_F(CompilerTest, BatchHLSL) {
  CodeGenTestCheckBatchDir(L"hlsl");
}

TEST_F(CompilerTest, BatchInfra) {
  CodeGenTestCheckBatchDir(L"infra");
}

TEST_F(CompilerTest, BatchPasses) {
  CodeGenTestCheckBatchDir(L"passes");
}

TEST_F(CompilerTest, BatchShaderTargets) {
  CodeGenTestCheckBatchDir(L"shader_targets");
}

TEST_F(CompilerTest, BatchValidation) {
  CodeGenTestCheckBatchDir(L"validation");
}

TEST_F(CompilerTest, BatchSamples) {
  CodeGenTestCheckBatchDir(L"samples");
}
