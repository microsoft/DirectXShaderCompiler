//===- FileTestUtils.cpp ---- Implementation of FileTestUtils -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "FileTestUtils.h"

#include <algorithm>
#include <sstream>

#include "dxc/Support/HLSLOptions.h"

#include "SpirvTestOptions.h"
#include "gtest/gtest.h"

namespace clang {
namespace spirv {
namespace utils {

bool disassembleSpirvBinary(std::vector<uint32_t> &binary,
                            std::string *generatedSpirvAsm,
                            bool generateHeader) {
  spvtools::SpirvTools spirvTools(SPV_ENV_VULKAN_1_1);
  spirvTools.SetMessageConsumer(
      [](spv_message_level_t, const char *, const spv_position_t &,
         const char *message) { fprintf(stdout, "%s\n", message); });
  uint32_t options = SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES;
  if (!generateHeader)
    options |= SPV_BINARY_TO_TEXT_OPTION_NO_HEADER;
  return spirvTools.Disassemble(binary, generatedSpirvAsm, options);
}

bool validateSpirvBinary(spv_target_env env, std::vector<uint32_t> &binary,
                         bool relaxLogicalPointer, bool beforeHlslLegalization,
                         bool glLayout, bool dxLayout, bool scalarLayout,
                         std::string *message) {
  spvtools::ValidatorOptions options;
  options.SetRelaxLogicalPointer(relaxLogicalPointer);
  options.SetBeforeHlslLegalization(beforeHlslLegalization);
  if (dxLayout || scalarLayout) {
    options.SetSkipBlockLayout(true);
  } else if (glLayout) {
    // The default for spirv-val.
  } else {
    options.SetRelaxBlockLayout(true);
  }
  spvtools::SpirvTools spirvTools(env);
  spirvTools.SetMessageConsumer([message](spv_message_level_t, const char *,
                                          const spv_position_t &,
                                          const char *msg) {
    if (message)
      *message = msg;
    else
      fprintf(stdout, "%s\n", msg);
  });
  return spirvTools.Validate(binary.data(), binary.size(), options);
}

bool processRunCommandArgs(const llvm::StringRef runCommandLine,
                           std::string *targetProfile, std::string *entryPoint,
                           std::vector<std::string> *restArgs) {
  std::istringstream buf(runCommandLine);
  std::istream_iterator<std::string> start(buf), end;
  std::vector<std::string> tokens(start, end);
  if (tokens.size() < 3 || tokens[1].find("Run") == std::string::npos ||
      tokens[2].find("%dxc") == std::string::npos) {
    fprintf(stderr, "The only supported format is: \"// Run: %%dxc -T "
                    "<profile> -E <entry>\"\n");
    return false;
  }

  std::ostringstream rest;
  for (uint32_t i = 3; i < tokens.size(); ++i) {
    if (tokens[i] == "-T" && (++i) < tokens.size())
      *targetProfile = tokens[i];
    else if (tokens[i] == "-E" && (++i) < tokens.size())
      *entryPoint = tokens[i];
    else
      restArgs->push_back(tokens[i]);
  }

  if (targetProfile->empty()) {
    fprintf(stderr, "Error: Missing target profile argument (-T).\n");
    return false;
  }
  // lib_6_* profile doesn't need an entryPoint
  if (targetProfile->c_str()[0] != 'l' && entryPoint->empty()) {
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
                                    const std::vector<std::string> &restArgs,
                                    std::vector<uint32_t> *generatedBinary,
                                    std::string *errorMessages) {
  std::wstring srcFile(inputFilePath.begin(), inputFilePath.end());
  std::wstring entry(entryPoint.begin(), entryPoint.end());
  std::wstring profile(targetProfile.begin(), targetProfile.end());

  std::vector<std::wstring> rest;
  for (const auto &arg : restArgs)
    rest.emplace_back(arg.begin(), arg.end());

  bool success = true;

  try {
    dxc::DxcDllSupport dllSupport;
    IFT(dllSupport.Initialize());
    DxcInitThreadMalloc();

    if (hlsl::options::initHlslOptTable())
      throw std::bad_alloc();

    CComPtr<IDxcLibrary> pLibrary;
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcBlobEncoding> pErrorBuffer;
    CComPtr<IDxcBlob> pCompiledBlob;
    CComPtr<IDxcIncludeHandler> pIncludeHandler;
    HRESULT resultStatus;

    bool requires_opt = false;
    for (const auto &arg : rest)
      if (arg == L"-O3" || arg.substr(0, 8) == L"-Oconfig")
        requires_opt = true;

    std::vector<LPCWSTR> flags;
    // lib_6_* profile doesn't need an entryPoint
    if (profile.c_str()[0] != 'l') {
      flags.push_back(L"-E");
      flags.push_back(entry.c_str());
    }
    flags.push_back(L"-T");
    flags.push_back(profile.c_str());
    flags.push_back(L"-spirv");
    // Disable legalization and optimization for testing, unless the caller
    // wants to run a specific optimization recipe (with -Oconfig).
    if (!requires_opt)
      flags.push_back(L"-fcgl");
    // Disable validation. We'll run it manually.
    flags.push_back(L"-Vd");
    for (const auto &arg : rest)
      flags.push_back(arg.c_str());

    IFT(dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    IFT(pLibrary->CreateBlobFromFile(srcFile.c_str(), nullptr, &pSource));
    IFT(pLibrary->CreateIncludeHandler(&pIncludeHandler));
    IFT(dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
    IFT(pCompiler->Compile(pSource, srcFile.c_str(), entry.c_str(),
                           profile.c_str(), flags.data(), flags.size(), nullptr,
                           0, pIncludeHandler, &pResult));

    // Compilation is done. We can clean up the HlslOptTable.
    hlsl::options::cleanupHlslOptTable();

    // Get compilation results.
    IFT(pResult->GetStatus(&resultStatus));

    // Get diagnostics string.
    IFT(pResult->GetErrorBuffer(&pErrorBuffer));
    const std::string diagnostics((char *)pErrorBuffer->GetBufferPointer(),
                                  pErrorBuffer->GetBufferSize());
    *errorMessages = diagnostics;

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

  DxcCleanupThreadMalloc();
  return success;
}

} // end namespace utils
} // end namespace spirv
} // end namespace clang
