//===-- SPIRVBuilder.h - SPIR-V builder --*- C++ -*------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVBUILDER_H
#define LLVM_CLANG_SPIRV_SPIRVBUILDER_H

namespace clang {
namespace spirv {

class SPIRVBuilder {
public:
  SPIRVBuilder() : NextID(0) {}

private:
  uint32_t NextID;
};

} // end namespace spirv
} // end namespace clang

#endif