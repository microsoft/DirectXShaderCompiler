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
  std::stringstream inputSS;
  std::ifstream inputFile;
  inputFile.exceptions(std::ifstream::failbit);

  try {
    inputFile.open(inputFilePath);
    inputSS << inputFile.rdbuf();
  } catch (...) {
    fprintf(
        stderr,
        "Error: Exception occurred while opening/reading the input file %s\n",
        inputFilePath.c_str());
    return false;
  }

  // Close the input file.
  inputFile.close();

  // Effcee skips any input line which doesn't have a CHECK directive, therefore
  // we can pass the entire input to effcee. This way, any warning/error message
  // provided by effcee also reflects the correct line number in the input file.
  checkCommands = inputSS.str();

  const auto runCmdStartPos = checkCommands.find(hlslStartLabel);
  if (runCmdStartPos != std::string::npos) {
    const auto runCmdEndPos = checkCommands.find('\n', runCmdStartPos);
    const auto runCommand = checkCommands.substr(runCmdStartPos, runCmdEndPos);
    if (!utils::processRunCommandArgs(runCommand, &targetProfile,
                                      &entryPoint)) {
      // An error has occured when parsing the Run command.
      return false;
    }
  } else {
    fprintf(stderr, "Error: Missing \"Run:\" command.\n");
    return false;
  }

  // Everything was successful.
  return true;
}

void FileTest::runFileTest(llvm::StringRef filename, bool expectSuccess,
                           bool runSpirvValidation) {
  inputFilePath = utils::getAbsPathOfInputDataFile(filename);

  // Parse the input file.
  ASSERT_TRUE(parseInputFile());

  std::string errorMessages;

  // Feed the HLSL source into the Compiler.
  const bool compileOk = utils::runCompilerWithSpirvGeneration(
      inputFilePath, entryPoint, targetProfile, &generatedBinary,
      &errorMessages);

  effcee::Result result(effcee::Result::Status::Ok);

  if (expectSuccess) {
    ASSERT_TRUE(compileOk);

    // Disassemble the generated SPIR-V binary.
    ASSERT_TRUE(utils::disassembleSpirvBinary(
        generatedBinary, &generatedSpirvAsm, true /* generateHeader */));

    // Run CHECK commands via effcee on disassembly.
    result = effcee::Match(generatedSpirvAsm, checkCommands,
                           effcee::Options().SetInputName(filename.str()));

  } else {
    ASSERT_FALSE(compileOk);

    // Run CHECK commands via effcee on error messages.
    result = effcee::Match(errorMessages, checkCommands,
                           effcee::Options().SetInputName("<error-message>"));
  }

  // Print effcee's error message (if any).
  if (result.status() != effcee::Result::Status::Ok) {
    fprintf(stderr, "%s\n", result.message().c_str());
  }

  // All checks must have passed.
  ASSERT_EQ(result.status(), effcee::Result::Status::Ok);

  // Run SPIR-V validation if requested.
  if (expectSuccess && runSpirvValidation) {
    EXPECT_TRUE(utils::validateSpirvBinary(generatedBinary));
  }
}

} // end namespace spirv
} // end namespace clang
