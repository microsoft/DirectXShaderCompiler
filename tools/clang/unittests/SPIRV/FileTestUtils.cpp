//===- FileTestUtils.cpp ---- Implementation of FileTestUtils -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <algorithm>

#include "FileTestUtils.h"
#include "SpirvTestOptions.h"
#include "gtest/gtest.h"

namespace clang {
namespace spirv {
namespace utils {

bool disassembleSpirvBinary(std::vector<uint32_t> &binary,
                            std::string *generatedSpirvAsm,
                            bool generateHeader) {
  spvtools::SpirvTools spirvTools(SPV_ENV_UNIVERSAL_1_0);
  spirvTools.SetMessageConsumer(
      [](spv_message_level_t, const char *, const spv_position_t &,
         const char *message) { fprintf(stdout, "%s\n", message); });
  uint32_t options = SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES;
  if (!generateHeader)
    options |= SPV_BINARY_TO_TEXT_OPTION_NO_HEADER;
  return spirvTools.Disassemble(binary, generatedSpirvAsm, options);
}

bool validateSpirvBinary(std::vector<uint32_t> &binary) {
  spvtools::SpirvTools spirvTools(SPV_ENV_UNIVERSAL_1_0);
  spirvTools.SetMessageConsumer(
      [](spv_message_level_t, const char *, const spv_position_t &,
         const char *message) { fprintf(stdout, "%s\n", message); });
  return spirvTools.Validate(binary);
}

bool processRunCommandArgs(const llvm::StringRef runCommandLine,
                           std::string *targetProfile,
                           std::string *entryPoint) {
  std::istringstream buf(runCommandLine);
  std::istream_iterator<std::string> start(buf), end;
  std::vector<std::string> tokens(start, end);
  if (tokens[1].find("Run") == std::string::npos ||
      tokens[2].find("%dxc") == std::string::npos) {
    fprintf(stderr, "The only supported format is: \"// Run: %%dxc -T "
                    "<profile> -E <entry>\"\n");
    return false;
  }

  for (size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i] == "-T" && i + 1 < tokens.size())
      *targetProfile = tokens[i + 1];
    else if (tokens[i] == "-E" && i + 1 < tokens.size())
      *entryPoint = tokens[i + 1];
  }
  if (targetProfile->empty()) {
    fprintf(stderr, "Error: Missing target profile argument (-T).\n");
    return false;
  }
  if (entryPoint->empty()) {
    fprintf(stderr, "Error: Missing entry point argument (-E).\n");
    return false;
  }
  return true;
}

void convertIDxcBlobToUint32(const CComPtr<IDxcBlob> &blob,
                             std::vector<uint32_t> *binaryWords) {
  size_t num32BitWords = (blob->GetBufferSize() + 3) / 4;
  std::string binaryStr((char *)blob->GetBufferPointer(),
                        blob->GetBufferSize());
  binaryStr.resize(num32BitWords * 4, 0);
  binaryWords->resize(num32BitWords, 0);
  memcpy(binaryWords->data(), binaryStr.data(), binaryStr.size());
}

std::string getAbsPathOfInputDataFile(const llvm::StringRef filename) {

  std::string path = clang::spirv::testOptions::inputDataDir;

#ifdef _WIN32
  const char sep = '\\';
  std::replace(path.begin(), path.end(), '/', '\\');
#else
  const char sep = '/';
#endif

  if (path[path.size() - 1] != sep) {
    path = path + sep;
  }
  path += filename;
  return path;
}

bool runCompilerWithSpirvGeneration(const llvm::StringRef inputFilePath,
                                    const llvm::StringRef entryPoint,
                                    const llvm::StringRef targetProfile,
                                    std::vector<uint32_t> *generatedBinary) {
  std::wstring srcFile(inputFilePath.begin(), inputFilePath.end());
  std::wstring entry(entryPoint.begin(), entryPoint.end());
  std::wstring profile(targetProfile.begin(), targetProfile.end());
  bool success = true;

  try {
    dxc::DxcDllSupport dllSupport;
    IFT(dllSupport.Initialize());

    CComPtr<IDxcLibrary> pLibrary;
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcBlobEncoding> pErrorBuffer;
    CComPtr<IDxcBlob> pCompiledBlob;
    CComPtr<IDxcIncludeHandler> pIncludeHandler;
    HRESULT resultStatus;

    std::vector<LPCWSTR> flags;
    flags.push_back(L"-E");
    flags.push_back(entry.c_str());
    flags.push_back(L"-T");
    flags.push_back(profile.c_str());
    flags.push_back(L"-spirv");

    IFT(dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    IFT(pLibrary->CreateBlobFromFile(srcFile.c_str(), nullptr, &pSource));
    IFT(pLibrary->CreateIncludeHandler(&pIncludeHandler));
    IFT(dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
    IFT(pCompiler->Compile(pSource, srcFile.c_str(), entry.c_str(),
                           profile.c_str(), flags.data(), flags.size(), nullptr,
                           0, pIncludeHandler, &pResult));
    // Get compilation results.
    IFT(pResult->GetStatus(&resultStatus));

    // Get diagnostics string and print warnings and errors to stderr.
    IFT(pResult->GetErrorBuffer(&pErrorBuffer));
    const std::string diagnostics((char *)pErrorBuffer->GetBufferPointer(),
                                  pErrorBuffer->GetBufferSize());
    fprintf(stderr, "%s\n", diagnostics.c_str());

    if (SUCCEEDED(resultStatus)) {
      CComPtr<IDxcBlobEncoding> pStdErr;
      IFT(pResult->GetResult(&pCompiledBlob));
      convertIDxcBlobToUint32(pCompiledBlob, generatedBinary);
      success = true;
    } else {
      success = false;
    }
  } catch (...) {
    // An exception has occured while running the compiler with SPIR-V
    // Generation
    success = false;
  }

  return success;
}

} // end namespace utils
} // end namespace spirv
} // end namespace clang
