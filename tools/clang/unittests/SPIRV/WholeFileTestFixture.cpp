//===- unittests/SPIRV/WholeFileTestFixture.cpp - WholeFileTest impl ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <fstream>

#include "FileTestUtils.h"
#include "WholeFileTestFixture.h"

namespace clang {
namespace spirv {

namespace {
const char hlslStartLabel[] = "// Run:";
const char spirvStartLabel[] = "// CHECK-WHOLE-SPIR-V:";
} // namespace

bool WholeFileTest::parseInputFile() {
  bool foundRunCommand = false;
  bool parseSpirv = false;
  std::ostringstream outString;
  std::ifstream inputFile;
  inputFile.exceptions(std::ifstream::failbit);
  try {
    inputFile.open(inputFilePath);
    for (std::string line; std::getline(inputFile, line);) {
      if (line.find(hlslStartLabel) != std::string::npos) {
        foundRunCommand = true;
        if (!utils::processRunCommandArgs(line, &targetProfile, &entryPoint,
                                          &restArgs)) {
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
  } catch (...) {
    if (!inputFile.eof()) {
      fprintf(
          stderr,
          "Error: Exception occurred while opening/reading the input file %s\n",
          inputFilePath.c_str());
      return false;
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

  // Everything was successful.
  return true;
}

void WholeFileTest::runWholeFileTest(llvm::StringRef filename,
                                     bool generateHeader,
                                     bool runSpirvValidation) {
  inputFilePath = utils::getAbsPathOfInputDataFile(filename);

  // Parse the input file.
  ASSERT_TRUE(parseInputFile());

  std::string errorMessages;

  // Feed the HLSL source into the Compiler.
  ASSERT_TRUE(utils::runCompilerWithSpirvGeneration(
      inputFilePath, entryPoint, targetProfile, restArgs, &generatedBinary,
      &errorMessages));

  // Disassemble the generated SPIR-V binary.
  ASSERT_TRUE(utils::disassembleSpirvBinary(generatedBinary, &generatedSpirvAsm,
                                            generateHeader));

  // Compare the expected and the generted SPIR-V code.
  EXPECT_EQ(expectedSpirvAsm, generatedSpirvAsm);

  // Run SPIR-V validation if requested.
  if (runSpirvValidation) {
    EXPECT_TRUE(utils::validateSpirvBinary(generatedBinary));
  }
}

} // end namespace spirv
} // end namespace clang
