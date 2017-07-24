//===- FileTestFixture.cpp ------------- File Test Fixture Implementation -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "FileTestFixture.h"

#include <fstream>

#include "FileTestUtils.h"
#include "effcee/effcee.h"

namespace clang {
namespace spirv {

bool FileTest::parseInputFile() {
  const char hlslStartLabel[] = "// Run:";
  bool foundRunCommand = false;
  std::ostringstream checkCommandStream;
  std::ifstream inputFile;
  inputFile.exceptions(std::ifstream::failbit);

  try {
    inputFile.open(inputFilePath);
    for (std::string line; std::getline(inputFile, line);) {
      if (line.find(hlslStartLabel) != std::string::npos) {
        foundRunCommand = true;
        if (!utils::processRunCommandArgs(line, &targetProfile, &entryPoint)) {
          // An error has occured when parsing the Run command.
          return false;
        }
      }
      checkCommandStream << line << std::endl;
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

  // Reached the end of the file. Check commands have ended.
  // Store them so they can be passed to effcee.
  checkCommands = checkCommandStream.str();

  // Close the input file.
  inputFile.close();

  // Everything was successful.
  return true;
}

void FileTest::runFileTest(llvm::StringRef filename, bool runSpirvValidation) {
  inputFilePath = utils::getAbsPathOfInputDataFile(filename);

  // Parse the input file.
  ASSERT_TRUE(parseInputFile());

  // Feed the HLSL source into the Compiler.
  ASSERT_TRUE(utils::runCompilerWithSpirvGeneration(
      inputFilePath, entryPoint, targetProfile, &generatedBinary));

  // Disassemble the generated SPIR-V binary.
  ASSERT_TRUE(utils::disassembleSpirvBinary(generatedBinary, &generatedSpirvAsm,
                                            true /* generateHeader */));

  // Run CHECK commands via effcee.
  auto result = effcee::Match(generatedSpirvAsm, checkCommands,
                              effcee::Options().SetInputName(filename.str()));

  // Print effcee's error message (if any).
  if (result.status() != effcee::Result::Status::Ok) {
    fprintf(stderr, "%s\n", result.message().c_str());
  }

  // All checks must have passed.
  ASSERT_EQ(result.status(), effcee::Result::Status::Ok);

  // Run SPIR-V validation if requested.
  if (runSpirvValidation) {
    EXPECT_TRUE(utils::validateSpirvBinary(generatedBinary));
  }
}

} // end namespace spirv
} // end namespace clang
