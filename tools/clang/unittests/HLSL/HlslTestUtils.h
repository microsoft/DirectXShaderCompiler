///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HlslTestUtils.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides utility functions for HLSL tests.                                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <string>
#include <sstream>
#include <fstream>
#include "dxc/Support/Unicode.h"

// If TAEF verify macros are available, use them to alias other legacy
// comparison macros that don't have a direct translation.
//
// Other common replacements are as follows.
//
// EXPECT_EQ -> VERIFY_ARE_EQUAL
// ASSERT_EQ -> VERIFY_ARE_EQUAL
//
// Note that whether verification throws or continues depends on
// preprocessor settings.

#ifdef VERIFY_ARE_EQUAL
#define EXPECT_STREQ(a, b) VERIFY_ARE_EQUAL(0, strcmp(a, b))
#define EXPECT_STREQW(a, b) VERIFY_ARE_EQUAL(0, wcscmp(a, b))
#define VERIFY_ARE_EQUAL_CMP(a, b, ...) VERIFY_IS_TRUE(a == b, __VA_ARGS__)
#define VERIFY_ARE_EQUAL_STR(a, b, ...) { \
  const char *pTmpA = (a);\
  const char *pTmpB = (b);\
  if (0 != strcmp(pTmpA, pTmpB)) {\
    CA2W conv(pTmpB, CP_UTF8); WEX::Logging::Log::Comment(conv);\
    const char *pA = pTmpA; const char *pB = pTmpB; \
    while(*pA == *pB) { pA++; pB++; } \
    wchar_t diffMsg[32]; swprintf_s(diffMsg, _countof(diffMsg), L"diff at %u", (unsigned)(pA-pTmpA)); \
    WEX::Logging::Log::Comment(diffMsg); \
  } \
  VERIFY_ARE_EQUAL(0, strcmp(pTmpA, pTmpB), __VA_ARGS__); \
}
#define VERIFY_ARE_EQUAL_WSTR(a, b, ...) { \
  if (0 != wcscmp(a, b)) { WEX::Logging::Log::Comment(b);} \
  VERIFY_ARE_EQUAL(0, wcscmp(a, b), __VA_ARGS__); \
}
#define ASSERT_EQ(expected, actual) VERIFY_ARE_EQUAL(expected, actual)
#define ASSERT_NE(expected, actual) VERIFY_ARE_NOT_EQUAL(expected, actual)
#define TEST_F(typeName, functionName) void typeName::functionName()
#define ASSERT_HRESULT_SUCCEEDED VERIFY_SUCCEEDED
#define EXPECT_EQ(expected, actual) VERIFY_ARE_EQUAL(expected, actual)
#endif

namespace hlsl_test {

inline std::wstring
vFormatToWString(_In_z_ _Printf_format_string_ const wchar_t *fmt, va_list argptr) {
  std::wstring result;
  int len = _vscwprintf(fmt, argptr);
  result.resize(len + 1);
  vswprintf_s((wchar_t *)result.data(), len + 1, fmt, argptr);
  return result;
}

inline std::wstring
FormatToWString(_In_z_ _Printf_format_string_ const wchar_t *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::wstring result(vFormatToWString(fmt, args));
  va_end(args);
  return result;
}

inline void LogCommentFmt(_In_z_ _Printf_format_string_ const wchar_t *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::wstring buf(vFormatToWString(fmt, args));
  va_end(args);
  WEX::Logging::Log::Comment(buf.data());
}

inline std::wstring GetPathToHlslDataFile(const wchar_t* relative) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  WEX::Common::String HlslDataDirValue;
  ASSERT_HRESULT_SUCCEEDED(WEX::TestExecution::RuntimeParameters::TryGetValue(L"HlslDataDir", HlslDataDirValue));

  wchar_t envPath[MAX_PATH];
  wchar_t expanded[MAX_PATH];
  swprintf_s(envPath, _countof(envPath), L"%s\\%s", reinterpret_cast<wchar_t*>(HlslDataDirValue.GetBuffer()), relative);
  VERIFY_WIN32_BOOL_SUCCEEDED(ExpandEnvironmentStringsW(envPath, expanded, _countof(expanded)));
  return std::wstring(expanded);
}

inline bool PathLooksAbsolute(LPCWSTR name) {
  // Very simplified, only for the cases we care about in the test suite.
  return name && *name && ((*name == L'\\') || (name[1] == L':'));
}

inline std::string GetFirstLine(LPCWSTR name) {
  char firstLine[300];
  memset(firstLine, 0, sizeof(firstLine));

  const std::wstring path = PathLooksAbsolute(name)
                                ? std::wstring(name)
                                : hlsl_test::GetPathToHlslDataFile(name);
  std::ifstream infile(path);
  if (infile.bad()) {
    std::wstring errMsg(L"Unable to read file ");
    errMsg += path;
    WEX::Logging::Log::Error(errMsg.c_str());
    VERIFY_FAIL();
  }

  infile.getline(firstLine, _countof(firstLine));
  return firstLine;
}

inline HANDLE CreateFileForReading(LPCWSTR path) {
  HANDLE sourceHandle = CreateFileW(path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
  if (sourceHandle == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    std::wstring errorMessage(FormatToWString(L"Unable to open file '%s', err=%u", path, err).c_str());
    VERIFY_SUCCEEDED(HRESULT_FROM_WIN32(err), errorMessage.c_str());
  }
  return sourceHandle;
}

inline HANDLE CreateNewFileForReadWrite(LPCWSTR path) {
  HANDLE sourceHandle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if (sourceHandle == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    std::wstring errorMessage(FormatToWString(L"Unable to create file '%s', err=%u", path, err).c_str());
    VERIFY_SUCCEEDED(HRESULT_FROM_WIN32(err), errorMessage.c_str());
  }
  return sourceHandle;
}

inline bool GetTestParamBool(LPCWSTR name) {
  WEX::Common::String ParamValue;
  WEX::Common::String NameValue;
  if (FAILED(WEX::TestExecution::RuntimeParameters::TryGetValue(name,
                                                                ParamValue))) {
    return false;
  }
  if (ParamValue.IsEmpty()) {
    return false;
  }
  if (0 == wcscmp(ParamValue, L"*")) {
    return true;
  }
  VERIFY_SUCCEEDED(WEX::TestExecution::RuntimeParameters::TryGetValue(
      L"TestName", NameValue));
  if (NameValue.IsEmpty()) {
    return false;
  }
  return Unicode::IsStarMatchUTF16(ParamValue, ParamValue.GetLength(),
                                   NameValue, NameValue.GetLength());
}

inline bool GetTestParamUseWARP(bool defaultVal) {
  WEX::Common::String AdapterValue;
  if (FAILED(WEX::TestExecution::RuntimeParameters::TryGetValue(
          L"Adapter", AdapterValue))) {
    return defaultVal;
  }
  if (defaultVal && AdapterValue.IsEmpty() ||
      AdapterValue.CompareNoCase(L"WARP") == 0) {
    return true;
  }
  return false;
}

}

#define SIMPLE_IUNKNOWN_IMPL1(_IFACE_) \
  private: volatile ULONG m_dwRef; \
  public:\
  ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement(&m_dwRef); } \
  ULONG STDMETHODCALLTYPE Release() { \
    ULONG result = InterlockedDecrement(&m_dwRef); \
    if (result == 0) delete this; \
    return result; \
  } \
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) { \
    if (ppvObject == nullptr) return E_POINTER; \
    if (IsEqualIID(iid, __uuidof(IUnknown)) || \
      IsEqualIID(iid, __uuidof(INoMarshal)) || \
      IsEqualIID(iid, __uuidof(_IFACE_))) { \
      *ppvObject = reinterpret_cast<_IFACE_*>(this); \
      reinterpret_cast<_IFACE_*>(this)->AddRef(); \
      return S_OK; \
    } \
    return E_NOINTERFACE; \
  }

