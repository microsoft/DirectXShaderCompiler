//===- unittests/SPIRV/CodeGenSPIRVTest.cpp ---- Run CodeGenSPIRV tests ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "WholeFileCheck.h"

TEST_F(WholeFileTest, EmptyVoidMain) {
  // Ideally all generated SPIR-V must be valid, but this currently fails with
  // this error message: "No OpEntryPoint instruction was found...".
  // TODO: change this test such that it does run validation.
  runWholeFileTest("empty-void-main.hlsl2spv",
                   /*generateHeader*/ true,
                   /*runValidation*/ false);
}

TEST_F(WholeFileTest, PassThruPixelShader) {
  runWholeFileTest("passthru-ps.hlsl2spv",
                   /*generateHeader*/ true,
                   /*runValidation*/ false);
}
