//===- FileTestFixute.h ---- Test Fixture for File Check style tests ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_UNITTESTS_DXIL2SPV_FILE_TEST_FIXTURE_H
#define LLVM_CLANG_UNITTESTS_DXIL2SPV_FILE_TEST_FIXTURE_H

#include "spirv-tools/libspirv.h"
#include "llvm/ADT/StringRef.h"
#include "gtest/gtest.h"

namespace clang {
namespace dxil2spv {

class FileTest : public ::testing::Test {
public:
  // Expected test result to be
  enum class Expect {
    Success, // Success (with or without warnings) - check disassembly
    Failure, // Failure (with errors) - check error message
  };

  FileTest() {}

  // Runs a test with the given input DXIL file.
  //
  // The first line of code must start with "; RUN:". Next lines must be proper
  // DXIL code for the test. It uses file check style output check e.g., ";
  // CHECK: ...".
  void runFileTest(llvm::StringRef path, Expect expect = Expect::Success);

private:
  // Reads in the given input file and parses the command to get
  // arguments to run dxil2spv.
  bool parseInputFile();

  // Parses the command and gets arguments to run dxil2spv.
  bool parseCommand();

  // Checks the compile result. Reports whether the expected compile
  // result matches the actual result and  whether the expected validation
  // result matches the actual one or not.
  void checkTestResult(llvm::StringRef filename, const bool compileOk,
                       const std::string &errorMessages, Expect expect);

  // Path to the input test file
  std::string inputFilePath;

  // CHECK commands that verify output
  std::string checkCommands;

  // Disassembled binary (SPIR-V code)
  std::string generatedSpirvAsm;
};

} // end namespace dxil2spv
} // end namespace clang

#endif
