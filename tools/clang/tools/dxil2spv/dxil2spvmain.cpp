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
// TODO: The current implementation parses a DXIL file but does not yet produce
// output.
//===----------------------------------------------------------------------===//

#include "dxc/DXIL/DxilUtil.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilContainerReader.h"
#include "dxc/Support/ErrorCodes.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/dxcapi.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

static dxc::DxcDllSupport dxcSupport;

#ifdef _WIN32
int __cdecl wmain(int argc, const wchar_t **argv_) {
#else
int main(int argc, const char **argv_) {
#endif // _WIN32
  if (argc < 2) {
    fprintf(stderr, "Required input file argument is missing.\n");
    return DXC_E_GENERAL_INTERNAL_ERROR;
  }

  hlsl::options::StringRefUtf16 filename(argv_[1]);

  // Read input file.
  IFT(dxcSupport.Initialize());
  CComPtr<IDxcBlobEncoding> blob;
  ReadFileIntoBlob(dxcSupport, filename, &blob);
  const char *blobContext =
      reinterpret_cast<const char *>(blob->GetBufferPointer());
  unsigned blobSize = blob->GetBufferSize();

  llvm::LLVMContext context;
  llvm::SMDiagnostic err;
  std::unique_ptr<llvm::MemoryBuffer> memoryBuffer;
  std::unique_ptr<llvm::Module> module;

  // Parse DXIL from bitcode.
  hlsl::DxilContainerHeader *pBlobHeader =
      (hlsl::DxilContainerHeader *)blob->GetBufferPointer();
  if (hlsl::IsValidDxilContainer(pBlobHeader,
                                 pBlobHeader->ContainerSizeInBytes)) {

    // Get DXIL program from container.
    const hlsl::DxilPartHeader *pPartHeader =
        hlsl::GetDxilPartByType(pBlobHeader, hlsl::DxilFourCC::DFCC_DXIL);
    IFTBOOL(pPartHeader != nullptr, DXC_E_MISSING_PART);
    const hlsl::DxilProgramHeader *pProgramHeader =
        reinterpret_cast<const hlsl::DxilProgramHeader *>(
            GetDxilPartData(pPartHeader));

    // Parse DXIL program to module.
    if (IsValidDxilProgramHeader(pProgramHeader, pPartHeader->PartSize)) {
      std::string DiagStr;
      GetDxilProgramBitcode(pProgramHeader, &blobContext, &blobSize);
      module = hlsl::dxilutil::LoadModuleFromBitcode(
          llvm::StringRef(blobContext, blobSize), context, DiagStr);
    }
  }
  // Parse DXIL from IR.
  else {
    llvm::StringRef bufStrRef(blobContext, blobSize);
    memoryBuffer = llvm::MemoryBuffer::getMemBufferCopy(bufStrRef);
    module = parseIR(memoryBuffer->getMemBufferRef(), err, context);
  }

  if (module == nullptr) {
    fprintf(stderr, "Could not parse DXIL module.\n");
    return DXC_E_GENERAL_INTERNAL_ERROR;
  }

  return 0;
}
