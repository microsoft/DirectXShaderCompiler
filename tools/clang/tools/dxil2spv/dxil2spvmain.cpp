//===------- dxil2spv.cpp - DXIL to SPIR-V Tool -----------------*- C++ -*-===//
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
//
//  This file provides the entry point for the dxil2spv executable program.
//
// NOTE
// ====
// The dxil2spv translator is under active development and is not yet feature
// complete.
//
// BUILD
// =====
// $ cd <dxc-build-dir>
// $ cmake <dxc-src-dir> -GNinja -C ../cmake/caches/PredefinedParams.cmake
//     -DENABLE_DXIL2SPV=ON
// $ ninja
//
// RUN
// ===
// $ <dxc-build-dir>\bin\dxil2spv <input-file>
//
//   where <input-file> may be either a DXIL bitcode file or DXIL IR.
//
// OUTPUT
// ======
// TODO: The current implementation produces incomplete SPIR-V output.
//===----------------------------------------------------------------------===//

#include "dxc/Support/ErrorCodes.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/WinAdapter.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/dxcapi.h"
#include "lib/dxil2spv.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/raw_ostream.h"

#ifdef _WIN32
int __cdecl wmain(int argc, const wchar_t **argv_) {
#else
int main(int argc, const char **argv_) {
#endif // _WIN32
  // Configure filesystem for llvm stdout and stderr handling.
  if (llvm::sys::fs::SetupPerThreadFileSystem())
    return DXC_E_GENERAL_INTERNAL_ERROR;
  llvm::sys::fs::AutoCleanupPerThreadFileSystem auto_cleanup_fs;
  llvm::sys::fs::MSFileSystem *msfPtr;
  HRESULT hr;
  if (!SUCCEEDED(hr = CreateMSFileSystemForDisk(&msfPtr)))
    return DXC_E_GENERAL_INTERNAL_ERROR;
  std::unique_ptr<llvm::sys::fs::MSFileSystem> msf(msfPtr);
  llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
  llvm::STDStreamCloser stdStreamCloser;

  // Check input arguments.
  if (argc < 2) {
    llvm::errs() << "Required input file argument is missing\n";
    return DXC_E_GENERAL_INTERNAL_ERROR;
  }

  // Setup a compiler instance with diagnostics.
  clang::CompilerInstance instance;
  auto *diagnosticPrinter = new clang::TextDiagnosticPrinter(
      llvm::errs(), new clang::DiagnosticOptions());
  instance.createDiagnostics(diagnosticPrinter, false);
  instance.setOutStream(&llvm::outs());

  // TODO: Allow configuration of targetEnv via options.
  instance.getCodeGenOpts().SpirvOptions.targetEnv = "vulkan1.0";

  // Set input filename.
  const llvm::StringRef inputFilename(argv_[1]);
  instance.getCodeGenOpts().MainFileName = inputFilename;

  // Run translator.
  clang::dxil2spv::Translator translator(instance);
  return translator.Run();
}
