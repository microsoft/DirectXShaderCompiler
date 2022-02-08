//===- FileTestUtils.h ---- Utilities For Running File Tests --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_UNITTESTS_SPIRV_FILETESTUTILS_H
#define LLVM_CLANG_UNITTESTS_SPIRV_FILETESTUTILS_H

#include <string>
#include <vector>

#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/dxcapi.use.h"
#include "spirv-tools/libspirv.hpp"
#include "llvm/ADT/StringRef.h"

namespace clang {
namespace dxil2spv {
namespace utils {

/// \brief Returns the absolute path to the input file of the test.
/// The input file is expected to be located in the directory given by the
/// testOptions::inputDataDir
std::string getAbsPathOfInputDataFile(const llvm::StringRef filename);

/// \brief Passes the DXIL input file to the dxil2spv translator.
/// Returns the generated SPIR-V code via 'generatedSpirv' argument.
/// Returns true on success, and false on failure. Writes error messages to
/// errorMessages and stderr on failure.
bool translateFileWithDxil2Spv(const llvm::StringRef inputFilePath,
                               std::string *generatedSpirv,
                               std::string *errorMessages);

} // end namespace utils
} // end namespace dxil2spv
} // end namespace clang

#endif
