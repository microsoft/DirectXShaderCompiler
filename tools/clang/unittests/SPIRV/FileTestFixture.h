//===- FileTestFixute.h ---- Test Fixture for File Check style tests ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_UNITTESTS_SPIRV_FILE_TEST_FIXTURE_H
#define LLVM_CLANG_UNITTESTS_SPIRV_FILE_TEST_FIXTURE_H

#include "spirv-tools/libspirv.h"
#include "llvm/ADT/StringRef.h"
#include "gtest/gtest.h"

namespace clang {
namespace spirv {

class FileTest : public ::testing::Test {
public:
  /// \brief Expected test result to be
  enum class Expect {
    Success,    // Success (with or without warnings) - check disassembly
    Warning,    // Success (with warnings) - check warning message
    Failure,    // Failure (with errors) - check error message
    ValFailure, // Validation failure (with errors) - check error message
  };

  FileTest()
      : targetEnv(SPV_ENV_VULKAN_1_0), beforeHLSLLegalization(false),
        glLayout(false), dxLayout(false), scalarLayout(false) {}

  void setBeforeHLSLLegalization() { beforeHLSLLegalization = true; }

  /// \brief Runs a test with the given input HLSL file.
  ///
  /// The first line of HLSL code must start with "// RUN:" and following DXC
  /// arguments to run the test. Next lines must be proper HLSL code for the
  /// test. It uses file check style output check e.g., "// CHECK: ...".
  void runFileTest(llvm::StringRef path, Expect expect = Expect::Success);

  /// \brief Runs a test with the given HLSL code.
  ///
  /// The first line of code must start with "// RUN:" and following DXC
  /// arguments to run the test. Next lines must be proper HLSL code for the
  /// test. It uses file check style output check e.g., "// CHECK: ...".
  void runCodeTest(llvm::StringRef code, Expect expect = Expect::Success,
                   bool runValidation = true);

private:
  /// \brief Reads in the given input file and parses the command to get
  /// arguments to run DXC.
  bool parseInputFile();
  /// \brief Parses the command and gets arguments to run DXC.
  bool parseCommand();
  /// \brief Checks the compile result. Reports whether the expected compile
  /// result matches the actual result and  whether the expected validation
  /// result matches the actual one or not.
  void checkTestResult(llvm::StringRef filename, const bool compileOk,
                       const std::string &errorMessages, Expect expect,
                       bool runValidation);

  std::string targetProfile;             ///< Target profile (argument of -T)
  std::string entryPoint;                ///< Entry point name (argument of -E)
  std::vector<std::string> restArgs;     ///< All the other arguments
  std::string inputFilePath;             ///< Path to the input test file
  std::vector<uint32_t> generatedBinary; ///< The generated SPIR-V Binary
  std::string checkCommands;             ///< CHECK commands that verify output
  std::string generatedSpirvAsm;         ///< Disassembled binary (SPIR-V code)
  spv_target_env targetEnv;              ///< Environment to validate against
  bool beforeHLSLLegalization;
  bool glLayout;
  bool dxLayout;
  bool scalarLayout;
};

} // end namespace spirv
} // end namespace clang

#endif
