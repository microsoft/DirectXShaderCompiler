//===- unittests/SPIRV/WholeFileCheck.cpp - WholeFileCheck Implementation -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <fstream>

#include "WholeFileCheck.h"
#include "gtest/gtest.h"

WholeFileTest::WholeFileTest() : spirvTools(SPV_ENV_UNIVERSAL_1_0) {
  spirvTools.SetMessageConsumer(
      [](spv_message_level_t, const char *, const spv_position_t &,
         const char *message) { fprintf(stdout, "%s\n", message); });
}

bool WholeFileTest::processRunCommandArgs(const std::string &runCommandLine) {
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
      targetProfile = tokens[i + 1];
    else if (tokens[i] == "-E" && i + 1 < tokens.size())
      entryPoint = tokens[i + 1];
  }
  if (targetProfile.empty()) {
    fprintf(stderr, "Error: Missing target profile argument (-T).\n");
    return false;
  }
  if (entryPoint.empty()) {
    fprintf(stderr, "Error: Missing entry point argument (-E).\n");
    return false;
  }
  return true;
}

bool WholeFileTest::parseInputFile() {
  bool foundRunCommand = false;
  bool parseSpirv = false;
  std::ostringstream outString;
  std::ifstream inputFile;
  inputFile.exceptions(std::ifstream::failbit);
  try {
    inputFile.open(inputFilePath);
    for (std::string line; !inputFile.eof() && std::getline(inputFile, line);) {
      if (line.find(hlslStartLabel) != std::string::npos) {
        foundRunCommand = true;
        if (!processRunCommandArgs(line)) {
          // An error has occured when parsing the Run command.
          return false;
        }
      } else if (line.find(spirvStartLabel) != std::string::npos) {
        // HLSL source has ended.
        // SPIR-V source starts on the next line.
        parseSpirv = true;
      } else if (parseSpirv) {
        // Strip the leading "//" from the SPIR-V assembly (skip 2 characters)
        if (line.size() > 2u) {
          line = line.substr(2);
        }
        // Skip any leading whitespace
        size_t found = line.find_first_not_of(" \t");
        if (found != std::string::npos) {
          line = line.substr(found);
        }
        outString << line << std::endl;
      }
    }

    if (!foundRunCommand) {
      fprintf(stderr, "Error: Missing \"Run:\" command.\n");
      return false;
    }
    if (!parseSpirv) {
      fprintf(stderr, "Error: Missing \"CHECK-WHOLE-SPIR-V:\" command.\n");
      return false;
    }

    // Reached the end of the file. SPIR-V source has ended. Store it for
    // comparison.
    expectedSpirvAsm = outString.str();

    // Close the input file.
    inputFile.close();
  } catch (...) {
    fprintf(
        stderr,
        "Error: Exception occurred while opening/reading the input file %s\n",
        inputFilePath.c_str());
    return false;
  }

  // Everything was successful.
  return true;
}

bool WholeFileTest::runCompilerWithSpirvGeneration() {
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
    IFT(pResult->GetStatus(&resultStatus));

    if (SUCCEEDED(resultStatus)) {
      CComPtr<IDxcBlobEncoding> pStdErr;
      IFT(pResult->GetResult(&pCompiledBlob));
      convertIDxcBlobToUint32(pCompiledBlob);
      success = true;
    } else {
      IFT(pResult->GetErrorBuffer(&pErrorBuffer));
      fprintf(stderr, "%s\n", (char *)pErrorBuffer->GetBufferPointer());
      success = false;
    }
  } catch (...) {
    // An exception has occured while running the compiler with SPIR-V
    // Generation
    success = false;
  }

  return success;
}

bool WholeFileTest::disassembleSpirvBinary(bool generateHeader) {
  uint32_t options = SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES;
  if (!generateHeader)
    options |= SPV_BINARY_TO_TEXT_OPTION_NO_HEADER;
  return spirvTools.Disassemble(generatedBinary, &generatedSpirvAsm, options);
}

bool WholeFileTest::validateSpirvBinary() {
  return spirvTools.Validate(generatedBinary);
}

void WholeFileTest::convertIDxcBlobToUint32(const CComPtr<IDxcBlob> &blob) {
  size_t num32BitWords = (blob->GetBufferSize() + 3) / 4;
  std::string binaryStr((char *)blob->GetBufferPointer(),
                        blob->GetBufferSize());
  binaryStr.resize(num32BitWords * 4, 0);
  generatedBinary.resize(num32BitWords, 0);
  memcpy(generatedBinary.data(), binaryStr.data(), binaryStr.size());
}

std::string
WholeFileTest::getAbsPathOfInputDataFile(const std::string &filename) {
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

void WholeFileTest::runWholeFileTest(std::string filename, bool generateHeader,
                                     bool runSpirvValidation) {
  inputFilePath = getAbsPathOfInputDataFile(filename);

  // Parse the input file.
  ASSERT_TRUE(parseInputFile());

  // Feed the HLSL source into the Compiler.
  ASSERT_TRUE(runCompilerWithSpirvGeneration());

  // Disassemble the generated SPIR-V binary.
  ASSERT_TRUE(disassembleSpirvBinary(generateHeader));

  // Run SPIR-V validation if requested.
  if (runSpirvValidation) {
    ASSERT_TRUE(validateSpirvBinary());
  }

  // Compare the expected and the generted SPIR-V code.
  EXPECT_EQ(expectedSpirvAsm, generatedSpirvAsm);
}
