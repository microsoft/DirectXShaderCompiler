//===- WholeFileTestFixture.cpp - WholeFileTest impl-----------------------===//
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
namespace dxil2spv {

namespace {
const char dxilStartLabel[] = "; RUN:";
const char spirvStartLabel[] = "; CHECK-WHOLE-SPIR-V:";
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
      if (line.find(dxilStartLabel) != std::string::npos) {
        foundRunCommand = true;
      } else if (line.find(spirvStartLabel) != std::string::npos) {
        // DXIL source has ended.
        // SPIR-V source starts on the next line.
        parseSpirv = true;
      } else if (parseSpirv) {
        // Strip the leading "; " from the SPIR-V assembly (skip 1 characters)
        if (line.size() > 2u) {
          line = line.substr(2);
        }
        if (line[line.size() - 1] == '\r')
          line = line.substr(0, line.size() - 1);
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
    fprintf(stderr, "Error: Missing \"RUN:\" command.\n");
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

void WholeFileTest::runWholeFileTest(llvm::StringRef filename) {
  inputFilePath = utils::getAbsPathOfInputDataFile(filename);

  // Parse the input file.
  ASSERT_TRUE(parseInputFile());

  std::string errorMessages;

  // Feed the HLSL source into the Compiler.
  ASSERT_TRUE(utils::translateFileWithDxil2Spv(
      inputFilePath, &generatedSpirvAsm, &errorMessages));

  // Compare the expected and the generted SPIR-V code.
  EXPECT_EQ(expectedSpirvAsm, generatedSpirvAsm);
}

} // end namespace dxil2spv
} // end namespace clang
