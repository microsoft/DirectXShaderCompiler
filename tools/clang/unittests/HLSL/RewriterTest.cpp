///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// RewriterTest.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// The following HLSL tests contain static_asserts and are not useful for    //
// the HLSL rewriter: more-operators.hlsl, object-operators.hlsl,            //
// scalar-operators-assign.hlsl, scalar-operators.hlsl, string.hlsl.         //
// They have been omitted.                                                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef UNICODE
#define UNICODE
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <memory>
#include <vector>
#include <string>
#include <cassert>
#include <sstream>
#include <algorithm>
#include <windows.h>
#include <unknwn.h>
#include "dxc/dxcapi.h"
#include <atlbase.h>
#include <atlfile.h>

#include "WexTestClass.h"
#include "dxc/Test/HLSLTestData.h"
#include "dxc/Test/HlslTestUtils.h"
#include "dxc/Test/DxcTestUtils.h"

#include "dxc/Support/Global.h"
#include "dxc/dxctools.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/dxcapi.internal.h"

using namespace std;
using namespace hlsl_test;

class RewriterTest {
public:
  BEGIN_TEST_CLASS(RewriterTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_METHOD(RunArrayLength);
  TEST_METHOD(RunAttributes);
  TEST_METHOD(RunAnonymousStruct);
  TEST_METHOD(RunCppErrors);
  TEST_METHOD(RunForceExtern);
  TEST_METHOD(RunIndexingOperator);
  TEST_METHOD(RunIntrinsicExamples);
  TEST_METHOD(RunMatrixAssignments);
  TEST_METHOD(RunMatrixPackOrientation);
  TEST_METHOD(RunMatrixSyntax);
  TEST_METHOD(RunPackReg);
  TEST_METHOD(RunScalarAssignments);
  TEST_METHOD(RunShared);
  TEST_METHOD(RunStructAssignments);
  TEST_METHOD(RunTemplateChecks);
  TEST_METHOD(RunTypemodsSyntax);
  TEST_METHOD(RunVarmodsSyntax);
  TEST_METHOD(RunVectorAssignments);
  TEST_METHOD(RunVectorSyntaxMix);
  TEST_METHOD(RunVectorSyntax);
  TEST_METHOD(RunIncludes);
  TEST_METHOD(RunStructMethods);
  TEST_METHOD(RunPredefines);
  TEST_METHOD(RunUTF16OneByte);
  TEST_METHOD(RunUTF16TwoByte);
  TEST_METHOD(RunUTF16ThreeByteBadChar);
  TEST_METHOD(RunUTF16ThreeByte);
  TEST_METHOD(RunNonUnicode);
  TEST_METHOD(RunEffect);
  TEST_METHOD(RunSemanticDefines);
  TEST_METHOD(RunNoFunctionBody);
  TEST_METHOD(RunNoFunctionBodyInclude);
  TEST_METHOD(RunNoStatic);
  TEST_METHOD(RunKeepUserMacro);
  TEST_METHOD(RunExtractUniforms);
  TEST_METHOD(RunRewriterFails)

  dxc::DxcDllSupport m_dllSupport;
  CComPtr<IDxcIncludeHandler> m_pIncludeHandler;

  struct VerifyResult {
    std::string warnings; // warnings from first compilation
    std::string rewrite;  // output of rewrite
    
    bool HasSubstringInRewrite(const char* val) {
      return std::string::npos != rewrite.find(val);
    }
    bool HasSubstringInWarnings(const char* val) {
      return std::string::npos != warnings.find(val);
    }
  };

  void CreateBlobPinned(_In_bytecount_(size) LPCVOID data, SIZE_T size,
                        UINT32 codePage, _In_ IDxcBlobEncoding **ppBlob) {
    CComPtr<IDxcLibrary> library;
    IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
    IFT(library->CreateBlobWithEncodingFromPinned(data, size, codePage,
                                                  ppBlob));
  }

  VerifyResult CheckVerifies(LPCWSTR path, LPCWSTR goldPath) {   
    CComPtr<IDxcRewriter> pRewriter;
    VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
    return CheckVerifies(pRewriter, path, goldPath);
  }

  VerifyResult CheckVerifies(IDxcRewriter *pRewriter, LPCWSTR path, LPCWSTR goldPath) {
    CComPtr<IDxcOperationResult> pRewriteResult;
    RewriteCompareGold(path, goldPath, &pRewriteResult, pRewriter);

    VerifyResult toReturn;

    CComPtr<IDxcBlob> pResultBlob;
    VERIFY_SUCCEEDED(pRewriteResult->GetResult(&pResultBlob));
    toReturn.rewrite = BlobToUtf8(pResultBlob);

    CComPtr<IDxcBlobEncoding> pErrorsBlob;
    VERIFY_SUCCEEDED(pRewriteResult->GetErrorBuffer(&pErrorsBlob));
    toReturn.warnings = BlobToUtf8(pErrorsBlob);

    return toReturn;
  }
  
  HRESULT CreateRewriter(IDxcRewriter** pRewriter) {
    if (!m_dllSupport.IsEnabled()) {
      VERIFY_SUCCEEDED(m_dllSupport.Initialize());

      CComPtr<IDxcLibrary> library;
      VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
      VERIFY_SUCCEEDED(library->CreateIncludeHandler(&m_pIncludeHandler));
    }
    return m_dllSupport.CreateInstance(CLSID_DxcRewriter, pRewriter);
  }

  HRESULT CreateRewriterWithSemanticDefines(IDxcRewriter** pRewriter, std::vector<LPCWSTR> defines) {
    VERIFY_SUCCEEDED(CreateRewriter(pRewriter));
    CComPtr<IDxcLangExtensions> pLangExtensions;
    VERIFY_SUCCEEDED((*pRewriter)->QueryInterface(&pLangExtensions));
    for (LPCWSTR define : defines)
      VERIFY_SUCCEEDED(pLangExtensions->RegisterSemanticDefine(define));

    return S_OK;
  }

  VerifyResult CheckVerifiesHLSL(LPCWSTR name, LPCWSTR goldName) {
    return CheckVerifies(GetPathToHlslDataFile(name).c_str(),
                         GetPathToHlslDataFile(goldName).c_str());
  }

  struct FileWithBlob {
    CAtlFile file;
    CAtlFileMapping<char> mapping;
    CComPtr<IDxcBlobEncoding> BlobEncoding;

    FileWithBlob(dxc::DxcDllSupport &support, LPCWSTR path) {
      IFT(file.Create(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING));
      IFT(mapping.MapFile(file));
      CComPtr<IDxcLibrary> library;
      IFT(support.CreateInstance(CLSID_DxcLibrary, &library));
      IFT(library->CreateBlobWithEncodingFromPinned(mapping.GetData(),
                                                    mapping.GetMappingSize(),
                                                    CP_UTF8, &BlobEncoding));
    }
  };

  bool CompareGold(std::string &firstPass, LPCWSTR goldPath) {
    HANDLE goldHandle = CreateFileW(goldPath, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    VERIFY_ARE_NOT_EQUAL(goldHandle, INVALID_HANDLE_VALUE);
    CHandle checkedGoldHandle(goldHandle);

    DWORD gFileSize = GetFileSize(goldHandle, NULL);
    CComHeapPtr<char> gReadBuff;
    VERIFY_IS_TRUE(gReadBuff.AllocateBytes(gFileSize));
    DWORD gnumActualRead;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadFile(checkedGoldHandle, gReadBuff.m_pData,
                                         gFileSize, &gnumActualRead, NULL));
    std::string gold = std::string((LPSTR)gReadBuff, gnumActualRead);
    gold.erase(std::remove(gold.begin(), gold.end(), '\r'), gold.end());

      // Kept because useful for debugging
      //int atChar = 0;
      //int numDiffChar = 0;

      //while (atChar < result.size){
      //  char rewriteChar = (firstPass.data())[atChar];
      //  char goldChar = (gold.data())[atChar];
      //  
      //  if (rewriteChar != goldChar){
      //    numDiffChar++;
      //  }
      //  atChar++;
      //}
    return firstPass.compare(gold) == 0;
  }

  // Note: Previous versions of this file included a RewriteCompareRewrite method here that rewrote twice and compared  
  // to check for stable output.  It has now been replaced by a new test that checks against a gold baseline.

  void RewriteCompareGold(LPCWSTR path, LPCWSTR goldPath,
                          _COM_Outptr_ IDxcOperationResult **ppResult,
                          _In_ IDxcRewriter *rewriter) {
    // Get the source text from a file
    FileWithBlob source(m_dllSupport, path);

    const int myDefinesCount = 3;
    DxcDefine myDefines[myDefinesCount] = {
        {L"myDefine", L"2"}, {L"myDefine3", L"1994"}, {L"myDefine4", nullptr}};

    LPCWSTR args[] = {L"-HV", L"2016"};

    CComPtr<IDxcRewriter2> rewriter2;
    VERIFY_SUCCEEDED(rewriter->QueryInterface(&rewriter2));
    // Run rewrite unchanged on the source code
    VERIFY_SUCCEEDED(rewriter2->RewriteWithOptions( source.BlobEncoding, path,
                                                    args, _countof(args),
                                                    myDefines, myDefinesCount,
                                                    nullptr, ppResult));

    // check for compilation errors
    HRESULT hrStatus;
    VERIFY_SUCCEEDED((*ppResult)->GetStatus(&hrStatus));

    if (!(SUCCEEDED(hrStatus))) {
        ::WEX::Logging::Log::Error(L"\nCompilation failed.\n");
        CComPtr<IDxcBlobEncoding> pErrorBuffer;
        IFT((*ppResult)->GetErrorBuffer(&pErrorBuffer));
        std::wstring errorStr = BlobToUtf16(pErrorBuffer);
        ::WEX::Logging::Log::Error(errorStr.data());
        VERIFY_SUCCEEDED(hrStatus);
        return;
    }

    CComPtr<IDxcBlob> pRewriteResult;
    IFT((*ppResult)->GetResult(&pRewriteResult));
    std::string firstPass = BlobToUtf8(pRewriteResult);

    if (CompareGold(firstPass, goldPath)) {
      return;
    }

    // Log things out before failing.
    std::wstring TestFileName(path);
    int index1 = TestFileName.find_last_of(L"\\");
    int index2 = TestFileName.find_last_of(L".");
    TestFileName = TestFileName.substr(index1+1, index2 - (index1+1));
        
    wchar_t TempPath[MAX_PATH];
    DWORD length = GetTempPathW(MAX_PATH, TempPath);
    VERIFY_WIN32_BOOL_SUCCEEDED(length != 0);
    
    std::wstring PrintName(TempPath);
    PrintName += TestFileName;
    PrintName += L"_rewrite_test_pass.txt";
        
    CHandle checkedWHandle(CreateNewFileForReadWrite(PrintName.data()));
    LPDWORD wnumWrite = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteFile(checkedWHandle, firstPass.data(),
                                          firstPass.size(), wnumWrite, NULL));
        
    std::wstringstream ss;
    ss << L"\nMismatch occurred between rewriter output and expected "
          L"output. To see the differences, run:\n"
          L"diff " << goldPath << L" " << PrintName << L"\n";
    ::WEX::Logging::Log::Error(ss.str().c_str());
  }

  bool RewriteCompareGoldInclude(LPCWSTR path, LPCWSTR goldPath,
                                 unsigned rewriteOption) {
    CComPtr<IDxcRewriter> pRewriter;
    VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
    CComPtr<IDxcOperationResult> pRewriteResult;

    std::wstring fileName = GetPathToHlslDataFile(path);

    // Get the source text from a file
    FileWithBlob source(m_dllSupport, fileName.c_str());

    const int myDefinesCount = 3;
    DxcDefine myDefines[myDefinesCount] = {
        {L"myDefine", L"2"}, {L"myDefine3", L"1994"}, {L"myDefine4", nullptr}};

    // Run rewrite no function body on the source code
    VERIFY_SUCCEEDED(pRewriter->RewriteUnchangedWithInclude(
        source.BlobEncoding, fileName.c_str(), myDefines, myDefinesCount,
        m_pIncludeHandler, rewriteOption, &pRewriteResult));

    CComPtr<IDxcBlob> result;
    VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));

    std::string rewriteText = BlobToUtf8(result);

    return CompareGold(rewriteText, GetPathToHlslDataFile(goldPath).c_str());
  }
};

TEST_F(RewriterTest, RunArrayLength) {
  CheckVerifiesHLSL(L"rewriter\\array-length-rw.hlsl", L"rewriter\\correct_rewrites\\array-length-rw_gold.hlsl");
}

TEST_F(RewriterTest, RunAttributes) {
    CheckVerifiesHLSL(L"rewriter\\attributes_noerr.hlsl", L"rewriter\\correct_rewrites\\attributes_gold.hlsl");
}

TEST_F(RewriterTest, RunAnonymousStruct) {
    CheckVerifiesHLSL(L"rewriter\\anonymous_struct.hlsl", L"rewriter\\correct_rewrites\\anonymous_struct_gold.hlsl");
}

TEST_F(RewriterTest, RunCppErrors) {
    CheckVerifiesHLSL(L"rewriter\\cpp-errors_noerr.hlsl", L"rewriter\\correct_rewrites\\cpp-errors_gold.hlsl");
}

TEST_F(RewriterTest, RunIndexingOperator) {
    CheckVerifiesHLSL(L"rewriter\\indexing-operator_noerr.hlsl", L"rewriter\\correct_rewrites\\indexing-operator_gold.hlsl");
}

TEST_F(RewriterTest, RunIntrinsicExamples) {
    CheckVerifiesHLSL(L"rewriter\\intrinsic-examples_noerr.hlsl", L"rewriter\\correct_rewrites\\intrinsic-examples_gold.hlsl");
}

TEST_F(RewriterTest, RunMatrixAssignments) {
    CheckVerifiesHLSL(L"rewriter\\matrix-assignments_noerr.hlsl", L"rewriter\\correct_rewrites\\matrix-assignments_gold.hlsl");
}

TEST_F(RewriterTest, RunMatrixPackOrientation) {
  CheckVerifiesHLSL(L"rewriter\\matrix-pack-orientation.hlsl", L"rewriter\\correct_rewrites\\matrix-pack-orientation_gold.hlsl");
}

TEST_F(RewriterTest, RunMatrixSyntax) {
    CheckVerifiesHLSL(L"rewriter\\matrix-syntax_noerr.hlsl", L"rewriter\\correct_rewrites\\matrix-syntax_gold.hlsl");
}

TEST_F(RewriterTest, RunPackReg) {
    CheckVerifiesHLSL(L"rewriter\\packreg_noerr.hlsl", L"rewriter\\correct_rewrites\\packreg_gold.hlsl");
}

TEST_F(RewriterTest, RunScalarAssignments) {
    CheckVerifiesHLSL(L"rewriter\\scalar-assignments_noerr.hlsl", L"rewriter\\correct_rewrites\\scalar-assignments_gold.hlsl");
}

TEST_F(RewriterTest, RunShared) {
    CheckVerifiesHLSL(L"rewriter\\shared.hlsl", L"rewriter\\correct_rewrites\\shared.hlsl");
}

TEST_F(RewriterTest, RunStructAssignments) {
    CheckVerifiesHLSL(L"rewriter\\struct-assignments_noerr.hlsl", L"rewriter\\correct_rewrites\\struct-assignments_gold.hlsl");
}

TEST_F(RewriterTest, RunTemplateChecks) {
    CheckVerifiesHLSL(L"rewriter\\template-checks_noerr.hlsl", L"rewriter\\correct_rewrites\\template-checks_gold.hlsl");
}

TEST_F(RewriterTest, RunTypemodsSyntax) {
    CheckVerifiesHLSL(L"rewriter\\typemods-syntax_noerr.hlsl", L"rewriter\\correct_rewrites\\typemods-syntax_gold.hlsl");
}

TEST_F(RewriterTest, RunVarmodsSyntax) {
    CheckVerifiesHLSL(L"rewriter\\varmods-syntax_noerr.hlsl", L"rewriter\\correct_rewrites\\varmods-syntax_gold.hlsl");
}

TEST_F(RewriterTest, RunVectorAssignments) {
    CheckVerifiesHLSL(L"rewriter\\vector-assignments_noerr.hlsl", L"rewriter\\correct_rewrites\\vector-assignments_gold.hlsl");
}

TEST_F(RewriterTest, RunVectorSyntaxMix) {
    CheckVerifiesHLSL(L"rewriter\\vector-syntax-mix_noerr.hlsl", L"rewriter\\correct_rewrites\\vector-syntax-mix_gold.hlsl");
}

TEST_F(RewriterTest, RunVectorSyntax) {
    CheckVerifiesHLSL(L"rewriter\\vector-syntax_noerr.hlsl", L"rewriter\\correct_rewrites\\vector-syntax_gold.hlsl");
}

TEST_F(RewriterTest, RunIncludes) {
  VERIFY_IS_TRUE(RewriteCompareGoldInclude(
      L"rewriter\\includes.hlsl",
      L"rewriter\\correct_rewrites\\includes_gold.hlsl",
      RewriterOptionMask::Default));
}

TEST_F(RewriterTest, RunNoFunctionBodyInclude) {
  VERIFY_IS_TRUE(RewriteCompareGoldInclude(
      L"rewriter\\includes.hlsl",
      L"rewriter\\correct_rewrites\\includes_gold_nobody.hlsl",
      RewriterOptionMask::SkipFunctionBody));
}

TEST_F(RewriterTest, RunStructMethods) {
  CheckVerifiesHLSL(L"rewriter\\struct-methods.hlsl", L"rewriter\\correct_rewrites\\struct-methods_gold.hlsl");
}

TEST_F(RewriterTest, RunPredefines) {
  CheckVerifiesHLSL(L"rewriter\\predefines.hlsl", L"rewriter\\correct_rewrites\\predefines_gold.hlsl");
}

static const UINT32 CP_UTF16 = 1200;

TEST_F(RewriterTest, RunUTF16OneByte) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  WCHAR utf16text[] = { L"\x0069\x006e\x0074\x0020\x0069\x003b" }; // "int i;"

  CComPtr<IDxcBlobEncoding> source;
  CreateBlobPinned(utf16text, sizeof(utf16text), CP_UTF16, &source);

  VERIFY_SUCCEEDED(pRewriter->RewriteUnchanged(source, 0, 0, &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));

  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(), "// Rewrite unchanged result:\n\x63\x6f\x6e\x73\x74\x20\x69\x6e\x74\x20\x69\x3b\n") == 0); // const added by default
}

TEST_F(RewriterTest, RunUTF16TwoByte) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  WCHAR utf16text[] = { L"\x0069\x006e\x0074\x0020\x00ed\x00f1\x0167\x003b" }; // "int (i w/ acute)(n w/tilde)(t w/ 2 strokes);"

  CComPtr<IDxcBlobEncoding> source;
  CreateBlobPinned(utf16text, sizeof(utf16text), CP_UTF16, &source);

  VERIFY_SUCCEEDED(pRewriter->RewriteUnchanged(source, 0, 0, &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));

  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(), "// Rewrite unchanged result:\n\x63\x6f\x6e\x73\x74\x20\x69\x6e\x74\x20\xc3\xad\xc3\xb1\xc5\xa7\x3b\n") == 0); // const added by default
}

TEST_F(RewriterTest, RunUTF16ThreeByteBadChar) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  WCHAR utf16text[] = { L"\x0069\x006e\x0074\x0020\x0041\x2655\x265a\x003b" }; // "int A(white queen)(black king);"

  CComPtr<IDxcBlobEncoding> source;
  CreateBlobPinned(utf16text, sizeof(utf16text), CP_UTF16, &source);

  VERIFY_SUCCEEDED(pRewriter->RewriteUnchanged(source, 0, 0, &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));

  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(), "// Rewrite unchanged result:\n\x63\x6f\x6e\x73\x74\x20\x69\x6e\x74\x20\x41\x3b\n") == 0); //"const int A;" -> should remove the weird characters
}

TEST_F(RewriterTest, RunUTF16ThreeByte) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  WCHAR utf16text[] = { L"\x0069\x006e\x0074\x0020\x1e8b\x003b" }; // "int (x with dot above);"

  CComPtr<IDxcBlobEncoding> source;
  CreateBlobPinned(utf16text, sizeof(utf16text), CP_UTF16, &source);

  VERIFY_SUCCEEDED(pRewriter->RewriteUnchanged(source, 0, 0, &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));

  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(), "// Rewrite unchanged result:\n\x63\x6f\x6e\x73\x74\x20\x69\x6e\x74\x20\xe1\xba\x8b\x3b\n") == 0); // const added by default
}

TEST_F(RewriterTest, RunNonUnicode) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  char greektext[] = { "\x69\x6e\x74\x20\xe1\xe2\xe3\x3b" }; // "int (small alpha)(small beta)(small kappa);"

  CComPtr<IDxcBlobEncoding> source;
  CreateBlobPinned(greektext, sizeof(greektext), 1253, &source); // 1253 == ANSI Greek

  VERIFY_SUCCEEDED(pRewriter->RewriteUnchanged(source, 0, 0, &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));

  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(), "// Rewrite unchanged result:\n\x63\x6f\x6e\x73\x74\x20\x69\x6e\x74\x20\xce\xb1\xce\xb2\xce\xb3\x3b\n") == 0); // const added by default
}

TEST_F(RewriterTest, RunEffect) {
  CheckVerifiesHLSL(L"rewriter\\effects-syntax_noerr.hlsl", L"rewriter\\correct_rewrites\\effects-syntax_gold.hlsl");
}

TEST_F(RewriterTest, RunSemanticDefines) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriterWithSemanticDefines(&pRewriter, {L"SD_*"}));
  CheckVerifies(pRewriter, hlsl_test::GetPathToHlslDataFile(L"rewriter\\semantic-defines.hlsl").c_str(),
                           hlsl_test::GetPathToHlslDataFile(L"rewriter\\correct_rewrites\\semantic-defines_gold.hlsl").c_str());
}

TEST_F(RewriterTest, RunNoFunctionBody) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  // Get the source text from a file
  FileWithBlob source(
      m_dllSupport,
      GetPathToHlslDataFile(L"rewriter\\vector-assignments_noerr.hlsl")
          .c_str());

  const int myDefinesCount = 3;
  DxcDefine myDefines[myDefinesCount] = {
      {L"myDefine", L"2"}, {L"myDefine3", L"1994"}, {L"myDefine4", nullptr}};

  // Run rewrite no function body on the source code
  VERIFY_SUCCEEDED(pRewriter->RewriteUnchangedWithInclude(
      source.BlobEncoding, L"vector-assignments_noerr.hlsl", myDefines,
      myDefinesCount, /*pIncludeHandler*/ nullptr, RewriterOptionMask::SkipFunctionBody,
      &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));
  // Function decl only.
  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(),
                        "// Rewrite unchanged result:\nfloat pick_one(float2 "
                        "f2);\nvoid main();\n") == 0);
}

TEST_F(RewriterTest, RunNoStatic) {
  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  // Get the source text from a file
  FileWithBlob source(
      m_dllSupport,
      GetPathToHlslDataFile(L"rewriter\\attributes_noerr.hlsl")
          .c_str());

  const int myDefinesCount = 3;
  DxcDefine myDefines[myDefinesCount] = {
      {L"myDefine", L"2"}, {L"myDefine3", L"1994"}, {L"myDefine4", nullptr}};

  // Run rewrite no function body on the source code
  VERIFY_SUCCEEDED(pRewriter->RewriteUnchangedWithInclude(
      source.BlobEncoding, L"attributes_noerr.hlsl", myDefines, myDefinesCount,
      /*pIncludeHandler*/ nullptr,
      RewriterOptionMask::SkipFunctionBody | RewriterOptionMask::SkipStatic,
      &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));
  std::string strResult = BlobToUtf8(result);
  // No static.
  VERIFY_IS_TRUE(strResult.find("static") == std::string::npos);
}

TEST_F(RewriterTest, RunForceExtern) {  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  // Get the source text from a file
  FileWithBlob source(
      m_dllSupport,
      GetPathToHlslDataFile(L"rewriter\\force_extern.hlsl")
          .c_str());

  const int myDefinesCount = 3;
  DxcDefine myDefines[myDefinesCount] = {
      {L"myDefine", L"2"}, {L"myDefine3", L"1994"}, {L"myDefine4", nullptr}};

  // Run rewrite no function body on the source code
  VERIFY_SUCCEEDED(pRewriter->RewriteUnchangedWithInclude(
      source.BlobEncoding, L"vector-assignments_noerr.hlsl", myDefines,
      myDefinesCount, /*pIncludeHandler*/ nullptr,
      RewriterOptionMask::SkipFunctionBody |
          RewriterOptionMask::GlobalExternByDefault,
      &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));
  // Function decl only.
  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(),
      "// Rewrite unchanged result:\n\
extern const float a;\n\
namespace b {\n\
  extern const float c;\n\
  namespace d {\n\
    extern const float e;\n\
  }\n\
}\n\
static int f;\n\
float4 main() : SV_Target;\n") == 0);
}

TEST_F(RewriterTest, RunKeepUserMacro) {  CComPtr<IDxcRewriter> pRewriter;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  CComPtr<IDxcOperationResult> pRewriteResult;

  // Get the source text from a file
  FileWithBlob source(
      m_dllSupport,
      GetPathToHlslDataFile(L"rewriter\\predefines2.hlsl")
          .c_str());

  const int myDefinesCount = 3;
  DxcDefine myDefines[myDefinesCount] = {
      {L"myDefine", L"2"}, {L"myDefine3", L"1994"}, {L"myDefine4", nullptr}};

  // Run rewrite no function body on the source code
  VERIFY_SUCCEEDED(pRewriter->RewriteUnchangedWithInclude(
      source.BlobEncoding, L"vector-assignments_noerr.hlsl", myDefines,
      myDefinesCount, /*pIncludeHandler*/ nullptr,
      RewriterOptionMask::KeepUserMacro,
      &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));
  // Function decl only.
  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(),
      "// Rewrite unchanged result:\n\
const float x = 1;\n\
float test(float a, float b) {\n\
  return ((a) + (b));\n\
}\n\
\n\n\n\
// Macros:\n\
#define X 1\n\
#define Y(A, B)  ( ( A ) + ( B ) )\n\
") == 0);
}

TEST_F(RewriterTest, RunExtractUniforms) {
  CComPtr<IDxcRewriter> pRewriter;
  CComPtr<IDxcRewriter2> pRewriter2;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  VERIFY_SUCCEEDED(pRewriter->QueryInterface(&pRewriter2));
  CComPtr<IDxcOperationResult> pRewriteResult;

  // Get the source text from a file
  FileWithBlob source(
      m_dllSupport,
      GetPathToHlslDataFile(L"rewriter\\rewrite-uniforms.hlsl")
          .c_str());

  LPCWSTR compileOptions[] = {L"-E", L"FloatFunc", L"-extract-entry-uniforms"};

  // Run rewrite on the source code to move uniform params to globals
  VERIFY_SUCCEEDED(pRewriter2->RewriteWithOptions(
    source.BlobEncoding, L"rewrite-uniforms.hlsl",
    compileOptions, _countof(compileOptions),
    nullptr, 0, nullptr, &pRewriteResult));

  CComPtr<IDxcBlob> result;
  VERIFY_SUCCEEDED(pRewriteResult->GetResult(&result));

  VERIFY_IS_TRUE(strcmp(BlobToUtf8(result).c_str(),
"// Rewrite unchanged result:\n\
[RootSignature(\"RootFlags(0),DescriptorTable(UAV(u0, numDescriptors = 1), CBV(b0, numDescriptors = 1))\")]\n\
[numthreads(4, 8, 16)]\n\
void IntFunc(uint3 id : SV_DispatchThreadID, uniform RWStructuredBuffer<int> buf, uniform uint ui) {\n\
  buf[id.x + id.y + id.z] = id.x + ui;\n\
}\n\
\n\
\n\
uniform RWStructuredBuffer<float> buf;\n\
cbuffer _Params {\n\
uniform uint ui;\n\
}\n\
[RootSignature(\"RootFlags(0),DescriptorTable(UAV(u0, numDescriptors = 1), CBV(b0, numDescriptors = 1))\")]\n\
[numthreads(4, 8, 16)]\n\
void FloatFunc(uint3 id : SV_DispatchThreadID) {\n\
  buf[id.x + id.y + id.z] = id.x;\n\
}\n\
\n\
") == 0);
}

TEST_F(RewriterTest, RunRewriterFails) {
  CComPtr<IDxcRewriter> pRewriter;
  CComPtr<IDxcRewriter2> pRewriter2;
  VERIFY_SUCCEEDED(CreateRewriter(&pRewriter));
  VERIFY_SUCCEEDED(pRewriter->QueryInterface(&pRewriter2));

  // Get the source text from a file
  std::wstring sourceName = GetPathToHlslDataFile(L"rewriter\\array-length-rw.hlsl");
  FileWithBlob source(m_dllSupport, sourceName.c_str());

  // Compilation should fail with these options
  CComPtr<IDxcOperationResult> pRewriteResult;
  LPCWSTR compileOptions[] = {L"-HV", L"2018"};
  
  // Run rewrite on the source code
  VERIFY_SUCCEEDED(pRewriter2->RewriteWithOptions(source.BlobEncoding, sourceName.c_str(), compileOptions, 2, 
                                                  nullptr, 0, nullptr, &pRewriteResult));

  // Verify it failed
  HRESULT hrStatus;
  VERIFY_SUCCEEDED(pRewriteResult->GetStatus(&hrStatus));
  VERIFY_FAILED(hrStatus);

  ::WEX::Logging::Log::Comment(L"\nCompilation failed as expected.\n");
  CComPtr<IDxcBlobEncoding> pErrorBuffer;
  IFT(pRewriteResult->GetErrorBuffer(&pErrorBuffer));
  std::wstring errorStr = BlobToUtf16(pErrorBuffer);
  
  ::WEX::Logging::Log::Comment(errorStr.data());

  VERIFY_IS_TRUE(errorStr.find(L"Length is only allowed for HLSL 2016 and lower.") >= 0);
}