///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PixTest.cpp                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides tests for the PIX-specific components                            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// This whole file is win32-only
#ifdef _WIN32


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
#include <atlfile.h>
#include "dia2.h"

#include "dxc/DXIL/DxilModule.h"

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
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringSwitch.h"


#include <../lib/DxilDia/DxilDiaSession.h>
#include <../lib/DxilDia/DxcPixLiveVariables.h>
#include <../lib/DxilDia/DxcPixLiveVariables_FragmentIterator.h>
#include <dxc/DxilPIXPasses/DxilPIXVirtualRegisters.h>

using namespace std;
using namespace hlsl;
using namespace hlsl_test;

// Aligned to SymTagEnum.
const char *SymTagEnumText[] = {
    "Null",               // SymTagNull
    "Exe",                // SymTagExe
    "Compiland",          // SymTagCompiland
    "CompilandDetails",   // SymTagCompilandDetails
    "CompilandEnv",       // SymTagCompilandEnv
    "Function",           // SymTagFunction
    "Block",              // SymTagBlock
    "Data",               // SymTagData
    "Annotation",         // SymTagAnnotation
    "Label",              // SymTagLabel
    "PublicSymbol",       // SymTagPublicSymbol
    "UDT",                // SymTagUDT
    "Enum",               // SymTagEnum
    "FunctionType",       // SymTagFunctionType
    "PointerType",        // SymTagPointerType
    "ArrayType",          // SymTagArrayType
    "BaseType",           // SymTagBaseType
    "Typedef",            // SymTagTypedef
    "BaseClass",          // SymTagBaseClass
    "Friend",             // SymTagFriend
    "FunctionArgType",    // SymTagFunctionArgType
    "FuncDebugStart",     // SymTagFuncDebugStart
    "FuncDebugEnd",       // SymTagFuncDebugEnd
    "UsingNamespace",     // SymTagUsingNamespace
    "VTableShape",        // SymTagVTableShape
    "VTable",             // SymTagVTable
    "Custom",             // SymTagCustom
    "Thunk",              // SymTagThunk
    "CustomType",         // SymTagCustomType
    "ManagedType",        // SymTagManagedType
    "Dimension",          // SymTagDimension
    "CallSite",           // SymTagCallSite
    "InlineSite",         // SymTagInlineSite
    "BaseInterface",      // SymTagBaseInterface
    "VectorType",         // SymTagVectorType
    "MatrixType",         // SymTagMatrixType
    "HLSLType",           // SymTagHLSLType
    "Caller",             // SymTagCaller
    "Callee",             // SymTagCallee
    "Export",             // SymTagExport
    "HeapAllocationSite", // SymTagHeapAllocationSite
    "CoffGroup",          // SymTagCoffGroup
};

// Aligned to LocationType.
const char* LocationTypeText[] =
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
const char* DataKindText[] =
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
const char* UdtKindText[] =
{
  "Struct",
  "Class",
  "Union",
  "Interface",
};

static std::vector<std::string> Tokenize(const std::string &str,
                                         const char *delimiters) {
  std::vector<std::string> tokens;
  std::string copy = str;

  for (auto i = strtok(&copy[0], delimiters); i != nullptr;
       i = strtok(nullptr, delimiters)) {
    tokens.push_back(i);
  }

  return tokens;
}

class PixTest {
public:
  BEGIN_TEST_CLASS(PixTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  TEST_METHOD(CompileWhenDebugThenDIPresent)
  
  TEST_METHOD(CompileDebugLines)
  TEST_METHOD(CompileDebugPDB)
  TEST_METHOD(CompileDebugDisasmPDB)
  
  TEST_METHOD(DiaLoadBadBitcodeThenFail)
  TEST_METHOD(DiaLoadDebugThenOK)
  TEST_METHOD(DiaTableIndexThenOK)
  TEST_METHOD(DiaLoadDebugSubrangeNegativeThenOK)
  TEST_METHOD(DiaLoadRelocatedBitcode)
  TEST_METHOD(DiaLoadBitcodePlusExtraData)
  TEST_METHOD(DiaCompileArgs)
  TEST_METHOD(PixDebugCompileInfo)

  TEST_METHOD(PixStructAnnotation_Simple)
  TEST_METHOD(PixStructAnnotation_CopiedStruct)
  TEST_METHOD(PixStructAnnotation_MixedSizes)
  TEST_METHOD(PixStructAnnotation_StructWithinStruct)
  TEST_METHOD(PixStructAnnotation_1DArray)
  TEST_METHOD(PixStructAnnotation_2DArray)
  TEST_METHOD(PixStructAnnotation_EmbeddedArray)
  TEST_METHOD(PixStructAnnotation_FloatN)
  TEST_METHOD(PixStructAnnotation_SequentialFloatN)
  TEST_METHOD(PixStructAnnotation_EmbeddedFloatN)
  TEST_METHOD(PixStructAnnotation_Matrix)
  TEST_METHOD(PixStructAnnotation_MemberFunction)
  TEST_METHOD(PixStructAnnotation_BigMess)

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

  HRESULT CreateContainerBuilder(IDxcContainerBuilder **ppResult) {
    return m_dllSupport.CreateInstance(CLSID_DxcContainerBuilder, ppResult);
  }

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
 
  std::string GetOption(std::string &cmd, char *opt) {
    std::string option = cmd.substr(cmd.find(opt));
    option = option.substr(option.find_first_of(' '));
    option = option.substr(option.find_first_not_of(' '));
    return option.substr(0, option.find_first_of(' '));
  }

  HRESULT CreateDiaSourceForCompile(const char* hlsl,
    IDiaDataSource** ppDiaSource)
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
    
    HRESULT compilationStatus;
    VERIFY_SUCCEEDED(pResult->GetStatus(&compilationStatus));
    if (FAILED(compilationStatus))
    {
      CComPtr<IDxcBlobEncoding> pErrros;
      VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrros));
      CA2W errorTextW(static_cast<const char *>(pErrros->GetBufferPointer()), CP_UTF8);
      WEX::Logging::Log::Error(errorTextW);
    }

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
  
  CComPtr<IDxcOperationResult> Compile(
    const char* hlsl,
    const wchar_t* target)
  {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;

    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
    CreateBlobFromText(hlsl, &pSource);
    LPCWSTR args[] = { L"/Zi", L"/Od", L"-enable-16bit-types", L"/Qembed_debug" };
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
      target, args, _countof(args), nullptr, 0, nullptr, &pResult));

    HRESULT compilationStatus;
    VERIFY_SUCCEEDED(pResult->GetStatus(&compilationStatus));
    if (FAILED(compilationStatus))
    {
      CComPtr<IDxcBlobEncoding> pErrros;
      VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrros));
      CA2W errorTextW(static_cast<const char*>(pErrros->GetBufferPointer()), CP_UTF8);
      WEX::Logging::Log::Error(errorTextW);
    }

#if 0 //handy for debugging
    {
      CComPtr<IDxcBlob> pProgram;
      CheckOperationSucceeded(pResult, &pProgram);

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
          pProgram, pBitcode - (char *)pProgram->GetBufferPointer(),
          bitcodeLength, &pProgramPdb));

      CComPtr<IDxcBlobEncoding> pDbgDisassembly;
      VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgramPdb, &pDbgDisassembly));
      std::string disText = BlobToUtf8(pDbgDisassembly);
      CA2W disTextW(disText.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(disTextW);
    }
#endif

    return pResult;
  }

  CComPtr<IDxcBlob> ExtractDxilPart(IDxcBlob *pProgram) {
    CComPtr<IDxcLibrary> pLib;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));
    const hlsl::DxilContainerHeader *pContainer = hlsl::IsDxilContainerLike(
        pProgram->GetBufferPointer(), pProgram->GetBufferSize());
    VERIFY_IS_NOT_NULL(pContainer);
    hlsl::DxilPartIterator partIter =
        std::find_if(hlsl::begin(pContainer), hlsl::end(pContainer),
                     hlsl::DxilPartIsType(hlsl::DFCC_DXIL));
    const hlsl::DxilProgramHeader *pProgramHeader =
        (const hlsl::DxilProgramHeader *)hlsl::GetDxilPartData(*partIter);
    uint32_t bitcodeLength;
    const char *pBitcode;
    CComPtr<IDxcBlob> pDxilBits;
    hlsl::GetDxilProgramBitcode(pProgramHeader, &pBitcode, &bitcodeLength);
    VERIFY_SUCCEEDED(pLib->CreateBlobFromBlob(
        pProgram, pBitcode - (char *)pProgram->GetBufferPointer(),
        bitcodeLength, &pDxilBits));
    return pDxilBits;
  }

  struct ValueLocation
  {
    int base;
    int count;
  };

  struct PassOutput
  {
    CComPtr<IDxcBlob> blob;
    std::vector<ValueLocation> valueLocations;
  };

  PassOutput RunAnnotationPasses(IDxcBlob * dxil)
  {
    CComPtr<IDxcOptimizer> pOptimizer;
    VERIFY_SUCCEEDED(
        m_dllSupport.CreateInstance(CLSID_DxcOptimizer, &pOptimizer));
    std::vector<LPCWSTR> Options;
    Options.push_back(L"-opt-mod-passes");
    Options.push_back(L"-dxil-dbg-value-to-dbg-declare");
    Options.push_back(L"-dxil-annotate-with-virtual-regs");

    CComPtr<IDxcBlob> pOptimizedModule;
    CComPtr<IDxcBlobEncoding> pText;
    VERIFY_SUCCEEDED(pOptimizer->RunOptimizer(
        dxil, Options.data(), Options.size(), &pOptimizedModule, &pText));

    std::string outputText;
    if (pText->GetBufferSize() != 0)
    {
      outputText = reinterpret_cast<const char*>(pText->GetBufferPointer());
    }

    auto lines = Tokenize(outputText, "\n");

    std::vector<ValueLocation> valueLocations;

    for (size_t line = 0; line < lines.size(); ++line) {
      if (lines[line] == "Begin - dxil values to virtual register mapping") {
        for (++line; line < lines.size(); ++line) {
          if (lines[line] == "End - dxil values to virtual register mapping") {
            break;
          }

          auto lineTokens = Tokenize(lines[line], " ");
          VERIFY_IS_TRUE(lineTokens.size() >= 2);
          if (lineTokens[1] == "dxil")
          {
            VERIFY_IS_TRUE(lineTokens.size() == 3);
            valueLocations.push_back({atoi(lineTokens[2].c_str()), 1});
          }
          else if (lineTokens[1] == "alloca")
          {
            VERIFY_IS_TRUE(lineTokens.size() == 4);
            valueLocations.push_back(
                {atoi(lineTokens[2].c_str()), atoi(lineTokens[3].c_str())});
          }
          else
          {
            VERIFY_IS_TRUE(false);
          }
        }
      }
    }

    return { std::move(pOptimizedModule), std::move(valueLocations) };
  }

  std::wstring Disassemble(IDxcBlob * pProgram)
  {
    CComPtr<IDxcCompiler> pCompiler;
    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));

    CComPtr<IDxcBlobEncoding> pDbgDisassembly;
    VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDbgDisassembly));
    std::string disText = BlobToUtf8(pDbgDisassembly);
    CA2W disTextW(disText.c_str(), CP_UTF8);
    return std::wstring(disTextW);
  }

  CComPtr<IDxcBlob> FindModule(hlsl::DxilFourCC fourCC, IDxcBlob *pSource)
  {
    const UINT32 BC_C0DE = ((INT32)(INT8)'B' | (INT32)(INT8)'C' << 8 |
                            (INT32)0xDEC0 << 16); // BC0xc0de in big endian
    const char *pBitcode = nullptr;
    const hlsl::DxilPartHeader *pDxilPartHeader =
        (hlsl::DxilPartHeader *)
            pSource->GetBufferPointer(); // Initialize assuming that source is
                                         // starting with DXIL part

    if (BC_C0DE == *(UINT32 *)pSource->GetBufferPointer()) {
      return pSource;
    }
    if (hlsl::IsValidDxilContainer(
            (hlsl::DxilContainerHeader *)pSource->GetBufferPointer(),
            pSource->GetBufferSize())) {
      hlsl::DxilContainerHeader *pDxilContainerHeader =
          (hlsl::DxilContainerHeader *)pSource->GetBufferPointer();
      pDxilPartHeader =
          *std::find_if(begin(pDxilContainerHeader), end(pDxilContainerHeader),
                        hlsl::DxilPartIsType(fourCC));
    }
    if (fourCC == pDxilPartHeader->PartFourCC) {
      UINT32 pBlobSize;
      hlsl::DxilProgramHeader *pDxilProgramHeader =
          (hlsl::DxilProgramHeader *)(pDxilPartHeader + 1);
      hlsl::GetDxilProgramBitcode(pDxilProgramHeader, &pBitcode, &pBlobSize);
      UINT32 offset =
          (UINT32)(pBitcode - (const char *)pSource->GetBufferPointer());
      CComPtr<IDxcLibrary> library;
      IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
      CComPtr<IDxcBlob> targetBlob;
      library->CreateBlobFromBlob(pSource, offset, pBlobSize, &targetBlob);
      return targetBlob;
    }
    return {};
  }


  void ReplaceDxilBlobPart(
      const void *originalShaderBytecode, SIZE_T originalShaderLength,
      IDxcBlob *pNewDxilBlob, IDxcBlob **ppNewShaderOut)
  {
    CComPtr<IDxcLibrary> pLibrary;
    IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));

    CComPtr<IDxcBlob> pNewContainer;

    // Use the container assembler to build a new container from the
    // recently-modified DXIL bitcode. This container will contain new copies of
    // things like input signature etc., which will supersede the ones from the
    // original compiled shader's container.
    {
      CComPtr<IDxcAssembler> pAssembler;
      IFT(m_dllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));

      CComPtr<IDxcOperationResult> pAssembleResult;
      VERIFY_SUCCEEDED(
          pAssembler->AssembleToContainer(pNewDxilBlob, &pAssembleResult));

      CComPtr<IDxcBlobEncoding> pAssembleErrors;
      VERIFY_SUCCEEDED(
          pAssembleResult->GetErrorBuffer(&pAssembleErrors));

      if (pAssembleErrors && pAssembleErrors->GetBufferSize() != 0) {
        OutputDebugStringA(
            static_cast<LPCSTR>(pAssembleErrors->GetBufferPointer()));
        VERIFY_SUCCEEDED(E_FAIL);
      }

      VERIFY_SUCCEEDED(pAssembleResult->GetResult(&pNewContainer));
    }

    // Now copy over the blobs from the original container that won't have been
    // invalidated by changing the shader code itself, using the container
    // reflection API
    { 
      // Wrap the original code in a container blob
      CComPtr<IDxcBlobEncoding> pContainer;
      VERIFY_SUCCEEDED(
          pLibrary->CreateBlobWithEncodingFromPinned(
              static_cast<LPBYTE>(const_cast<void *>(originalShaderBytecode)),
              static_cast<UINT32>(originalShaderLength), CP_ACP, &pContainer));

      CComPtr<IDxcContainerReflection> pReflection;
      IFT(m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));

      // Load the reflector from the original shader
      VERIFY_SUCCEEDED(pReflection->Load(pContainer));

      UINT32 partIndex;

      if (SUCCEEDED(pReflection->FindFirstPartKind(hlsl::DFCC_PrivateData,
                                                   &partIndex))) {
        CComPtr<IDxcBlob> pPart;
        VERIFY_SUCCEEDED(
            pReflection->GetPartContent(partIndex, &pPart));

        CComPtr<IDxcContainerBuilder> pContainerBuilder;
        IFT(m_dllSupport.CreateInstance(CLSID_DxcContainerBuilder,
                                        &pContainerBuilder));

        VERIFY_SUCCEEDED(
            pContainerBuilder->Load(pNewContainer));

        VERIFY_SUCCEEDED(
            pContainerBuilder->AddPart(hlsl::DFCC_PrivateData, pPart));

        CComPtr<IDxcOperationResult> pBuildResult;

        VERIFY_SUCCEEDED(
            pContainerBuilder->SerializeContainer(&pBuildResult));

        CComPtr<IDxcBlobEncoding> pBuildErrors;
        VERIFY_SUCCEEDED(
            pBuildResult->GetErrorBuffer(&pBuildErrors));

        if (pBuildErrors && pBuildErrors->GetBufferSize() != 0) {
          OutputDebugStringA(
              reinterpret_cast<LPCSTR>(pBuildErrors->GetBufferPointer()));
          VERIFY_SUCCEEDED(E_FAIL);
        }

        VERIFY_SUCCEEDED(
            pBuildResult->GetResult(&pNewContainer));
      }
    }

    *ppNewShaderOut = pNewContainer.Detach();
  }

  class ModuleAndHangersOn
  {
    std::unique_ptr<llvm::LLVMContext> llvmContext;
    std::unique_ptr<llvm::Module> llvmModule;
    DxilModule* dxilModule;

  public:
    ModuleAndHangersOn(IDxcBlob* pBlob)
    {
      // Verify we have a valid dxil container.
      const DxilContainerHeader *pContainer = IsDxilContainerLike(
          pBlob->GetBufferPointer(), pBlob->GetBufferSize());
      VERIFY_IS_NOT_NULL(pContainer);
      VERIFY_IS_TRUE(IsValidDxilContainer(pContainer, pBlob->GetBufferSize()));

      // Get Dxil part from container.
      DxilPartIterator it =
          std::find_if(begin(pContainer), end(pContainer),
                       DxilPartIsType(DFCC_ShaderDebugInfoDXIL));
      VERIFY_IS_FALSE(it == end(pContainer));

      const DxilProgramHeader *pProgramHeader =
          reinterpret_cast<const DxilProgramHeader *>(GetDxilPartData(*it));
      VERIFY_IS_TRUE(IsValidDxilProgramHeader(pProgramHeader, (*it)->PartSize));

      // Get a pointer to the llvm bitcode.
      const char *pIL;
      uint32_t pILLength;
      GetDxilProgramBitcode(pProgramHeader, &pIL, &pILLength);

      // Parse llvm bitcode into a module.
      std::unique_ptr<llvm::MemoryBuffer> pBitcodeBuf(
          llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(pIL, pILLength), "",
                                           false));

      llvmContext.reset(new llvm::LLVMContext);

      llvm::ErrorOr<std::unique_ptr<llvm::Module>> pModule(
          llvm::parseBitcodeFile(pBitcodeBuf->getMemBufferRef(),
                                 *llvmContext));
      if (std::error_code ec = pModule.getError()) {
        VERIFY_FAIL();
      }

      llvmModule = std::move(pModule.get());

      dxilModule =
          DxilModule::TryGetDxilModule(llvmModule.get());
    }

    DxilModule& GetDxilModule()
    {
      return *dxilModule;
    }
  };

  struct AggregateOffsetAndSize
  {
    unsigned countOfMembers;
    unsigned offset;
    unsigned size;
  };
  struct AllocaWrite {
    std::string memberName;
    uint32_t regBase;
    uint32_t regSize;
    uint64_t index;
  };
  struct TestableResults
  {
    std::vector<AggregateOffsetAndSize> OffsetAndSizes;
    std::vector<AllocaWrite> AllocaWrites;
  };

  TestableResults TestStructAnnotationCase(const char* hlsl);
  void ValidateAllocaWrite(std::vector<AllocaWrite> const& allocaWrites, size_t index, const char* name);

};


bool PixTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    m_ver.Initialize(m_dllSupport);
  }
  return true;
}

TEST_F(PixTest, CompileWhenDebugThenDIPresent) {
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

TEST_F(PixTest, CompileDebugDisasmPDB) {
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
TEST_F(PixTest, CompileDebugPDB) {
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

TEST_F(PixTest, CompileDebugLines) {
  CComPtr<IDiaDataSource> pDiaSource;
  VERIFY_SUCCEEDED(
      CreateDiaSourceForCompile(
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

TEST_F(PixTest, DiaLoadBadBitcodeThenFail) {
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

static const char EmptyCompute[] = "[numthreads(8,8,1)] void main() { }";

static void CompileTestAndLoadDia(dxc::DxcDllSupport &dllSupport, IDiaDataSource **ppDataSource) {
  CompileTestAndLoadDiaSource(dllSupport, EmptyCompute, L"cs_6_0", ppDataSource);
}

TEST_F(PixTest, DiaLoadDebugSubrangeNegativeThenOK) {
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

TEST_F(PixTest, DiaLoadRelocatedBitcode) {

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

TEST_F(PixTest, DiaCompileArgs) {
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

TEST_F(PixTest, DiaLoadBitcodePlusExtraData) {
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

TEST_F(PixTest, DiaLoadDebugThenOK) {
  CompileTestAndLoadDia(m_dllSupport, nullptr);
}

TEST_F(PixTest, DiaTableIndexThenOK) {
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

TEST_F(PixTest, PixDebugCompileInfo) {
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

// This function lives in lib\DxilPIXPasses\DxilAnnotateWithVirtualRegister.cpp
// Declared here so we can test it.
uint32_t CountStructMembers(llvm::Type const* pType);

PixTest::TestableResults PixTest::TestStructAnnotationCase(const char* hlsl)
{
  auto pOperationResult = Compile(hlsl, L"as_6_5");
  CComPtr<IDxcBlob> pBlob;
  CheckOperationSucceeded(pOperationResult, &pBlob);

  CComPtr<IDxcBlob> pDxil = FindModule(DFCC_ShaderDebugInfoDXIL, pBlob);

  PassOutput passOutput = RunAnnotationPasses(pDxil);

  auto pAnnotated = passOutput.blob;

  CComPtr<IDxcBlob> pAnnotatedContainer;
  ReplaceDxilBlobPart(
    pBlob->GetBufferPointer(),
    pBlob->GetBufferSize(),
    pAnnotated,
    &pAnnotatedContainer);

  ModuleAndHangersOn moduleEtc(pAnnotatedContainer);
  
  llvm::Function *entryFunction = moduleEtc.GetDxilModule().GetEntryFunction();

  PixTest::TestableResults ret;

  // For every dbg.declare, run the member iterator and record what it finds:
  for (auto& block : entryFunction->getBasicBlockList())
  {
    for (auto& instruction : block.getInstList())
    {
      if (auto* dbgDeclare = llvm::dyn_cast<llvm::DbgDeclareInst>(&instruction))
      {
        llvm::Value* Address = dbgDeclare->getAddress();
        auto* AddressAsAlloca = llvm::dyn_cast<llvm::AllocaInst>(Address);
        if (AddressAsAlloca != nullptr)
        {
            auto* Expression = dbgDeclare->getExpression();

            std::unique_ptr<dxil_debug_info::MemberIterator> iterator = dxil_debug_info::CreateMemberIterator(
                dbgDeclare,
                moduleEtc.GetDxilModule().GetModule()->getDataLayout(),
                AddressAsAlloca,
                Expression);

            unsigned int startingBit = 0;
            unsigned int coveredBits = 0;
            unsigned int memberIndex = 0;
            unsigned int memberCount = 0;
            while (iterator->Next(&memberIndex))
            {
                memberCount++;
                if (memberIndex == 0)
                {
                    startingBit = iterator->OffsetInBits(memberIndex);
                    coveredBits = iterator->SizeInBits(memberIndex);
                }
                else
                {
                    coveredBits = std::max<unsigned int>(coveredBits, iterator->OffsetInBits(memberIndex) + iterator->SizeInBits(memberIndex));
                }
            }

            ret.OffsetAndSizes.push_back({ memberCount, startingBit, coveredBits });

            // Use this independent count of number of struct members to test the 
            // function that operates on the alloca type:
            llvm::Type* pAllocaTy = AddressAsAlloca->getType()->getElementType();
            if (auto* AT = llvm::dyn_cast<llvm::ArrayType>(pAllocaTy))
            {
                // This is the case where a struct is passed to a function, and in 
                // these tests there should be only one struct behind the pointer.
                VERIFY_ARE_EQUAL(AT->getNumElements(), 1);
                pAllocaTy = AT->getArrayElementType();
            }

            if (auto* ST = llvm::dyn_cast<llvm::StructType>(pAllocaTy))
            {
                uint32_t countOfMembers = CountStructMembers(ST);
                // memberIndex might be greater, because the fragment iterator also includes contained derived types as
                // fragments, in addition to the members of that contained derived types. CountStructMembers only counts
                // the leaf-node types.
                VERIFY_ARE_EQUAL(countOfMembers, memberCount);
            }
            else if (pAllocaTy->isFloatingPointTy() || pAllocaTy->isIntegerTy())
            {
                // If there's only one member in the struct in the pass-to-function (by pointer)
                // case, then the underlying type will have been reduced to the contained type.
                VERIFY_ARE_EQUAL(1, memberCount);
            }
            else
            {
                VERIFY_IS_TRUE(false);
            }
        }
      }
    }
  }

  // The member iterator should find a solid run of bits that is exactly covered
  // by exactly one of the members found by the annotation pass:
  for (auto const& cover : ret.OffsetAndSizes)
  {
    bool found = false;
    for (auto const& valueLocation : passOutput.valueLocations)
    {
      constexpr unsigned int eightBitsPerByte = 8;
      if (valueLocation.base * eightBitsPerByte == cover.offset)
      {
        VERIFY_IS_FALSE(found);
        found = true;
        VERIFY_ARE_EQUAL(valueLocation.count, cover.countOfMembers);
      }
    }
    VERIFY_IS_TRUE(found);
  }

  // For every store operation to the struct alloca, check that the annotation pass correctly determined which alloca
  for (auto& block : entryFunction->getBasicBlockList()) {
    for (auto& instruction : block.getInstList()) {
      if (auto* store =
        llvm::dyn_cast<llvm::StoreInst>(&instruction)) {

        if (auto* pGEP = llvm::dyn_cast<llvm::GetElementPtrInst>(store->getPointerOperand()))
        {
          ret.AllocaWrites.push_back({});
          auto& NewAllocaWrite = ret.AllocaWrites.back();
          llvm::Value* pPointerOperand = pGEP->getPointerOperand();
          if (auto* pGEP2 = llvm::dyn_cast<llvm::GetElementPtrInst>(pPointerOperand))
          {
            auto *pMemberIndex = llvm::dyn_cast<llvm::ConstantInt>(
                pGEP->getOperand(2));
            uint64_t memberIndex = pMemberIndex->getLimitedValue();
            // Until we have debugging info for floatN, matrixNxM etc., we can't get the name:
            // auto *secondPointer = pGEP2->getPointerOperandType();
            // auto* pStruct =
            // llvm::dyn_cast<llvm::StructType>(secondPointer->getVectorElementType());
            NewAllocaWrite.memberName =
                "member" + std::to_string(memberIndex);
          }
          else
          {
            NewAllocaWrite.memberName = pGEP->getName();
          }

          llvm::Value* index;
          if (pix_dxil::PixAllocaRegWrite::FromInst(
            store, 
            &NewAllocaWrite.regBase, 
            &NewAllocaWrite.regSize,
            &index)) {
            auto* asInt = llvm::dyn_cast<llvm::ConstantInt>(index);
            NewAllocaWrite.index = asInt->getLimitedValue();
          }
        }
      }
    }
  }

  return ret;
}

void PixTest::ValidateAllocaWrite(std::vector<AllocaWrite> const &allocaWrites,
                                  size_t index, const char *name) {
  VERIFY_ARE_EQUAL(index, allocaWrites[index].regBase + allocaWrites[index].index);
#if DBG
  // Compilation may add a prefix to the struct member name:
  VERIFY_IS_TRUE(0 == strncmp(name, allocaWrites[index].memberName.c_str(), strlen(name)));
#endif
}


TEST_F(PixTest, PixStructAnnotation_Simple) {
  if (m_ver.SkipDxilVersion(1, 5)) return;

  const char *hlsl = R"(
struct smallPayload
{
    uint dummy;
};


[numthreads(1, 1, 1)]
void main()
{
    smallPayload p;
    p.dummy = 42;
    DispatchMesh(1, 1, 1, p);
}
)";

  auto Testables = TestStructAnnotationCase(hlsl);

  VERIFY_ARE_EQUAL(1, Testables.OffsetAndSizes.size());
  VERIFY_ARE_EQUAL(1, Testables.OffsetAndSizes[0].countOfMembers);
  VERIFY_ARE_EQUAL(0, Testables.OffsetAndSizes[0].offset);  
  VERIFY_ARE_EQUAL(32, Testables.OffsetAndSizes[0].size);

  VERIFY_ARE_EQUAL(1, Testables.AllocaWrites.size());
  ValidateAllocaWrite(Testables.AllocaWrites, 0, "dummy");
}


TEST_F(PixTest, PixStructAnnotation_CopiedStruct) {
  if (m_ver.SkipDxilVersion(1, 5)) return;

  const char *hlsl = R"(
struct smallPayload
{
    uint dummy;
};


[numthreads(1, 1, 1)]
void main()
{
    smallPayload p;
    p.dummy = 42;
    smallPayload p2 = p;
    DispatchMesh(1, 1, 1, p2);
}
)";

  auto Testables = TestStructAnnotationCase(hlsl);

  VERIFY_ARE_EQUAL(2, Testables.OffsetAndSizes.size());
  VERIFY_ARE_EQUAL(1, Testables.OffsetAndSizes[0].countOfMembers);
  VERIFY_ARE_EQUAL(0, Testables.OffsetAndSizes[0].offset);  
  VERIFY_ARE_EQUAL(32, Testables.OffsetAndSizes[0].size);

  VERIFY_ARE_EQUAL(2, Testables.AllocaWrites.size());
  // The values in the copy don't have stable names:
  ValidateAllocaWrite(Testables.AllocaWrites, 0, "");
}

TEST_F(PixTest, PixStructAnnotation_MixedSizes) {
  if (m_ver.SkipDxilVersion(1, 5)) return;

  const char *hlsl = R"(
struct smallPayload
{
    bool b1;
    uint16_t sixteen;
    uint32_t thirtytwo;
    uint64_t sixtyfour;
};


[numthreads(1, 1, 1)]
void main()
{
    smallPayload p;
    p.b1 = true;
    p.sixteen = 16;
    p.thirtytwo = 32;
    p.sixtyfour = 64;
    DispatchMesh(1, 1, 1, p);
}
)";

  auto Testables = TestStructAnnotationCase(hlsl);

  VERIFY_ARE_EQUAL(1, Testables.OffsetAndSizes.size());
  VERIFY_ARE_EQUAL(4, Testables.OffsetAndSizes[0].countOfMembers);
  VERIFY_ARE_EQUAL(0, Testables.OffsetAndSizes[0].offset);
  // 8 bytes align for uint64_t:
  VERIFY_ARE_EQUAL(32 + 16 + 16 /*alignment for next field*/ + 32 + 32/*alignment for max align*/ + 64,
                   Testables.OffsetAndSizes[0].size);

  VERIFY_ARE_EQUAL(4, Testables.AllocaWrites.size());
  ValidateAllocaWrite(Testables.AllocaWrites, 0, "b1");
  ValidateAllocaWrite(Testables.AllocaWrites, 1, "sixteen");
  ValidateAllocaWrite(Testables.AllocaWrites, 2, "thirtytwo");
  ValidateAllocaWrite(Testables.AllocaWrites, 3, "sixtyfour");
}

TEST_F(PixTest, PixStructAnnotation_StructWithinStruct) {
  if (m_ver.SkipDxilVersion(1, 5)) return;

  const char *hlsl = R"(

struct Contained
{
  uint32_t one;
  uint32_t two;
};

struct smallPayload
{
  uint32_t before;
  Contained contained;
  uint32_t after;
};


[numthreads(1, 1, 1)]
void main()
{
    smallPayload p;
    p.before = 0xb4;
    p.contained.one = 1;
    p.contained.two = 2;
    p.after = 3;
    DispatchMesh(1, 1, 1, p);
}
)";

  auto Testables = TestStructAnnotationCase(hlsl);

  VERIFY_ARE_EQUAL(1, Testables.OffsetAndSizes.size());
  VERIFY_ARE_EQUAL(4, Testables.OffsetAndSizes[0].countOfMembers);
  VERIFY_ARE_EQUAL(0, Testables.OffsetAndSizes[0].offset);
  VERIFY_ARE_EQUAL(4*32, Testables.OffsetAndSizes[0].size);

  ValidateAllocaWrite(Testables.AllocaWrites, 0, "before");
  ValidateAllocaWrite(Testables.AllocaWrites, 1, "one");
  ValidateAllocaWrite(Testables.AllocaWrites, 2, "two");
  ValidateAllocaWrite(Testables.AllocaWrites, 3, "after");
}

TEST_F(PixTest, PixStructAnnotation_1DArray) {
  if (m_ver.SkipDxilVersion(1, 5)) return;

    const char* hlsl = R"(
struct smallPayload
{
    uint32_t Array[2];
};


[numthreads(1, 1, 1)]
void main()
{
    smallPayload p;
    p.Array[0] = 250;
    p.Array[1] = 251;
    DispatchMesh(1, 1, 1, p);
}
)";

    auto Testables = TestStructAnnotationCase(hlsl);
    VERIFY_ARE_EQUAL(1, Testables.OffsetAndSizes.size());
    VERIFY_ARE_EQUAL(2, Testables.OffsetAndSizes[0].countOfMembers);
    VERIFY_ARE_EQUAL(0, Testables.OffsetAndSizes[0].offset);
    VERIFY_ARE_EQUAL(2 * 32, Testables.OffsetAndSizes[0].size);
}

TEST_F(PixTest, PixStructAnnotation_2DArray) {
  if (m_ver.SkipDxilVersion(1, 5)) return;

  const char *hlsl = R"(
struct smallPayload
{
    uint32_t TwoDArray[2][3];
};


[numthreads(1, 1, 1)]
void main()
{
    smallPayload p;
    p.TwoDArray[0][0] = 250;
    p.TwoDArray[0][1] = 251;
    p.TwoDArray[0][2] = 252;
    p.TwoDArray[1][0] = 253;
    p.TwoDArray[1][1] = 254;
    p.TwoDArray[1][2] = 255;
    DispatchMesh(1, 1, 1, p);
}
)";

  auto Testables = TestStructAnnotationCase(hlsl);
  VERIFY_ARE_EQUAL(1, Testables.OffsetAndSizes.size());
  VERIFY_ARE_EQUAL(6, Testables.OffsetAndSizes[0].countOfMembers);
  VERIFY_ARE_EQUAL(0, Testables.OffsetAndSizes[0].offset);
  VERIFY_ARE_EQUAL(2 * 3 * 32, Testables.OffsetAndSizes[0].size);
}

TEST_F(PixTest, PixStructAnnotation_EmbeddedArray) {
  if (m_ver.SkipDxilVersion(1, 5)) return;

  const char *hlsl = R"(

struct Contained
{
  uint32_t array[3];
};

struct smallPayload
{
  uint32_t before;
  Contained contained;
  uint32_t after;
};


[numthreads(1, 1, 1)]
void main()
{
    smallPayload p;
    p.before = 0xb4;
    p.contained.array[0] = 0;
    p.contained.array[1] = 1;
    p.contained.array[2] = 2;
    p.after = 3;
    DispatchMesh(1, 1, 1, p);
}
)";

  auto Testables = TestStructAnnotationCase(hlsl);

  VERIFY_ARE_EQUAL(1, Testables.OffsetAndSizes.size());
  VERIFY_ARE_EQUAL(5, Testables.OffsetAndSizes[0].countOfMembers);
  VERIFY_ARE_EQUAL(0, Testables.OffsetAndSizes[0].offset);
  VERIFY_ARE_EQUAL(5 * 32, Testables.OffsetAndSizes[0].size);

  ValidateAllocaWrite(Testables.AllocaWrites, 0, "before");
  ValidateAllocaWrite(Testables.AllocaWrites, 1, "array");
  ValidateAllocaWrite(Testables.AllocaWrites, 2, "array");
  ValidateAllocaWrite(Testables.AllocaWrites, 3, "array");
  ValidateAllocaWrite(Testables.AllocaWrites, 4, "after");
}

TEST_F(PixTest, PixStructAnnotation_FloatN) {
  if (m_ver.SkipDxilVersion(1, 5)) return;

  const char *hlsl = R"(
struct smallPayload
{
    float2 f2;
};


[numthreads(1, 1, 1)]
void main()
{
    smallPayload p;
    p.f2 = float2(1,2);
    DispatchMesh(1, 1, 1, p);
}
)";

  auto Testables = TestStructAnnotationCase(hlsl);

  // Can't test this until dbg.declare instructions are emitted when structs contain pointers-to-pointers
  // VERIFY_ARE_EQUAL(1, Testables.OffsetAndSizes.size());
  // VERIFY_ARE_EQUAL(2, Testables.OffsetAndSizes[0].countOfMembers);
  // VERIFY_ARE_EQUAL(0, Testables.OffsetAndSizes[0].offset);
  // VERIFY_ARE_EQUAL(32 + 32, Testables.OffsetAndSizes[0].size);

  VERIFY_ARE_EQUAL(2, Testables.AllocaWrites.size());
  ValidateAllocaWrite(Testables.AllocaWrites, 0, "member0"); // "memberN" until dbg.declare works
  ValidateAllocaWrite(Testables.AllocaWrites, 1, "member1"); // "memberN" until dbg.declare works
}


TEST_F(PixTest, PixStructAnnotation_SequentialFloatN) {
  if (m_ver.SkipDxilVersion(1, 5)) return;

  const char *hlsl = R"(
struct smallPayload
{
    float3 color;
    float3 dir;
};


[numthreads(1, 1, 1)]
void main()
{
    smallPayload p;
    p.color = float3(1,2,3);
    p.dir = float3(4,5,6);

    DispatchMesh(1, 1, 1, p);
}
)";

  auto Testables = TestStructAnnotationCase(hlsl);

  // Can't test this until dbg.declare instructions are emitted when structs contain pointers-to-pointers
  // VERIFY_ARE_EQUAL(1, Testables.OffsetAndSizes.size());
  // VERIFY_ARE_EQUAL(2, Testables.OffsetAndSizes[0].countOfMembers);
  // VERIFY_ARE_EQUAL(0, Testables.OffsetAndSizes[0].offset);
  // VERIFY_ARE_EQUAL(32 + 32, Testables.OffsetAndSizes[0].size);

  VERIFY_ARE_EQUAL(6, Testables.AllocaWrites.size());
  ValidateAllocaWrite(Testables.AllocaWrites, 0, "member0"); // "memberN" until dbg.declare works
  ValidateAllocaWrite(Testables.AllocaWrites, 1, "member1"); // "memberN" until dbg.declare works
  ValidateAllocaWrite(Testables.AllocaWrites, 2, "member2"); // "memberN" until dbg.declare works
  ValidateAllocaWrite(Testables.AllocaWrites, 3, "member0"); // "memberN" until dbg.declare works
  ValidateAllocaWrite(Testables.AllocaWrites, 4, "member1"); // "memberN" until dbg.declare works
  ValidateAllocaWrite(Testables.AllocaWrites, 5, "member2"); // "memberN" until dbg.declare works
}

TEST_F(PixTest, PixStructAnnotation_EmbeddedFloatN) {
  if (m_ver.SkipDxilVersion(1, 5)) return;

  const char *hlsl = R"(

struct Embedded
{
    float2 f2;
};

struct smallPayload
{
  uint32_t i32;
  Embedded e;
};


[numthreads(1, 1, 1)]
void main()
{
    smallPayload p;
    p.i32 = 32;
    p.e.f2 = float2(1,2);
    DispatchMesh(1, 1, 1, p);
}
)";

  auto Testables = TestStructAnnotationCase(hlsl);

  // Can't test this until dbg.declare instructions are emitted when structs
  // contain pointers-to-pointers
  //VERIFY_ARE_EQUAL(1, Testables.OffsetAndSizes.size());
  //VERIFY_ARE_EQUAL(2, Testables.OffsetAndSizes[0].countOfMembers);
  //VERIFY_ARE_EQUAL(0, Testables.OffsetAndSizes[0].offset);
  //VERIFY_ARE_EQUAL(32 + 32, Testables.OffsetAndSizes[0].size);

  VERIFY_ARE_EQUAL(3, Testables.AllocaWrites.size());
  ValidateAllocaWrite(Testables.AllocaWrites, 0, ""); 
  ValidateAllocaWrite(Testables.AllocaWrites, 1, "member0");
  ValidateAllocaWrite(Testables.AllocaWrites, 2, "member1");
}

TEST_F(PixTest, PixStructAnnotation_Matrix) {
  if (m_ver.SkipDxilVersion(1, 5)) return;

  const char *hlsl = R"(
struct smallPayload
{
  float4x4 mat;
};


[numthreads(1, 1, 1)]
void main()
{
  smallPayload p;
  p.mat = float4x4( 1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15, 16);
  DispatchMesh(1, 1, 1, p);
}
)";

  auto Testables = TestStructAnnotationCase(hlsl);
  // Can't test member iterator until dbg.declare instructions are emitted when structs
  // contain pointers-to-pointers
  VERIFY_ARE_EQUAL(16, Testables.AllocaWrites.size());
  for (int i = 0; i < 16; ++i)
  {
    ValidateAllocaWrite(Testables.AllocaWrites, i, "");
  }

}

TEST_F(PixTest, PixStructAnnotation_MemberFunction) {
  const char *hlsl = R"(

RWStructuredBuffer<float> floatRWUAV: register(u0);

struct smallPayload
{
    int i;
};

float2 signNotZero(float2 v)
{
 return (v > 0.0f ? float(1).xx : float(-1).xx);
}

float2 unpackUnorm2(uint packed)
{
 return (1.0 / 65535.0) * float2((packed >> 16) & 0xffff, packed & 0xffff);
}

float3 unpackOctahedralSnorm(float2 e)
{
 float3 v = float3(e.xy, 1.0f - abs(e.x) - abs(e.y));
 if (v.z < 0.0f) v.xy = (1.0f - abs(v.yx)) * signNotZero(v.xy);
 return normalize(v);
}

float3 unpackOctahedralUnorm(float2 e)
{
 return unpackOctahedralSnorm(e * 2.0f - 1.0f);
}

float2 unpackHalf2(uint packed)
{
 return float2(f16tof32(packed >> 16), f16tof32(packed & 0xffff));
}

struct Gbuffer
{
	float3 worldNormal;
	float3 objectNormal; //offset:12
	
	float linearZ; //24
	float prevLinearZ; //28
	
	
	float fwidthLinearZ; //32
	float fwidthObjectNormal; //36
	
	
	uint materialType; //40
	uint2 materialParams0; //44
	uint4 materialParams1; //52  <--------- this is the variable that's being covered twice (52*8 = 416 416)
	
	uint instanceId;  //68  <------- and there's one dword left over, as expected
	
	
	void load(int2 pixelPos, Texture2DArray<uint4> gbTex)
	{
	uint4 data0 = gbTex.Load(int4(pixelPos, 0, 0));
	uint4 data1 = gbTex.Load(int4(pixelPos, 1, 0));
	uint4 data2 = gbTex.Load(int4(pixelPos, 2, 0));
	
	
	worldNormal = unpackOctahedralUnorm(unpackUnorm2(data0.x));
	linearZ = f16tof32((data0.y >> 8) & 0xffff);
	materialType = (data0.y & 0xff);
	materialParams0 = data0.zw;
	
	
	materialParams1 = data1.xyzw;
	
	
	instanceId = data2.x;
	prevLinearZ = asfloat(data2.y);
	objectNormal = unpackOctahedralUnorm(unpackUnorm2(data2.z));
	float2 fwidth = unpackHalf2(data2.w);
	fwidthLinearZ = fwidth.x;
	fwidthObjectNormal = fwidth.y;
	}
};

Gbuffer loadGbuffer(int2 pixelPos, Texture2DArray<uint4> gbTex)
{
	Gbuffer output;
	output.load(pixelPos, gbTex);
	return output;
}

Texture2DArray<uint4> g_gbuffer : register(t0, space0);

[numthreads(1, 1, 1)]
void main()
{	
	const Gbuffer gbuffer = loadGbuffer(int2(0,0), g_gbuffer);
    smallPayload p;
    p.i = gbuffer.materialParams1.x + gbuffer.materialParams1.y + gbuffer.materialParams1.z + gbuffer.materialParams1.w;
    DispatchMesh(1, 1, 1, p);
}


)";

  auto Testables = TestStructAnnotationCase(hlsl);
  // Can't test member iterator until dbg.declare instructions are emitted when structs
  // contain pointers-to-pointers
  
  // Can't validate # of writes: rel and dbg are different
  //VERIFY_ARE_EQUAL(43, Testables.AllocaWrites.size());

  // Can't test individual writes until struct member names are returned:
  //for (int i = 0; i < 51; ++i)
  //{
  //  ValidateAllocaWrite(Testables.AllocaWrites, i, "");
  //}
}

TEST_F(PixTest, PixStructAnnotation_BigMess) {
  if (m_ver.SkipDxilVersion(1, 5)) return;

  const char *hlsl = R"(

struct BigStruct
{
    uint64_t bigInt;
    double bigDouble;
};

struct EmbeddedStruct
{
    uint32_t OneInt;
    uint32_t TwoDArray[2][2];
};

struct smallPayload
{
    uint dummy;
    uint vertexCount;
    uint primitiveCount;
    EmbeddedStruct embeddedStruct;
#ifdef PAYLOAD_MATRICES
    float4x4 mat;
#endif
    uint64_t bigOne;
    half littleOne;
    BigStruct bigStruct[2];
    uint lastCheck;
};


[numthreads(1, 1, 1)]
void main()
{
    smallPayload p;
    // Adding enough instructions to make the shader interesting to debug:
    p.dummy = 42;
    p.vertexCount = 3;
    p.primitiveCount = 1;
    p.embeddedStruct.OneInt = 123;
    p.embeddedStruct.TwoDArray[0][0] = 252;
    p.embeddedStruct.TwoDArray[0][1] = 253;
    p.embeddedStruct.TwoDArray[1][0] = 254;
    p.embeddedStruct.TwoDArray[1][1] = 255;
#ifdef PAYLOAD_MATRICES
    p.mat = float4x4( 1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15, 16);
#endif
    p.bigOne = 123456789;
    p.littleOne = 1.0;
    p.bigStruct[0].bigInt = 10;
    p.bigStruct[0].bigDouble = 2.0;
    p.bigStruct[1].bigInt = 20;
    p.bigStruct[1].bigDouble = 4.0;
    p.lastCheck = 27;
    DispatchMesh(1, 1, 1, p);
}
)";

  auto Testables = TestStructAnnotationCase(hlsl);
  VERIFY_ARE_EQUAL(1, Testables.OffsetAndSizes.size());
  VERIFY_ARE_EQUAL(15, Testables.OffsetAndSizes[0].countOfMembers);
  VERIFY_ARE_EQUAL(0, Testables.OffsetAndSizes[0].offset);
  constexpr uint32_t BigStructBitSize = 64 * 2;
  constexpr uint32_t EmbeddedStructBitSize = 32 * 5;
  VERIFY_ARE_EQUAL(3 * 32 + EmbeddedStructBitSize + 64 + 16 +16/*alignment for next field*/ + BigStructBitSize*2 + 32 + 32/*align to max align*/, Testables.OffsetAndSizes[0].size);
}


#endif
