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
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

// Command line options for dxil2spv. The general approach is to mirror the
// relevant options from HLSLOptions.td.
static llvm::cl::opt<bool> optHelp("help",
                                   llvm::cl::desc("Display available options"));
static llvm::cl::opt<std::string>
    optInputFilename(llvm::cl::Positional, llvm::cl::desc("<input file>"));
static llvm::cl::opt<std::string>
    optOutputFilename("Fo", llvm::cl::desc("Output object file"),
                      llvm::cl::value_desc("file"));

#ifdef _WIN32
int __cdecl wmain(int argc, const wchar_t **argv_) {
#else
int main(int argc, const char **argv_) {
#endif // _WIN32
  // Configure filesystem for llvm stdout and stderr handling.
  if (llvm::sys::fs::SetupPerThreadFileSystem())
    return EXIT_FAILURE;
  llvm::sys::fs::AutoCleanupPerThreadFileSystem auto_cleanup_fs;
  llvm::sys::fs::MSFileSystem *msfPtr;
  HRESULT hr;
  if (!SUCCEEDED(hr = CreateMSFileSystemForDisk(&msfPtr)))
    return EXIT_FAILURE;
  std::unique_ptr<llvm::sys::fs::MSFileSystem> msf(msfPtr);
  llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
  llvm::STDStreamCloser stdStreamCloser;

  // Check input arguments.
  const char *helpMessage =
      "dxil2spv is a tool that translates DXIL code to SPIR-V.\n\n"
      "WARNING: This tool is a prototype in early development.\n";
  llvm::cl::ParseCommandLineOptions(argc, argv_, helpMessage);
  if (optInputFilename.empty() || optHelp) {
    llvm::cl::PrintHelpMessage();
    return EXIT_SUCCESS;
  }

  // Setup a compiler instance with diagnostics.
  clang::CompilerInstance instance;
  auto *diagnosticPrinter = new clang::TextDiagnosticPrinter(
      llvm::errs(), new clang::DiagnosticOptions());
  instance.createDiagnostics(diagnosticPrinter, false);
  instance.setOutStream(&llvm::outs());

  // TODO: Allow configuration of targetEnv via options.
  instance.getCodeGenOpts().SpirvOptions.targetEnv = "vulkan1.0";

  // Set input and ouptut filenames.
  instance.getCodeGenOpts().MainFileName = optInputFilename;
  instance.getFrontendOpts().OutputFile = optOutputFilename;

  // Run translator.
  clang::dxil2spv::Translator translator(instance);
  translator.Run();

  return instance.getDiagnosticClient().getNumErrors() > 0 ? EXIT_FAILURE
                                                           : EXIT_SUCCESS;
}
