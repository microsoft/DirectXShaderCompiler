//===- FileTestUtils.cpp ---- Implementation of FileTestUtils -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "FileTestUtils.h"

#include "dxc/Support/HLSLOptions.h"

#include "Dxil2SpvTestOptions.h"
#include "dxil2spv/lib/dxil2spv.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"

namespace clang {
namespace dxil2spv {
namespace utils {

std::string getAbsPathOfInputDataFile(const llvm::StringRef filename) {

  std::string path = clang::dxil2spv::testOptions::inputDataDir;

#ifdef _WIN32
  const char sep = '\\';
  std::replace(path.begin(), path.end(), '/', '\\');
#else
  const char sep = '/';
#endif

  if (path[path.size() - 1] != sep) {
    path = path + sep;
  }
  path += filename;
  return path;
}

bool translateFileWithDxil2Spv(const llvm::StringRef inputFilePath,
                               std::string *generatedSpirv,
                               std::string *errorMessages) {
  bool success = true;

  std::string stdoutStr;
  std::string stderrStr;
  llvm::raw_string_ostream OS(stdoutStr);
  llvm::raw_string_ostream ERR(stderrStr);

  try {
    // Configure filesystem.
    llvm::sys::fs::MSFileSystem *msfPtr;
    HRESULT hr;
    if (!SUCCEEDED(hr = CreateMSFileSystemForDisk(&msfPtr)))
      return DXC_E_GENERAL_INTERNAL_ERROR;
    std::unique_ptr<llvm::sys::fs::MSFileSystem> msf(msfPtr);
    llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    IFTLLVM(pts.error_code());

    // Set up diagnostics.
    CompilerInstance instance;
    auto *diagnosticPrinter =
        new clang::TextDiagnosticPrinter(ERR, new clang::DiagnosticOptions());
    instance.createDiagnostics(diagnosticPrinter, false);
    instance.setOutStream(&OS);
    instance.getCodeGenOpts().MainFileName = inputFilePath;
    instance.getCodeGenOpts().SpirvOptions.targetEnv = "vulkan1.0";

    Translator translator(instance);
    translator.Run();
  } catch (...) {
    success = false;
  }

  OS.flush();
  ERR.flush();

  generatedSpirv->assign(stdoutStr);
  errorMessages->assign(stderrStr);

  return success;
}

} // end namespace utils
} // end namespace dxil2spv
} // end namespace clang
