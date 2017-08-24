//===- unittests/SPIRV/WholeFileTestFixture.h - Whole file test Fixture ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_UNITTESTS_SPIRV_WHOLEFILETESTFIXTURE_H
#define LLVM_CLANG_UNITTESTS_SPIRV_WHOLEFILETESTFIXTURE_H

#include "llvm/ADT/StringRef.h"
#include "gtest/gtest.h"

namespace clang {
namespace spirv {

/// \brief The purpose of the this test class is to take in an input file with
/// the following format:
///
///    // Comments...
///    // More comments...
///    // Run: %dxc -T ps_6_0 -E main
///    ...
///    <HLSL code goes here>
///    ...
///    // CHECK-WHOLE-SPIR-V:
///    // ...
///    // <SPIR-V code goes here>
///    // ...
///
/// This file is fully read in as the HLSL source (therefore any non-HLSL must
/// be commented out). It is fed to the DXC compiler with the SPIR-V Generation
/// option. The resulting SPIR-V binary is then fed to the SPIR-V disassembler
/// (via SPIR-V Tools) to get a SPIR-V assembly text. The resulting SPIR-V
/// assembly text is compared to the second part of the input file (after the
/// <CHECK-WHOLE-SPIR-V:> directive). If these match, the test is marked as a
/// PASS, and marked as a FAILED otherwise.
class WholeFileTest : public ::testing::Test {
public:
  /// \brief Runs a WHOLE-FILE-TEST! (See class description for more info)
  /// Returns true if the test passes; false otherwise.
  /// Since SPIR-V headers may change, a test is more robust if the
  /// disassembler does not include the header.
  /// It is also important that all generated SPIR-V code is valid. Users of
  /// WholeFileTest may choose not to run the SPIR-V Validator (for cases where
  /// a certain feature has not been added to the Validator yet).
  void runWholeFileTest(llvm::StringRef path, bool generateHeader = false,
                        bool runSpirvValidation = true);

private:
  /// \brief Reads in the given input file.
  /// Stores the SPIR-V portion of the file into the <expectedSpirvAsm>
  /// member variable. All "//" are also removed from the SPIR-V assembly.
  /// Returns true on success, and false on failure.
  bool parseInputFile();

  std::string targetProfile;             ///< Target profile (argument of -T)
  std::string entryPoint;                ///< Entry point name (argument of -E)
  std::string restArgs;                  ///< All the other arguments
  std::string inputFilePath;             ///< Path to the input test file
  std::vector<uint32_t> generatedBinary; ///< The generated SPIR-V Binary
  std::string expectedSpirvAsm;          ///< Expected SPIR-V parsed from input
  std::string generatedSpirvAsm;         ///< Disassembled binary (SPIR-V code)
};

} // end namespace spirv
} // end namespace clang

#endif