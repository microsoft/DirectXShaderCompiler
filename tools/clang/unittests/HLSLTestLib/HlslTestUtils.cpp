#include "dxc/Support/WinIncludes.h"
#include "dxc/Test/HlslTestUtils.h"
#include <vector>
#include <string>

// *** THIS FILE CANNOT TAKE ANY LLVM DEPENDENCIES  *** //

std::vector<std::string> strtok(const std::string& value, const char* delimiters) {
  size_t searchOffset = 0;
  std::vector<std::string> tokens;
  while (searchOffset != value.size()) {
    size_t tokenStartIndex = value.find_first_not_of(delimiters, searchOffset);
    if (tokenStartIndex == std::string::npos) break;
    size_t tokenEndIndex = value.find_first_of(delimiters, tokenStartIndex);
    if (tokenEndIndex == std::string::npos) tokenEndIndex = value.size();
    tokens.emplace_back(value.substr(tokenStartIndex, tokenEndIndex - tokenStartIndex));
    searchOffset = tokenEndIndex;
  }
  return tokens;
}

std::wstring hlsl_test::vFormatToWString(_In_z_ _Printf_format_string_ const wchar_t* fmt, va_list argptr) {
  std::wstring result;
#ifdef _WIN32
  int len = _vscwprintf(fmt, argptr);
  result.resize(len + 1);
  vswprintf_s((wchar_t*)result.data(), len + 1, fmt, argptr);
#else
  wchar_t fmtOut[1000];
  int len = vswprintf(fmtOut, 1000, fmt, argptr);
  DXASSERT_LOCALVAR(len, len >= 0,
    "Too long formatted string in vFormatToWstring");
  result = fmtOut;
#endif
  return result;
}

std::wstring hlsl_test::FormatToWString(_In_z_ _Printf_format_string_ const wchar_t* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::wstring result(hlsl_test::vFormatToWString(fmt, args));
  va_end(args);
  return result;
}

void hlsl_test::LogCommentFmt(_In_z_ _Printf_format_string_ const wchar_t* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::wstring buf(hlsl_test::vFormatToWString(fmt, args));
  va_end(args);
  WEX::Logging::Log::Comment(buf.data());
}

void hlsl_test::LogErrorFmt(_In_z_ _Printf_format_string_ const wchar_t* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::wstring buf(hlsl_test::vFormatToWString(fmt, args));
  va_end(args);
  WEX::Logging::Log::Error(buf.data());
}

std::wstring hlsl_test::GetPathToHlslDataFile(const wchar_t* relative, LPCWSTR paramName) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  WEX::Common::String HlslDataDirValue;
  if (std::wstring(paramName).compare(HLSLDATAFILEPARAM) != 0) {
    // Not fatal, for instance, FILECHECKDUMPDIRPARAM will dump files before running FileCheck, so they can be compared run to run
    if (FAILED(WEX::TestExecution::RuntimeParameters::TryGetValue(paramName, HlslDataDirValue)))
      return std::wstring();
  }
  else {
    ASSERT_HRESULT_SUCCEEDED(WEX::TestExecution::RuntimeParameters::TryGetValue(HLSLDATAFILEPARAM, HlslDataDirValue));
  }
  wchar_t envPath[MAX_PATH];
  wchar_t expanded[MAX_PATH];
  swprintf_s(envPath, _countof(envPath), L"%ls\\%ls", reinterpret_cast<const wchar_t*>(HlslDataDirValue.GetBuffer()), relative);
  VERIFY_WIN32_BOOL_SUCCEEDED(ExpandEnvironmentStringsW(envPath, expanded, _countof(expanded)));
  return std::wstring(expanded);
}

bool hlsl_test::PathLooksAbsolute(LPCWSTR name) {
  // Very simplified, only for the cases we care about in the test suite.
#ifdef _WIN32
  return name && *name && ((*name == L'\\') || (name[1] == L':'));
#else
  return name && *name && (*name == L'/');
#endif
}

std::vector<std::string> hlsl_test::GetRunLines(const LPCWSTR name) {
  const std::wstring path = hlsl_test::PathLooksAbsolute(name)
    ? std::wstring(name)
    : hlsl_test::GetPathToHlslDataFile(name);
#ifdef _WIN32
  std::ifstream infile(path);
#else
  std::ifstream infile((CW2A(path.c_str())));
#endif
  if (infile.bad()) {
    std::wstring errMsg(L"Unable to read file ");
    errMsg += path;
    WEX::Logging::Log::Error(errMsg.c_str());
    VERIFY_FAIL();
  }
  std::vector<std::string> runlines;
  std::string line;
  while (std::getline(infile, line)) {
    if (!hlsl_test::HasRunLine(line))
      continue;
    runlines.emplace_back(line);
  }
  return runlines;
}

std::string hlsl_test::GetFirstLine(LPCWSTR name) {
  const std::wstring path = hlsl_test::PathLooksAbsolute(name)
    ? std::wstring(name)
    : hlsl_test::GetPathToHlslDataFile(name);
#ifdef _WIN32
  std::ifstream infile(path);
#else
  std::ifstream infile((CW2A(path.c_str())));
#endif
  if (infile.bad()) {
    std::wstring errMsg(L"Unable to read file ");
    errMsg += path;
    WEX::Logging::Log::Error(errMsg.c_str());
    VERIFY_FAIL();
  }
  std::string line;
  std::getline(infile, line);
  return line;
}

HANDLE hlsl_test::CreateFileForReading(LPCWSTR path) {
  HANDLE sourceHandle = CreateFileW(path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
  if (sourceHandle == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    std::wstring errorMessage(hlsl_test::FormatToWString(L"Unable to open file '%s', err=%u", path, err).c_str());
    VERIFY_SUCCEEDED(HRESULT_FROM_WIN32(err), errorMessage.c_str());
  }
  return sourceHandle;
}

HANDLE hlsl_test::CreateNewFileForReadWrite(LPCWSTR path) {
  HANDLE sourceHandle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if (sourceHandle == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    std::wstring errorMessage(hlsl_test::FormatToWString(L"Unable to create file '%s', err=%u", path, err).c_str());
    VERIFY_SUCCEEDED(HRESULT_FROM_WIN32(err), errorMessage.c_str());
  }
  return sourceHandle;
}

bool hlsl_test::GetTestParamBool(LPCWSTR name) {
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
  return Unicode::IsStarMatchWide(ParamValue, ParamValue.GetLength(),
    NameValue, NameValue.GetLength());
}

bool hlsl_test::GetTestParamUseWARP(bool defaultVal) {
  WEX::Common::String AdapterValue;
  if (FAILED(WEX::TestExecution::RuntimeParameters::TryGetValue(
    L"Adapter", AdapterValue))) {
    return defaultVal;
  }
  if ((defaultVal && AdapterValue.IsEmpty()) ||
    AdapterValue.CompareNoCase(L"WARP") == 0) {
    return true;
  }
  return false;
}

// namespace functions end

uint16_t ConvertFloat32ToFloat16(float val) {
  union Bits {
    uint32_t u_bits;
    float f_bits;
  };
  static const uint32_t SignMask = 0x8000;
  // Minimum f32 value representable in f16 format without denormalizing
  static const uint32_t Min16in32 = 0x38800000;
  // Maximum f32 value (next to infinity)
  static const uint32_t Max32 = 0x7f7FFFFF;
  // Mask for f32 mantissa
  static const uint32_t Fraction32Mask = 0x007FFFFF;
  // pow(2,24)
  static const uint32_t DenormalRatio = 0x4B800000;
  static const uint32_t NormalDelta = 0x38000000;
  Bits bits;
  bits.f_bits = val;
  uint32_t sign = bits.u_bits & (SignMask << 16);
  Bits Abs;
  Abs.u_bits = bits.u_bits ^ sign;
  bool isLessThanNormal = Abs.f_bits < *(const float*)&Min16in32;
  bool isInfOrNaN = Abs.u_bits > Max32;
  if (isLessThanNormal) {
    // Compute Denormal result
    return (uint16_t)(Abs.f_bits * *(const float*)(&DenormalRatio)) | (uint16_t)(sign >> 16);
  }
  else if (isInfOrNaN) {
    // Compute Inf or Nan result
    uint32_t Fraction = Abs.u_bits & Fraction32Mask;
    uint16_t IsNaN = Fraction == 0 ? 0 : 0xffff;
    return (IsNaN & FLOAT16_BIT_MANTISSA) | FLOAT16_BIT_EXP | (uint16_t)(sign >> 16);
  }
  else {
    // Compute Normal result
    return (uint16_t)((Abs.u_bits - NormalDelta) >> 13) | (uint16_t)(sign >> 16);
  }
}

float ConvertFloat16ToFloat32(uint16_t x) {
  union Bits {
    float f_bits;
    uint32_t u_bits;
  };
  uint32_t Sign = (x & FLOAT16_BIT_SIGN) << 16;
  // nan -> exponent all set and mantisa is non zero
  // +/-inf -> exponent all set and mantissa is zero
  // denorm -> exponent zero and significand nonzero
  uint32_t Abs = (x & 0x7fff);
  uint32_t IsNormal = Abs > FLOAT16_BIGGEST_DENORM;
  uint32_t IsInfOrNaN = Abs > FLOAT16_BIGGEST_NORMAL;
  // Signless Result for normals
  uint32_t DenormRatio = 0x33800000;
  float DenormResult = Abs * (*(float*)&DenormRatio);
  uint32_t AbsShifted = Abs << 13;
  // Signless Result for normals
  uint32_t NormalResult = AbsShifted + 0x38000000;
  // Signless Result for int & nans
  uint32_t InfResult = AbsShifted + 0x70000000;
  Bits bits;
  bits.u_bits = 0;
  if (IsInfOrNaN)
    bits.u_bits |= InfResult;
  else if (IsNormal)
    bits.u_bits |= NormalResult;
  else
    bits.f_bits = DenormResult;
  bits.u_bits |= Sign;
  return bits.f_bits;
}

bool CompareFloatULP(const float& fsrc, const float& fref, int ULPTolerance,
  hlsl::DXIL::Float32DenormMode mode) {
  if (fsrc == fref) {
    return true;
  }
  if (std::isnan(fsrc)) {
    return std::isnan(fref);
  }
  if (mode == hlsl::DXIL::Float32DenormMode::Any) {
    // If denorm expected, output can be sign preserved zero. Otherwise output
    // should pass the regular ulp testing.
    if (isdenorm(fref) && fsrc == 0 && std::signbit(fsrc) == std::signbit(fref))
      return true;
  }
  // For FTZ or Preserve mode, we should get the expected number within
  // ULPTolerance for any operations.
  int diff = *((const DWORD*)&fsrc) - *((const DWORD*)&fref);
  unsigned int uDiff = diff < 0 ? -diff : diff;
  return uDiff <= (unsigned int)ULPTolerance;
}

bool CompareFloatEpsilon(const float& fsrc, const float& fref, float epsilon,
  hlsl::DXIL::Float32DenormMode mode) {
  if (fsrc == fref) {
    return true;
  }
  if (std::isnan(fsrc)) {
    return std::isnan(fref);
  }
  if (mode == hlsl::DXIL::Float32DenormMode::Any) {
    // If denorm expected, output can be sign preserved zero. Otherwise output
    // should pass the regular epsilon testing.
    if (isdenorm(fref) && fsrc == 0 && std::signbit(fsrc) == std::signbit(fref))
      return true;
  }
  // For FTZ or Preserve mode, we should get the expected number within
  // epsilon for any operations.
  return fabsf(fsrc - fref) < epsilon;
}

bool CompareHalfULP(const uint16_t& fsrc, const uint16_t& fref, float ULPTolerance) {
  if (fsrc == fref)
    return true;
  if (isnanFloat16(fsrc))
    return isnanFloat16(fref);
  // 16-bit floating point numbers must preserve denorms
  int diff = fsrc - fref;
  unsigned int uDiff = diff < 0 ? -diff : diff;
  return uDiff <= (unsigned int)ULPTolerance;
}

bool CompareHalfEpsilon(const uint16_t& fsrc, const uint16_t& fref, float epsilon) {
  if (fsrc == fref)
    return true;
  if (isnanFloat16(fsrc))
    return isnanFloat16(fref);
  float src_f32 = ConvertFloat16ToFloat32(fsrc);
  float ref_f32 = ConvertFloat16ToFloat32(fref);
  return std::abs(src_f32 - ref_f32) < epsilon;
}

void ReplaceDisassemblyTextWithoutRegex(const std::vector<std::string>& lookFors,
  const std::vector<std::string>& replacements,
  std::string& disassembly) {
  for (unsigned i = 0; i < lookFors.size(); ++i) {

    bool bOptional = false;

    bool found = false;
    size_t pos = 0;
    LPCSTR pLookFor = lookFors[i].data();
    size_t lookForLen = lookFors[i].size();
    if (pLookFor[0] == '?') {
      bOptional = true;
      pLookFor++;
      lookForLen--;
    }
    if (!pLookFor || !*pLookFor) {
      continue;
    }
    for (;;) {
      pos = disassembly.find(pLookFor, pos);
      if (pos == std::string::npos)
        break;
      found = true; // at least once
      disassembly.replace(pos, lookForLen, replacements[i]);
      pos += replacements[i].size();
    }
    if (!bOptional) {
      if (!found) {
        WEX::Logging::Log::Comment(WEX::Common::String().Format(
          L"String not found: '%S' in text:\r\n%.*S", pLookFor,
          (unsigned)disassembly.size(), disassembly.data()));
      }
      VERIFY_IS_TRUE(found);
    }
  }
}

void AssembleToContainer(dxc::DxcDllSupport& dllSupport, IDxcBlob* pModule,
  IDxcBlob** pContainer) {
  CComPtr<IDxcAssembler> pAssembler;
  CComPtr<IDxcOperationResult> pResult;
  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
  VERIFY_SUCCEEDED(pAssembler->AssembleToContainer(pModule, &pResult));
  CheckOperationSucceeded(pResult, pContainer);
}

void MultiByteStringToBlob(dxc::DxcDllSupport& dllSupport,
  const std::string& val, UINT32 codePage,
  _Outptr_ IDxcBlobEncoding** ppBlob) {
  CComPtr<IDxcLibrary> library;
  IFT(dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
  IFT(library->CreateBlobWithEncodingOnHeapCopy(val.data(), (UINT32)val.size(),
    codePage, ppBlob));
}

void VerifyCompileOK(dxc::DxcDllSupport& dllSupport, LPCSTR pText,
  LPWSTR pTargetProfile, std::vector<LPCWSTR>& args,
  _Outptr_ IDxcBlob** ppResult) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcOperationResult> pResult;
  HRESULT hrCompile;
  *ppResult = nullptr;
  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
  Utf8ToBlob(dllSupport, pText, &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    pTargetProfile, args.data(), (UINT32)args.size(),
    nullptr, 0, nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetStatus(&hrCompile));
  VERIFY_SUCCEEDED(hrCompile);
  VERIFY_SUCCEEDED(pResult->GetResult(ppResult));
}

void VerifyCompileOK(dxc::DxcDllSupport& dllSupport, LPCSTR pText,
  LPWSTR pTargetProfile, LPCWSTR pArgs,
  _Outptr_ IDxcBlob** ppResult) {
  std::vector<std::wstring> argsW;
  std::vector<LPCWSTR> args;
  if (pArgs) {
    wistringstream argsS(pArgs);
    copy(istream_iterator<wstring, wchar_t>(argsS),
      istream_iterator<wstring, wchar_t>(), back_inserter(argsW));
    transform(argsW.begin(), argsW.end(), back_inserter(args),
      [](const wstring& w) { return w.data(); });
  }
  VerifyCompileOK(dllSupport, pText, pTargetProfile, args, ppResult);
}

std::string BlobToUtf8(_In_ IDxcBlob* pBlob) {
  if (!pBlob)
    return std::string();
  CComPtr<IDxcBlobUtf8> pBlobUtf8;
  if (SUCCEEDED(pBlob->QueryInterface(&pBlobUtf8)))
    return std::string(pBlobUtf8->GetStringPointer(),
      pBlobUtf8->GetStringLength());
  CComPtr<IDxcBlobEncoding> pBlobEncoding;
  IFT(pBlob->QueryInterface(&pBlobEncoding));
  // if (FAILED(pBlob->QueryInterface(&pBlobEncoding))) {
  //   // Assume it is already UTF-8
  //   return std::string((const char*)pBlob->GetBufferPointer(),
  //                      pBlob->GetBufferSize());
  // }
  BOOL known;
  UINT32 codePage;
  IFT(pBlobEncoding->GetEncoding(&known, &codePage));
  if (!known) {
    throw std::runtime_error("unknown codepage for blob.");
  }
  std::string result;
  if (codePage == DXC_CP_WIDE) {
    const wchar_t* text = (const wchar_t*)pBlob->GetBufferPointer();
    size_t length = pBlob->GetBufferSize() / 2;
    if (length >= 1 && text[length - 1] == L'\0')
      length -= 1; // Exclude null-terminator
    Unicode::WideToUTF8String(text, length, &result);
    return result;
  }
  else if (codePage == CP_UTF8) {
    const char* text = (const char*)pBlob->GetBufferPointer();
    size_t length = pBlob->GetBufferSize();
    if (length >= 1 && text[length - 1] == '\0')
      length -= 1; // Exclude null-terminator
    result.resize(length);
    memcpy(&result[0], text, length);
    return result;
  }
  else {
    throw std::runtime_error("Unsupported codepage.");
  }
}

std::string DisassembleProgram(dxc::DxcDllSupport& dllSupport,
  IDxcBlob* pProgram) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pDisassembly;
  if (!dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(dllSupport.Initialize());
  }
  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
  VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembly));
  return BlobToUtf8(pDisassembly);
}


#ifdef _WIN32
UINT GetByteSizeForFormat(DXGI_FORMAT value) {
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
#endif