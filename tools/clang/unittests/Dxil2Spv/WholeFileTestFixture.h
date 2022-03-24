//===- WholeFileTestFixture.h - Whole file test Fixture--------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_UNITTESTS_DXIL2SPV_WHOLEFILETESTFIXTURE_H
#define LLVM_CLANG_UNITTESTS_DXIL2SPV_WHOLEFILETESTFIXTURE_H

#include "llvm/ADT/StringRef.h"
#include "gtest/gtest.h"

namespace clang {
namespace dxil2spv {

/// \brief The purpose of the this test class is to take in an input file with
/// the following format:
///
///    ; Comments...
///    ; More comments...
///    ; RUN: %dxil2spv
///    ...
///    <DXIL bitcode goes here>
///    ...
///    ; CHECK-WHOLE-SPIR-V:
///    ; ...
///    ; <SPIR-V code goes here>
///    ; ...
///
/// This file is fully read in as the DXIL source (therefore any non-DXIL must
/// be commented out). It is fed to dxil2spv to translate to SPIR-V. The
/// resulting SPIR-V assembly text is compared to the second part of the input
/// file (after the <CHECK-WHOLE-SPIR-V:> directive). If these match, the test
/// is marked as a PASS, and marked as a FAILED otherwise.
class WholeFileTest : public ::testing::Test {
public:
  /// \brief Runs a WHOLE-FILE-TEST! (See class description for more info)
  /// Returns true if the test passes; false otherwise.
  void runWholeFileTest(llvm::StringRef path);

  WholeFileTest() {}

private:
  /// \brief Reads in the given input file.
  /// Stores the SPIR-V portion of the file into the <expectedSpirvAsm>
  /// member variable.
  /// Returns true on success, and false on failure.
  bool parseInputFile();

  std::string inputFilePath;     ///< Path to the input test file
  std::string expectedSpirvAsm;  ///< Expected SPIR-V parsed from input
  std::string generatedSpirvAsm; ///< Disassembled binary (SPIR-V code)
  std::string expectedErrors;    ///< Expected errors parsed from input
  std::string generatedErrors;   ///< Actual errors from running
};

} // end namespace dxil2spv
} // end namespace clang

#endif
