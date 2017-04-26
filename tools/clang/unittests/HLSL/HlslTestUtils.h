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
#include <dxgiformat.h>

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

inline void LogErrorFmt(_In_z_ _Printf_format_string_ const wchar_t *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::wstring buf(vFormatToWString(fmt, args));
    va_end(args);
    WEX::Logging::Log::Error(buf.data());
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

inline bool isdenorm(float f) {
  return FP_SUBNORMAL == fpclassify(f);
}

inline bool isdenorm(double d) {
  return FP_SUBNORMAL == fpclassify(d);
}

inline float ifdenorm_flushf(float a) {
  return isdenorm(a) ? copysign(0.0f, a) : a;
}

inline bool ifdenorm_flushf_eq(float a, float b) {
  return ifdenorm_flushf(a) == ifdenorm_flushf(b);
}

inline bool ifdenorm_flushf_eq_or_nans(float a, float b) {
  if (isnan(a) && isnan(b)) return true;
  return ifdenorm_flushf(a) == ifdenorm_flushf(b);
}

inline bool CompareFloatULP(const float &fsrc, const float &fref, int ULPTolerance) {
    if (isnan(fsrc)) {
        return isnan(fref);
    }
    if (isdenorm(fref)) { // Arithmetic operations of denorm may flush to sign-preserved zero
        return (isdenorm(fsrc) || fsrc == 0) && (signbit(fsrc) == signbit(fref));
    }
    if (fsrc == fref) {
        return true;
    }
    int diff = *((DWORD *)&fsrc) - *((DWORD *)&fref);
    unsigned int uDiff = diff < 0 ? -diff : diff;
    return uDiff <= (unsigned int)ULPTolerance;
}

inline bool CompareFloatEpsilon(const float &fsrc, const float &fref, float epsilon) {
    if (isnan(fsrc)) {
        return isnan(fref);
    }
    if (isdenorm(fref)) { // Arithmetic operations of denorm may flush to sign-preserved zero
        return (isdenorm(fsrc) || fsrc == 0) && (signbit(fsrc) == signbit(fref));
    }
    return fsrc == fref || fabsf(fsrc - fref) < epsilon;
}

// Compare using relative error (relative error < 2^{nRelativeExp})
inline bool CompareFloatRelativeEpsilon(const float &fsrc, const float &fref, int nRelativeExp) {
    return CompareFloatULP(fsrc, fref, 23 - nRelativeExp);
}

// returns the number of bytes per pixel for a given dxgi format
// add more cases if different format needed to copy back resources
inline UINT GetByteSizeForFormat(DXGI_FORMAT value) {
    switch (value) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: return 16;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
    case DXGI_FORMAT_R32G32B32A32_UINT: return 16;
    case DXGI_FORMAT_R32G32B32A32_SINT: return 16;
    case DXGI_FORMAT_R32G32B32_TYPELESS: return 12;
    case DXGI_FORMAT_R32G32B32_FLOAT: return 12;
    case DXGI_FORMAT_R32G32B32_UINT: return 12;
    case DXGI_FORMAT_R32G32B32_SINT: return 12;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return 8;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return 8;
    case DXGI_FORMAT_R16G16B16A16_UNORM: return 8;
    case DXGI_FORMAT_R16G16B16A16_UINT: return 8;
    case DXGI_FORMAT_R16G16B16A16_SNORM: return 8;
    case DXGI_FORMAT_R16G16B16A16_SINT: return 8;
    case DXGI_FORMAT_R32G32_TYPELESS: return 8;
    case DXGI_FORMAT_R32G32_FLOAT: return 8;
    case DXGI_FORMAT_R32G32_UINT: return 8;
    case DXGI_FORMAT_R32G32_SINT: return 8;
    case DXGI_FORMAT_R32G8X24_TYPELESS: return 8;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return 4;
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: return 4;
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: return 4;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return 4;
    case DXGI_FORMAT_R10G10B10A2_UNORM: return 4;
    case DXGI_FORMAT_R10G10B10A2_UINT: return 4;
    case DXGI_FORMAT_R11G11B10_FLOAT: return 4;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return 4;
    case DXGI_FORMAT_R8G8B8A8_UNORM: return 4;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return 4;
    case DXGI_FORMAT_R8G8B8A8_UINT: return 4;
    case DXGI_FORMAT_R8G8B8A8_SNORM: return 4;
    case DXGI_FORMAT_R8G8B8A8_SINT: return 4;
    case DXGI_FORMAT_R16G16_TYPELESS: return 4;
    case DXGI_FORMAT_R16G16_FLOAT: return 4;
    case DXGI_FORMAT_R16G16_UNORM: return 4;
    case DXGI_FORMAT_R16G16_UINT: return 4;
    case DXGI_FORMAT_R16G16_SNORM: return 4;
    case DXGI_FORMAT_R16G16_SINT: return 4;
    case DXGI_FORMAT_R32_TYPELESS: return 4;
    case DXGI_FORMAT_D32_FLOAT: return 4;
    case DXGI_FORMAT_R32_FLOAT: return 4;
    case DXGI_FORMAT_R32_UINT: return 4;
    case DXGI_FORMAT_R32_SINT: return 4;
    case DXGI_FORMAT_R24G8_TYPELESS: return 4;
    case DXGI_FORMAT_D24_UNORM_S8_UINT: return 4;
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS: return 4;
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return 4;
    case DXGI_FORMAT_R8G8_TYPELESS: return 2;
    case DXGI_FORMAT_R8G8_UNORM: return 2;
    case DXGI_FORMAT_R8G8_UINT: return 2;
    case DXGI_FORMAT_R8G8_SNORM: return 2;
    case DXGI_FORMAT_R8G8_SINT: return 2;
    case DXGI_FORMAT_R16_TYPELESS: return 2;
    case DXGI_FORMAT_R16_FLOAT: return 2;
    case DXGI_FORMAT_D16_UNORM: return 2;
    case DXGI_FORMAT_R16_UNORM: return 2;
    case DXGI_FORMAT_R16_UINT: return 2;
    case DXGI_FORMAT_R16_SNORM: return 2;
    case DXGI_FORMAT_R16_SINT: return 2;
    case DXGI_FORMAT_R8_TYPELESS: return 1;
    case DXGI_FORMAT_R8_UNORM: return 1;
    case DXGI_FORMAT_R8_UINT: return 1;
    case DXGI_FORMAT_R8_SNORM: return 1;
    case DXGI_FORMAT_R8_SINT: return 1;
    case DXGI_FORMAT_A8_UNORM: return 1;
    case DXGI_FORMAT_R1_UNORM: return 1;
    default:
        VERIFY_FAILED(E_INVALIDARG);
        return 0;
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

