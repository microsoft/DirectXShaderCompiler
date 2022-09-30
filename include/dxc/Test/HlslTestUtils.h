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

// *** THIS FILE CANNOT TAKE ANY LLVM DEPENDENCIES  *** //

#include <string>
#include <sstream>
#include <fstream>
#include <atomic>
#include <cmath>
#include <vector>
#include <algorithm>
#ifdef _WIN32
#include <dxgiformat.h>
#include "WexTestClass.h"
#else
#include "dxc/Support/Global.h" // DXASSERT_LOCALVAR
#include "WEXAdapter.h"
#endif
#include "dxc/Support/Unicode.h"
#include "dxc/DXIL/DxilConstants.h" // DenormMode

#include "dxc/Support/dxcapi.use.h" // disassembleProgram
#include "dxc/Support/Global.h" // IFT and other macros

using namespace std;

#ifndef HLSLDATAFILEPARAM
#define HLSLDATAFILEPARAM L"HlslDataDir"
#endif

#ifndef FILECHECKDUMPDIRPARAM
#define FILECHECKDUMPDIRPARAM L"FileCheckDumpDir"
#endif

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
#ifndef EXPECT_STREQ
#define EXPECT_STREQ(a, b) VERIFY_ARE_EQUAL(0, strcmp(a, b))
#endif
#define EXPECT_STREQW(a, b) VERIFY_ARE_EQUAL(0, wcscmp(a, b))
#define VERIFY_ARE_EQUAL_CMP(a, b, ...) VERIFY_IS_TRUE(a == b, __VA_ARGS__)
#define VERIFY_ARE_EQUAL_STR(a, b) { \
  const char *pTmpA = (a);\
  const char *pTmpB = (b);\
  if (0 != strcmp(pTmpA, pTmpB)) {\
    CA2W conv(pTmpB, CP_UTF8); WEX::Logging::Log::Comment(conv);\
    const char *pA = pTmpA; const char *pB = pTmpB; \
    while(*pA == *pB) { pA++; pB++; } \
    wchar_t diffMsg[32]; swprintf_s(diffMsg, _countof(diffMsg), L"diff at %u", (unsigned)(pA-pTmpA)); \
    WEX::Logging::Log::Comment(diffMsg); \
  } \
  VERIFY_ARE_EQUAL(0, strcmp(pTmpA, pTmpB)); \
}
#define VERIFY_ARE_EQUAL_WSTR(a, b) { \
  if (0 != wcscmp(a, b)) { WEX::Logging::Log::Comment(b);} \
  VERIFY_ARE_EQUAL(0, wcscmp(a, b)); \
}
#ifndef ASSERT_EQ
#define ASSERT_EQ(expected, actual) VERIFY_ARE_EQUAL(expected, actual)
#endif
#ifndef ASSERT_NE
#define ASSERT_NE(expected, actual) VERIFY_ARE_NOT_EQUAL(expected, actual)
#endif
#ifndef TEST_F
#define TEST_F(typeName, functionName) void typeName::functionName()
#endif
#define ASSERT_HRESULT_SUCCEEDED VERIFY_SUCCEEDED
#ifndef EXPECT_EQ
#define EXPECT_EQ(expected, actual) VERIFY_ARE_EQUAL(expected, actual)
#endif 
#endif // VERIFY_ARE_EQUAL

static constexpr char whitespaceChars[] = " \t\r\n";

inline std::string strltrim(const std::string& value) {
  size_t first = value.find_first_not_of(whitespaceChars);
  return first == string::npos ? value : value.substr(first);
}

inline std::string strrtrim(const std::string& value) {
  size_t last = value.find_last_not_of(whitespaceChars);
  return last == string::npos ? value : value.substr(0, last + 1);
}

inline std::string strtrim(const std::string& value) {
  return strltrim(strrtrim(value));
}

inline bool strstartswith(const std::string& value, const char* pattern) {
  for (size_t i = 0; ; ++i) {
    if (pattern[i] == '\0') return true;
    if (i == value.size() || value[i] != pattern[i]) return false;
  }
}

std::vector<std::string> strtok(const std::string& value, const char* delimiters = whitespaceChars);

namespace hlsl_test {
std::wstring vFormatToWString(_In_z_ _Printf_format_string_ const wchar_t* fmt, va_list argptr);

std::wstring FormatToWString(_In_z_ _Printf_format_string_ const wchar_t* fmt, ...);
  
void LogCommentFmt(_In_z_ _Printf_format_string_ const wchar_t* fmt, ...);

void LogErrorFmt(_In_z_ _Printf_format_string_ const wchar_t* fmt, ...);

std::wstring GetPathToHlslDataFile(const wchar_t* relative, LPCWSTR paramName = HLSLDATAFILEPARAM);

bool PathLooksAbsolute(LPCWSTR name);

inline static bool HasRunLine(std::string& line) {
  const char* delimiters = " ;/";
  auto lineelems = strtok(line, delimiters);
  return !lineelems.empty() &&
    lineelems.front().compare("RUN:") == 0;
}

std::vector<std::string> GetRunLines(const LPCWSTR name);

std::string GetFirstLine(LPCWSTR name);

HANDLE CreateFileForReading(LPCWSTR path);

HANDLE CreateNewFileForReadWrite(LPCWSTR path);

bool GetTestParamBool(LPCWSTR name);

bool GetTestParamUseWARP(bool defaultVal);
}
#ifdef FP_SUBNORMAL

inline bool isdenorm(float f) {
  return FP_SUBNORMAL == std::fpclassify(f);
}

#else

inline bool isdenorm(float f) {
  return (std::numeric_limits<float>::denorm_min() <= f && f < std::numeric_limits<float>::min()) ||
         (-std::numeric_limits<float>::min() < f && f <= -std::numeric_limits<float>::denorm_min());
}

#endif // FP_SUBNORMAL

inline float ifdenorm_flushf(float a) {
  return isdenorm(a) ? copysign(0.0f, a) : a;
}

inline bool ifdenorm_flushf_eq(float a, float b) {
  return ifdenorm_flushf(a) == ifdenorm_flushf(b);
}

static const uint16_t Float16NaN = 0xff80;
static const uint16_t Float16PosInf = 0x7c00;
static const uint16_t Float16NegInf = 0xfc00;
static const uint16_t Float16PosDenorm = 0x0008;
static const uint16_t Float16NegDenorm = 0x8008;
static const uint16_t Float16PosZero = 0x0000;
static const uint16_t Float16NegZero = 0x8000;

inline bool GetSign(float x) {
  return std::signbit(x);
}

inline int GetMantissa(float x) {
  int bits = reinterpret_cast<int &>(x);
  return bits & 0x7fffff;
}

inline int GetExponent(float x) {
  int bits = reinterpret_cast<int &>(x);
  return (bits >> 23) & 0xff;
}

#define FLOAT16_BIT_SIGN 0x8000
#define FLOAT16_BIT_EXP 0x7c00
#define FLOAT16_BIT_MANTISSA 0x03ff
#define FLOAT16_BIGGEST_DENORM FLOAT16_BIT_MANTISSA
#define FLOAT16_BIGGEST_NORMAL 0x7bff

inline bool isnanFloat16(uint16_t val) {
  return (val & FLOAT16_BIT_EXP) == FLOAT16_BIT_EXP &&
         (val & FLOAT16_BIT_MANTISSA) != 0;
}

uint16_t ConvertFloat32ToFloat16(float val);

float ConvertFloat16ToFloat32(uint16_t x);

uint16_t ConvertFloat32ToFloat16(float val);

float ConvertFloat16ToFloat32(uint16_t val);

bool CompareFloatULP(const float& fsrc, const float& fref, int ULPTolerance,
  hlsl::DXIL::Float32DenormMode mode = hlsl::DXIL::Float32DenormMode::Any);

bool CompareFloatEpsilon(const float& fsrc, const float& fref, float epsilon,
  hlsl::DXIL::Float32DenormMode mode = hlsl::DXIL::Float32DenormMode::Any);

// Compare using relative error (relative error < 2^{nRelativeExp})
inline bool CompareFloatRelativeEpsilon(const float& fsrc, const float& fref, int nRelativeExp,
  hlsl::DXIL::Float32DenormMode mode = hlsl::DXIL::Float32DenormMode::Any) {
  return CompareFloatULP(fsrc, fref, 23 - nRelativeExp, mode);
}

bool CompareHalfULP(const uint16_t& fsrc, const uint16_t& fref, float ULPTolerance);

bool CompareHalfEpsilon(const uint16_t& fsrc, const uint16_t& fref, float epsilon);

void ReplaceDisassemblyTextWithoutRegex(const std::vector<std::string>& lookFors,
  const std::vector<std::string>& replacements,
  std::string& disassembly);

inline void CheckOperationSucceeded(IDxcOperationResult* pResult, IDxcBlob** ppBlob) {
  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
  VERIFY_SUCCEEDED(pResult->GetResult(ppBlob));
}

void AssembleToContainer(dxc::DxcDllSupport& dllSupport, IDxcBlob* pModule,
  IDxcBlob** pContainer);

void MultiByteStringToBlob(dxc::DxcDllSupport& dllSupport,
  const std::string& val, UINT32 codePage,
  _Outptr_ IDxcBlobEncoding** ppBlob);

inline void MultiByteStringToBlob(dxc::DxcDllSupport& dllSupport,
  const std::string& val, UINT32 codePage,
  _Outptr_ IDxcBlob** ppBlob) {
  MultiByteStringToBlob(dllSupport, val, codePage, (IDxcBlobEncoding**)ppBlob);
}
inline void Utf8ToBlob(dxc::DxcDllSupport& dllSupport, const char* pVal, _Outptr_ IDxcBlobEncoding** ppBlob) {
  CComPtr<IDxcLibrary> library;
  IFT(dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
  IFT(library->CreateBlobWithEncodingOnHeapCopy(pVal, (UINT32)strlen(pVal), CP_UTF8, ppBlob));
}

inline void Utf8ToBlob(dxc::DxcDllSupport& dllSupport, const std::string& val,
  _Outptr_ IDxcBlobEncoding** ppBlob) {
  MultiByteStringToBlob(dllSupport, val, CP_UTF8, ppBlob);
}

inline void Utf8ToBlob(dxc::DxcDllSupport& dllSupport, const std::string& val,
  _Outptr_ IDxcBlob** ppBlob) {
  Utf8ToBlob(dllSupport, val, (IDxcBlobEncoding**)ppBlob);
}

void VerifyCompileOK(dxc::DxcDllSupport& dllSupport, LPCSTR pText,
  LPWSTR pTargetProfile, std::vector<LPCWSTR>& args,
  _Outptr_ IDxcBlob** ppResult);

void VerifyCompileOK(dxc::DxcDllSupport& dllSupport, LPCSTR pText,
  LPWSTR pTargetProfile, LPCWSTR pArgs,
  _Outptr_ IDxcBlob** ppResult);

std::string BlobToUtf8(_In_ IDxcBlob* pBlob);

std::string DisassembleProgram(dxc::DxcDllSupport& dllSupport, IDxcBlob* pProgram);

inline bool CompareHalfRelativeEpsilon(const uint16_t& fsrc, const uint16_t& fref, int nRelativeExp) {
  return CompareHalfULP(fsrc, fref, (float)(10 - nRelativeExp));
}

#ifdef _WIN32
// returns the number of bytes per pixel for a given dxgi format
// add more cases if different format needed to copy back resources
UINT GetByteSizeForFormat(DXGI_FORMAT value);
#endif
