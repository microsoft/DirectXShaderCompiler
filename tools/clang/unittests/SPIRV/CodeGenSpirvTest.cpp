//===- unittests/SPIRV/CodeGenSPIRVTest.cpp ---- Run CodeGenSPIRV tests ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "LibTestFixture.h"

#include "gmock/gmock.h"

namespace {
using clang::spirv::LibTest;

using ::testing::ContainsRegex;

// This test is purely for demonstrating how to use `LibTest`, and does not
// test anything specific in DXC itself.
TEST_F(LibTest, SourceCodeWithoutFilePath) {
  const std::string command(R"(// RUN: %dxc -T ps_6_0 -E PSMain -Zi)");
  const std::string code = command + R"(
float4 PSMain(float4 color : COLOR) : SV_TARGET { return color; }
)";
  std::string spirv = compileCodeAndGetSpirvAsm(code);
  EXPECT_THAT(spirv, ContainsRegex("%PSMain = OpFunction"));
}

} // namespace
