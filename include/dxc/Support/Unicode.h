///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Unicode.h                                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides utitlity functions to work with Unicode and other encodings.     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

#ifdef _WIN32
#include <specstrings.h>
#else
// MultiByteToWideChar which is a Windows-specific method.
// This is a very simplistic implementation for non-Windows platforms. This
// implementation completely ignores CodePage and dwFlags.
int MultiByteToWideChar(uint32_t CodePage, uint32_t dwFlags,
                        const char *lpMultiByteStr, int cbMultiByte,
                        wchar_t *lpWideCharStr, int cchWideChar);

// WideCharToMultiByte is a Windows-specific method.
// This is a very simplistic implementation for non-Windows platforms. This
// implementation completely ignores CodePage and dwFlags.
int WideCharToMultiByte(uint32_t CodePage, uint32_t dwFlags,
                        const wchar_t *lpWideCharStr, int cchWideChar,
                        char *lpMultiByteStr, int cbMultiByte,
                        const char *lpDefaultChar = nullptr,
                        bool *lpUsedDefaultChar = nullptr);
#endif // _WIN32

namespace Unicode
{

// Based on http://msdn.microsoft.com/en-us/library/windows/desktop/dd374101(v=vs.85).aspx.
enum class Encoding { ASCII = 0, UTF8, UTF8_BOM, UTF16_LE, UTF16_BE, UTF32_LE, UTF32_BE };

// An acp_char is a character encoded in the current Windows ANSI code page.
typedef char acp_char;

// A ccp_char is a character encoded in the console code page.
typedef char ccp_char;

_Success_(return != false)
bool UTF8ToConsoleString(_In_opt_count_(textLen) const char* text, _In_ size_t textLen, _Inout_ std::string* pValue, _Out_opt_ bool* lossy);

_Success_(return != false)
bool UTF8ToConsoleString(_In_z_ const char* text, _Inout_ std::string* pValue, _Out_opt_ bool* lossy);

_Success_(return != false)
bool UTF16ToConsoleString(_In_opt_count_(textLen) const wchar_t* text, _In_ size_t textLen, _Inout_ std::string* pValue, _Out_opt_ bool* lossy);

_Success_(return != false)
bool UTF16ToConsoleString(_In_z_ const wchar_t* text, _Inout_ std::string* pValue, _Out_opt_ bool* lossy);

_Success_(return != false)
bool UTF8ToUTF16String(_In_opt_z_ const char *pUTF8, _Inout_ std::wstring *pUTF16);

_Success_(return != false)
bool UTF8ToUTF16String(_In_opt_count_(cbUTF8) const char *pUTF8, size_t cbUTF8, _Inout_ std::wstring *pUTF16);

std::wstring UTF8ToUTF16StringOrThrow(_In_z_ const char *pUTF8);

_Success_(return != false)
bool UTF16ToUTF8String(_In_z_ const wchar_t *pUTF16, size_t cUTF16, _Inout_ std::string *pUTF8);
bool UTF16ToUTF8String(_In_z_ const wchar_t *pUTF16, _Inout_ std::string *pUTF8);

std::string UTF16ToUTF8StringOrThrow(_In_z_ const wchar_t *pUTF16);

bool IsStarMatchUTF8(_In_reads_opt_(maskLen) const char *pMask, size_t maskLen,
                     _In_reads_opt_(nameLen) const char *pName, size_t nameLen);
bool IsStarMatchUTF16(_In_reads_opt_(maskLen) const wchar_t *pMask, size_t maskLen,
                      _In_reads_opt_(nameLen) const wchar_t *pName, size_t nameLen);

_Success_(return != false)
bool UTF8BufferToUTF16ComHeap(_In_z_ const char *pUTF8,
                              _Outptr_result_z_ wchar_t **ppUTF16) throw();

_Success_(return != false)
bool UTF8BufferToUTF16Buffer(
  _In_NLS_string_(cbUTF8) const char *pUTF8,
  int cbUTF8, 
  _Outptr_result_buffer_(*pcchUTF16) wchar_t **ppUTF16,
  size_t *pcchUTF16) throw();

_Success_(return != false)
bool UTF16BufferToUTF8Buffer(
  _In_NLS_string_(cchUTF16) const wchar_t *pUTF16,
  int cchUTF16,
  _Outptr_result_buffer_(*pcbUTF8) char **ppUTF8,
  size_t *pcbUTF8) throw();

}  // namespace Unicode
