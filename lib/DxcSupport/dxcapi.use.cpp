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
#include "dxc/Support/WinFunctions.h"

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
  if (formattedMsg > 0 && formattedMsgLen < _countof(formattedMsg)) {
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

void WriteBlobToConsole(_In_opt_ IDxcBlob *pBlob, DWORD streamType) {
  if (pBlob == nullptr) {
    return;
  }

  // Assume UTF-8 for now, which is typically the case for dxcompiler ouput.
  WriteUtf8ToConsoleSizeT((char *)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), streamType);
}

void WriteBlobToFile(_In_opt_ IDxcBlob *pBlob, _In_ LPCWSTR pFileName) {
  if (pBlob == nullptr) {
    return;
  }

  CHandle file(CreateFileW(pFileName, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (file == INVALID_HANDLE_VALUE) {
    IFT_Data(HRESULT_FROM_WIN32(GetLastError()), pFileName);
  }
  WriteBlobToHandle(pBlob, file, pFileName);
}

void WriteBlobToHandle(_In_opt_ IDxcBlob *pBlob, _In_ HANDLE hFile, _In_opt_ LPCWSTR pFileName) {
  if (pBlob == nullptr) {
    return;
  }

  DWORD written;
  if (FALSE == WriteFile(hFile, pBlob->GetBufferPointer(),
    pBlob->GetBufferSize(), &written, nullptr)) {
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
  bool lossy; // Note: even if there was loss,  print anyway
  Unicode::UTF8BufferToUTF16Buffer(pText, charCount, &utf16Message,
                                   &utf16MessageLen);

  std::string consoleMessage;
  Unicode::UTF16ToConsoleString(utf16Message, &consoleMessage, &lossy);
  if (streamType == STD_OUTPUT_HANDLE) {
    fprintf(stdout, "%s\n", consoleMessage.c_str());
  }
  else if (streamType == STD_ERROR_HANDLE) {
    fprintf(stderr, "%s\n", consoleMessage.c_str());
  }
  else {
    throw hlsl::Exception(E_INVALIDARG);
  }

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
