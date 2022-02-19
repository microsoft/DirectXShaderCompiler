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

  hlsl::options::StringRefWide filename(inputFilePath);

  std::string stdoutStr;
  std::string stderrStr;
  llvm::raw_string_ostream OS(stdoutStr);
  llvm::raw_string_ostream ERR(stderrStr);

  try {
    dxc::DxcDllSupport dxcSupport;
    IFT(dxcSupport.Initialize());
    CComPtr<IDxcBlobEncoding> blob;
    ReadFileIntoBlob(dxcSupport, filename, &blob);

    // Set up diagnostics.
    CompilerInstance instance;
    auto *diagnosticPrinter =
        new clang::TextDiagnosticPrinter(ERR, new clang::DiagnosticOptions());
    instance.createDiagnostics(diagnosticPrinter, false);
    instance.setOutStream(&OS);

    Translator translator(instance);
    translator.Run(blob);
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
