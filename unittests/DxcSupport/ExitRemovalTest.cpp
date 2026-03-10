//===- unittests/DxcSupport/ExitRemovalTest.cpp - Exit removal tests ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Tests that verify exit()/abort() calls have been replaced with
// hlsl::Exception throws in shared library code. These tests would fail
// (process termination) if the old exit()/abort() calls were still present.
//
//===----------------------------------------------------------------------===//

#include "dxc/Support/ErrorCodes.h"
#include "dxc/Support/exception.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "gtest/gtest.h"

namespace {

// StackOption auto-removes itself from the command-line registry on
// destruction, matching the pattern used in CommandLineTest.cpp.
template <typename T>
class StackOption : public llvm::cl::opt<T> {
  typedef llvm::cl::opt<T> Base;

public:
  template <class M0t, class M1t>
  StackOption(const M0t &M0, const M1t &M1) : Base(M0, M1) {}

  ~StackOption() override { this->removeArgument(); }
};

// Test that report_fatal_error() throws hlsl::Exception instead of calling
// exit(1). Without the HLSL change to ErrorHandling.cpp, this test would
// terminate the process on non-Windows platforms.
TEST(ExitRemovalTest, ReportFatalErrorThrowsException) {
  EXPECT_THROW(
      { llvm::report_fatal_error("test fatal error"); }, hlsl::Exception);
}

// Test that the thrown exception carries the correct HRESULT error code.
TEST(ExitRemovalTest, ReportFatalErrorHasCorrectErrorCode) {
  try {
    llvm::report_fatal_error("test fatal error");
    FAIL() << "report_fatal_error should have thrown hlsl::Exception";
  } catch (const hlsl::Exception &e) {
    EXPECT_EQ(e.hr, DXC_E_LLVM_FATAL_ERROR);
  }
}

// Test that the thrown exception message contains the error reason.
TEST(ExitRemovalTest, ReportFatalErrorPreservesMessage) {
  try {
    llvm::report_fatal_error("specific error reason");
    FAIL() << "report_fatal_error should have thrown hlsl::Exception";
  } catch (const hlsl::Exception &e) {
    EXPECT_NE(e.msg.find("specific error reason"), std::string::npos)
        << "Exception message should contain the error reason";
  }
}

// Test that llvm_unreachable_internal() throws hlsl::Exception instead of
// calling abort(). Without the HLSL change to ErrorHandling.cpp, this test
// would terminate the process on non-Windows platforms.
TEST(ExitRemovalTest, UnreachableThrowsException) {
  EXPECT_THROW(
      { llvm::llvm_unreachable_internal("test unreachable", __FILE__, __LINE__); },
      hlsl::Exception);
}

// Test that the unreachable exception carries the correct HRESULT error code.
TEST(ExitRemovalTest, UnreachableHasCorrectErrorCode) {
  try {
    llvm::llvm_unreachable_internal("test unreachable", __FILE__, __LINE__);
    FAIL() << "llvm_unreachable_internal should have thrown hlsl::Exception";
  } catch (const hlsl::Exception &e) {
    EXPECT_EQ(e.hr, DXC_E_LLVM_UNREACHABLE);
  }
}

// Test that ParseCommandLineOptions() with a missing required argument throws
// hlsl::Exception (via report_fatal_error) instead of calling exit(1).
// Without the HLSL change to CommandLine.cpp, this test would terminate the
// process on all platforms.
TEST(ExitRemovalTest, ParseCommandLineErrorThrowsException) {
  StackOption<std::string> RequiredOpt("exit-removal-test-required-opt",
                                       llvm::cl::Required);

  const char *argv[] = {"test-prog"};

  try {
    llvm::cl::ParseCommandLineOptions(1, argv);
    FAIL() << "ParseCommandLineOptions should have thrown for missing required "
              "option";
  } catch (const hlsl::Exception &e) {
    EXPECT_EQ(e.hr, DXC_E_LLVM_FATAL_ERROR);
  }
}

} // namespace
