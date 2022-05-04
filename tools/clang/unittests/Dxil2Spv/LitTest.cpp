//===- utils/unittest/Dxil2Spv/LitTest.cpp ---- Run dxil2spv lit tests ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "FileTestUtils.h"
#include "dxc/Test/DxcTestUtils.h"
#include "dxc/Test/WEXAdapter.h"

namespace {

#ifdef _WIN32
class FileTest {
#else
class FileTest : public ::testing::Test {
#endif
public:
  BEGIN_TEST_CLASS(FileTest)
  TEST_CLASS_PROPERTY(L"Parallel", L"true")
  TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  dxc::DxcDllSupport m_dllSupport;
  VersionSupportInfo m_ver;

  void runFileTest(std::string name) {
    std::string fullPath =
        clang::dxil2spv::utils::getAbsPathOfInputDataFile(name);
    FileRunTestResult t =
        FileRunTestResult::RunFromFileCommands(CA2W(fullPath.c_str()));
    if (t.RunResult != 0) {
      WEX::Logging::Log::Error(L"FileTest failed");
      WEX::Logging::Log::Error(CA2W(t.ErrorMessage.c_str(), CP_UTF8));
    }
  }
};

bool FileTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    m_ver.Initialize(m_dllSupport);
  }
  return true;
}

TEST_F(FileTest, PassThruPixelShader) { runFileTest("passthru-ps.ll"); }

TEST_F(FileTest, PassThruVertexShader) { runFileTest("passthru-vs.ll"); }

TEST_F(FileTest, PassThruComputeShader) { runFileTest("passthru-cs.ll"); }

TEST_F(FileTest, StaticVertex) { runFileTest("static-vertex.ll"); }

} // namespace
