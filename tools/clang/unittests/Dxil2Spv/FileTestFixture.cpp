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
namespace dxil2spv {

bool FileTest::parseInputFile() {
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
  checkCommands = inputSS.str();
  return parseCommand();
}

bool FileTest::parseCommand() {
  // Effcee skips any input line which doesn't have a CHECK directive, therefore
  // we can pass the entire input to effcee. This way, any warning/error message
  // provided by effcee also reflects the correct line number in the input file.
  const char dxilStartLabel[] = "; RUN:";
  const auto runCmdStartPos = checkCommands.find(dxilStartLabel);
  if (runCmdStartPos == std::string::npos) {
    fprintf(stderr, "Error: Missing \"RUN:\" command.\n");
    return false;
  }

  // Everything was successful.
  return true;
}

void FileTest::runFileTest(llvm::StringRef filename, Expect expect) {
  inputFilePath = utils::getAbsPathOfInputDataFile(filename);

  // Parse the input file.
  ASSERT_TRUE(parseInputFile());

  // Feed DXIL source into the translator.
  std::string errorMessages;
  const bool compileOk = utils::translateFileWithDxil2Spv(
      inputFilePath, &generatedSpirvAsm, &errorMessages);

  checkTestResult(filename, compileOk, errorMessages, expect);
}

void FileTest::checkTestResult(llvm::StringRef filename, const bool compileOk,
                               const std::string &errorMessages,
                               Expect expect) {
  effcee::Result result(effcee::Result::Status::Ok);

  if (expect == Expect::Success) {
    ASSERT_TRUE(compileOk);

    auto options = effcee::Options()
                       .SetChecksName(filename.str())
                       .SetInputName("<codegen>");

    // Run CHECK commands via effcee on disassembly.
    result = effcee::Match(generatedSpirvAsm, checkCommands, options);

    // Print effcee's error message (if any).
    if (result.status() != effcee::Result::Status::Ok) {
      fprintf(stderr, "%s\n", result.message().c_str());
    }

    // All checks must have passed.
    ASSERT_EQ(result.status(), effcee::Result::Status::Ok);
  } else {
    ASSERT_FALSE(compileOk);

    auto options = effcee::Options()
                       .SetChecksName(filename.str())
                       .SetInputName("<message>");

    // Run CHECK commands via effcee on error messages.
    result = effcee::Match(errorMessages, checkCommands, options);

    // Print effcee's error message (if any).
    if (result.status() != effcee::Result::Status::Ok) {
      fprintf(stderr, "%s\n", result.message().c_str());
    }

    // All checks must have passed.
    ASSERT_EQ(result.status(), effcee::Result::Status::Ok);
  }
}

} // end namespace dxil2spv
} // end namespace clang
