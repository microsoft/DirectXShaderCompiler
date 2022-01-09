///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcapi.use.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides support for DXC API users.                                       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/WinFunctions.h"

// For MinGW, export specializations for our public COM interface. See WinAdapter.h for more info.
#ifdef __MINGW32__
MINGW_UUIDOF(IDxcBlob, "8BA5FB08-5195-40e2-AC58-0D989C3A0102")
MINGW_UUIDOF(IDxcBlobEncoding, "7241d424-2646-4191-97c0-98e96e42fc68")
MINGW_UUIDOF(IDxcBlobUtf16, "A3F84EAB-0FAA-497E-A39C-EE6ED60B2D84")
MINGW_UUIDOF(IDxcBlobUtf8, "3DA636C9-BA71-4024-A301-30CBF125305B")
MINGW_UUIDOF(IDxcIncludeHandler, "7f61fc7d-950d-467f-b3e3-3c02fb49187c")
MINGW_UUIDOF(IDxcCompilerArgs, "73EFFE2A-70DC-45F8-9690-EFF64C02429D")
MINGW_UUIDOF(IDxcLibrary, "e5204dc7-d18c-4c3c-bdfb-851673980fe7")
MINGW_UUIDOF(IDxcOperationResult, "CEDB484A-D4E9-445A-B991-CA21CA157DC2")
MINGW_UUIDOF(IDxcCompiler, "8c210bf3-011f-4422-8d70-6f9acb8db617")
MINGW_UUIDOF(IDxcCompiler2, "A005A9D9-B8BB-4594-B5C9-0E633BEC4D37")
MINGW_UUIDOF(IDxcLinker, "F1B5BE2A-62DD-4327-A1C2-42AC1E1E78E6")
MINGW_UUIDOF(IDxcUtils, "4605C4CB-2019-492A-ADA4-65F20BB7D67F")
MINGW_UUIDOF(IDxcResult, "58346CDA-DDE7-4497-9461-6F87AF5E0659")
MINGW_UUIDOF(IDxcExtraOutputs, "319b37a2-a5c2-494a-a5de-4801b2faf989")
MINGW_UUIDOF(IDxcCompiler3, "228B4687-5A6A-4730-900C-9702B2203F54")
MINGW_UUIDOF(IDxcValidator, "A6E82BD2-1FD7-4826-9811-2857E797F49A")
MINGW_UUIDOF(IDxcValidator2, "458e1fd1-b1b2-4750-a6e1-9c10f03bed92")
MINGW_UUIDOF(IDxcContainerBuilder, "334b1f50-2292-4b35-99a1-25588d8c17fe")
MINGW_UUIDOF(IDxcAssembler, "091f7a26-1c1f-4948-904b-e6e3a8a771d5")
MINGW_UUIDOF(IDxcContainerReflection, "d2c21b26-8350-4bdc-976a-331ce6f4c54c")
MINGW_UUIDOF(IDxcOptimizerPass, "AE2CD79F-CC22-453F-9B6B-B124E7A5204C")
MINGW_UUIDOF(IDxcOptimizer, "25740E2E-9CBA-401B-9119-4FB42F39F270")
MINGW_UUIDOF(IDxcVersionInfo, "b04f5b50-2059-4f12-a8ff-a1e0cde1cc7e")
MINGW_UUIDOF(IDxcVersionInfo2, "fb6904c4-42f0-4b62-9c46-983af7da7c83")
MINGW_UUIDOF(IDxcVersionInfo3, "5e13e843-9d25-473c-9ad2-03b2d0b44b1e")
MINGW_UUIDOF(IDxcPdbUtils, "E6C9647E-9D6A-4C3B-B94C-524B5A6C343D")
#endif

namespace dxc {

#ifdef _WIN32
static void TrimEOL(_Inout_z_ char *pMsg) {
  char *pEnd = pMsg + strlen(pMsg);
  --pEnd;
  while (pEnd > pMsg && (*pEnd == '\r' || *pEnd == '\n')) {
    --pEnd;
  }
  pEnd[1] = '\0';
}

static std::string GetWin32ErrorMessage(DWORD err) {
  char formattedMsg[200];
  DWORD formattedMsgLen =
      FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr, err, 0, formattedMsg, _countof(formattedMsg), 0);
  if (formattedMsgLen > 0 && formattedMsgLen < _countof(formattedMsg)) {
    TrimEOL(formattedMsg);
    return std::string(formattedMsg);
  }
  return std::string();
}
#else
static std::string GetWin32ErrorMessage(DWORD err) {
  // Since we use errno for handling messages, we use strerror to get the error
  // message.
  return std::string(std::strerror(err));
}
#endif // _WIN32

void IFT_Data(HRESULT hr, LPCWSTR data) {
  if (SUCCEEDED(hr)) return;
  CW2A pData(data, CP_UTF8);
  std::string errMsg;
  if (HRESULT_IS_WIN32ERR(hr)) {
    DWORD err = HRESULT_AS_WIN32ERR(hr);
    errMsg.append(GetWin32ErrorMessage(err));
    if (data != nullptr) {
      errMsg.append(" ", 1);
    }
  }
  if (data != nullptr) {
    errMsg.append(pData);
  }
  throw ::hlsl::Exception(hr, errMsg);
}

void EnsureEnabled(DxcDllSupport &dxcSupport) {
  if (!dxcSupport.IsEnabled()) {
    IFT(dxcSupport.Initialize());
  }
}

void ReadFileIntoBlob(DxcDllSupport &dxcSupport, _In_ LPCWSTR pFileName,
                      _COM_Outptr_ IDxcBlobEncoding **ppBlobEncoding) {
  CComPtr<IDxcLibrary> library;
  IFT(dxcSupport.CreateInstance(CLSID_DxcLibrary, &library));
  IFT_Data(library->CreateBlobFromFile(pFileName, nullptr, ppBlobEncoding),
           pFileName);
}

void WriteOperationErrorsToConsole(_In_ IDxcOperationResult *pResult,
                                   bool outputWarnings) {
  HRESULT status;
  IFT(pResult->GetStatus(&status));
  if (FAILED(status) || outputWarnings) {
    CComPtr<IDxcBlobEncoding> pErrors;
    IFT(pResult->GetErrorBuffer(&pErrors));
    if (pErrors.p != nullptr) {
      WriteBlobToConsole(pErrors, STD_ERROR_HANDLE);
    }
  }
}

void WriteOperationResultToConsole(_In_ IDxcOperationResult *pRewriteResult,
                                   bool outputWarnings) {
  WriteOperationErrorsToConsole(pRewriteResult, outputWarnings);

  CComPtr<IDxcBlob> pBlob;
  IFT(pRewriteResult->GetResult(&pBlob));
  WriteBlobToConsole(pBlob, STD_OUTPUT_HANDLE);
}

static void WriteUtf16NullTermToConsole(_In_opt_count_(charCount) const wchar_t *pText,
                                 DWORD streamType) {
  if (pText == nullptr) {
    return;
  }

  bool lossy; // Note: even if there was loss,  print anyway
  std::string consoleMessage;
  Unicode::UTF16ToConsoleString(pText, &consoleMessage, &lossy);
  if (streamType == STD_OUTPUT_HANDLE) {
    fprintf(stdout, "%s\n", consoleMessage.c_str());
  }
  else if (streamType == STD_ERROR_HANDLE) {
    fprintf(stderr, "%s\n", consoleMessage.c_str());
  }
  else {
    throw hlsl::Exception(E_INVALIDARG);
  }
}

static HRESULT BlobToUtf8IfText(_In_opt_ IDxcBlob *pBlob, IDxcBlobUtf8 **ppBlobUtf8) {
  CComPtr<IDxcBlobEncoding> pBlobEncoding;
  if (SUCCEEDED(pBlob->QueryInterface(&pBlobEncoding))) {
    BOOL known;
    UINT32 cp = 0;
    IFT(pBlobEncoding->GetEncoding(&known, &cp));
    if (known) {
      return hlsl::DxcGetBlobAsUtf8(pBlob, nullptr, ppBlobUtf8);
    }
  }
  return S_OK;
}

static HRESULT BlobToUtf16IfText(_In_opt_ IDxcBlob *pBlob, IDxcBlobUtf16 **ppBlobUtf16) {
  CComPtr<IDxcBlobEncoding> pBlobEncoding;
  if (SUCCEEDED(pBlob->QueryInterface(&pBlobEncoding))) {
    BOOL known;
    UINT32 cp = 0;
    IFT(pBlobEncoding->GetEncoding(&known, &cp));
    if (known) {
      return hlsl::DxcGetBlobAsUtf16(pBlob, nullptr, ppBlobUtf16);
    }
  }
  return S_OK;
}

void WriteBlobToConsole(_In_opt_ IDxcBlob *pBlob, DWORD streamType) {
  if (pBlob == nullptr) {
    return;
  }

  // Try to get as UTF-16 or UTF-8
  BOOL known;
  UINT32 cp = 0;
  CComPtr<IDxcBlobEncoding> pBlobEncoding;
  IFT(pBlob->QueryInterface(&pBlobEncoding));
  IFT(pBlobEncoding->GetEncoding(&known, &cp));

  if (cp == DXC_CP_UTF16) {
    CComPtr<IDxcBlobUtf16> pUtf16;
    IFT(hlsl::DxcGetBlobAsUtf16(pBlob, nullptr, &pUtf16));
    WriteUtf16NullTermToConsole(pUtf16->GetStringPointer(), streamType);
  } else if (cp == CP_UTF8) {
    CComPtr<IDxcBlobUtf8> pUtf8;
    IFT(hlsl::DxcGetBlobAsUtf8(pBlob, nullptr, &pUtf8));
    WriteUtf8ToConsoleSizeT(pUtf8->GetStringPointer(), pUtf8->GetStringLength(), streamType);
  }
}

void WriteBlobToFile(_In_opt_ IDxcBlob *pBlob, _In_ LPCWSTR pFileName, _In_ UINT32 textCodePage) {
  if (pBlob == nullptr) {
    return;
  }

  CHandle file(CreateFileW(pFileName, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (file == INVALID_HANDLE_VALUE) {
    IFT_Data(HRESULT_FROM_WIN32(GetLastError()), pFileName);
  }

  WriteBlobToHandle(pBlob, file, pFileName, textCodePage);
}

void WriteBlobToHandle(_In_opt_ IDxcBlob *pBlob, _In_ HANDLE hFile, _In_opt_ LPCWSTR pFileName, _In_ UINT32 textCodePage) {
  if (pBlob == nullptr) {
    return;
  }

  LPCVOID pPtr = pBlob->GetBufferPointer();
  SIZE_T size = pBlob->GetBufferSize();

  std::string BOM;
  CComPtr<IDxcBlobUtf8> pBlobUtf8;
  CComPtr<IDxcBlobUtf16> pBlobUtf16;
  if (textCodePage == DXC_CP_UTF8) {
    IFT_Data(BlobToUtf8IfText(pBlob, &pBlobUtf8), pFileName);
    if (pBlobUtf8) {
      pPtr = pBlobUtf8->GetStringPointer();
      size = pBlobUtf8->GetStringLength();
      // TBD: Should we write UTF-8 BOM?
      //BOM = "\xef\xbb\xbf"; // UTF-8
    }
  } else if (textCodePage == DXC_CP_UTF16) {
    IFT_Data(BlobToUtf16IfText(pBlob, &pBlobUtf16), pFileName);
    if (pBlobUtf16) {
      pPtr = pBlobUtf16->GetStringPointer();
      size = pBlobUtf16->GetStringLength() * sizeof(wchar_t);
      BOM = "\xff\xfe"; // UTF-16 LE
    }
  }

  IFT_Data(size > (SIZE_T)UINT32_MAX ? E_OUTOFMEMORY : S_OK , pFileName);

  DWORD written;

  if (!BOM.empty()) {
    if (FALSE == WriteFile(hFile, BOM.data(), BOM.length(), &written, nullptr)) {
      IFT_Data(HRESULT_FROM_WIN32(GetLastError()), pFileName);
    }
  }

  if (FALSE == WriteFile(hFile, pPtr, (DWORD)size, &written, nullptr)) {
    IFT_Data(HRESULT_FROM_WIN32(GetLastError()), pFileName);
  }
}

void WriteUtf8ToConsole(_In_opt_count_(charCount) const char *pText,
                        int charCount, DWORD streamType) {
  if (charCount == 0 || pText == nullptr) {
    return;
  }

  std::string resultToPrint;
  wchar_t *utf16Message = nullptr;
  size_t utf16MessageLen;
  Unicode::UTF8BufferToUTF16Buffer(pText, charCount, &utf16Message,
                                   &utf16MessageLen);

  WriteUtf16NullTermToConsole(utf16Message, streamType);

  delete[] utf16Message;
}

void WriteUtf8ToConsoleSizeT(_In_opt_count_(charCount) const char *pText,
  size_t charCount, DWORD streamType) {
  if (charCount == 0) {
    return;
  }

  int charCountInt = 0;
  IFT(SizeTToInt(charCount, &charCountInt));
  WriteUtf8ToConsole(pText, charCountInt, streamType);
}

} // namespace dxc
