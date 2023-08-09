// NOTE: this is for tools which use static cl::opt and cl::ParseCommandLineOptions to parse options.
// Should not include this when use hlsl::options::ReadDxcOpts, because hlsl::options::ReadDxcOpts supports multithreading.
#pragma once

static llvm::cl::opt<bool> Help("help", llvm::cl::desc("Print help"));
