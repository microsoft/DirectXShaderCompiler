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

#include "llvm/ADT/StringRef.h"
#include "gtest/gtest.h"

namespace clang {
namespace spirv {

class FileTest : public ::testing::Test {
public:
  /// \brief Runs a File Test! (See class description for more info)
  ///
  /// If the compilation is expected to fail, expectedSuccess should be
  /// set to false.
  /// It is important that all generated SPIR-V code is valid. Users of
  /// FileTest may choose not to run the SPIR-V validator (for cases where
  /// a certain feature has not been added to the validator yet).
  void runFileTest(llvm::StringRef path, bool expectSuccess = true,
                   bool runSpirvValidation = true);

private:
  /// \brief Reads in the given input file.
  bool parseInputFile();

  std::string targetProfile;             ///< Target profile (argument of -T)
  std::string entryPoint;                ///< Entry point name (argument of -E)
  std::string restArgs;                  ///< All the other arguments
  std::string inputFilePath;             ///< Path to the input test file
  std::vector<uint32_t> generatedBinary; ///< The generated SPIR-V Binary
  std::string checkCommands;             ///< CHECK commands that verify output
  std::string generatedSpirvAsm;         ///< Disassembled binary (SPIR-V code)
};

} // end namespace spirv
} // end namespace clang

#endif
