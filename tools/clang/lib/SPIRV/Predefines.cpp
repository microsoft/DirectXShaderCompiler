//===-- Predefines.h - Predefines for SPIR-V ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===--------------------------------------------------------------===//
//
//  This file includes predefined struct/class, functions, variables,
//  types, and constants for SPIR-V as constant strings.
//
//===--------------------------------------------------------------===//

#include "clang/SPIRV/Predefines.h"

static const char *kSpirvDefinition = "#define __spirv__ 1\n";

static const char *kRawBufferLoad = R"(
namespace vk {

template<typename T, uint alignment>
T RawBufferLoad(uint64_t address) {
  T output;
  vk::RawBufferLoadInto(output, address, alignment);
  return output;
}

}
)";

namespace clang {
namespace spirv {

void BuildPredefinesForSPIRV(llvm::raw_ostream &Output,
                             bool isTemplateEnabled) {
  Output << kSpirvDefinition;
  if (isTemplateEnabled) {
    Output << kRawBufferLoad;
  }
}

} // end namespace spirv
} // end namespace clang
