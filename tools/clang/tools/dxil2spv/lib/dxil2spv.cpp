///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxil2spv.cpp                                                              //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides wrappers to dxil2spv main function.                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxil2spv.h"

#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilContainerReader.h"
#include "dxc/Support/ErrorCodes.h"
#include "dxc/Support/Global.h"
#include "dxc/dxcapi.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include "spirv-tools/libspirv.hpp"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/SPIRV/SpirvBuilder.h"
#include "clang/SPIRV/SpirvContext.h"

int dxil2spvlib::RunTranslator(CComPtr<IDxcBlobEncoding> blob,
                               llvm::raw_ostream &OS, llvm::raw_ostream &ERR) {
  const char *blobContext =
      reinterpret_cast<const char *>(blob->GetBufferPointer());
  unsigned blobSize = blob->GetBufferSize();

  llvm::LLVMContext context;
  llvm::SMDiagnostic err;
  std::unique_ptr<llvm::MemoryBuffer> memoryBuffer;
  std::unique_ptr<llvm::Module> module;

  // Parse LLVM module from bitcode.
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
  // Parse LLVM module from IR.
  else {
    llvm::StringRef bufStrRef(blobContext, blobSize);
    memoryBuffer = llvm::MemoryBuffer::getMemBufferCopy(bufStrRef);
    module = parseIR(memoryBuffer->getMemBufferRef(), err, context);
  }

  if (module == nullptr) {
    ERR << "Could not parse DXIL module\n";
    return DXC_E_GENERAL_INTERNAL_ERROR;
  }

  // Construct DXIL module.
  hlsl::DxilModule &program = module->GetOrCreateDxilModule();

  const hlsl::ShaderModel *shaderModel = program.GetShaderModel();
  if (shaderModel->GetKind() == hlsl::ShaderModel::Kind::Invalid)
    ERR << "Unknown shader model: " << shaderModel->GetName();

  // Set shader model kind and HLSL major/minor version.
  clang::spirv::SpirvContext spvContext;
  spvContext.setCurrentShaderModelKind(shaderModel->GetKind());
  spvContext.setMajorVersion(shaderModel->GetMajor());
  spvContext.setMinorVersion(shaderModel->GetMinor());

  clang::spirv::SpirvCodeGenOptions spvOpts{};
  // TODO: Allow configuration of targetEnv via options.
  spvOpts.targetEnv = "vulkan1.0";

  // Construct SPIR-V builder with diagnostics
  clang::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagnosticOpts =
      new clang::DiagnosticOptions();
  clang::TextDiagnosticPrinter diagnosticPrinter(ERR, &*diagnosticOpts);
  clang::DiagnosticsEngine diagnosticEngine(
      clang::IntrusiveRefCntPtr<clang::DiagnosticIDs>(
          new clang::DiagnosticIDs()),
      &*diagnosticOpts, &diagnosticPrinter, false);

  clang::spirv::FeatureManager featureMgr(diagnosticEngine, spvOpts);
  clang::spirv::SpirvBuilder spvBuilder(spvContext, spvOpts, featureMgr);

  // Set default addressing and memory model for SPIR-V module.
  spvBuilder.setMemoryModel(spv::AddressingModel::Logical,
                            spv::MemoryModel::GLSL450);

  // Contsruct the SPIR-V module.
  std::vector<uint32_t> m = spvBuilder.takeModuleForDxilToSpv();

  // Disassemble SPIR-V for output.
  std::string assembly;
  spvtools::SpirvTools spirvTools(SPV_ENV_VULKAN_1_1);
  uint32_t spirvDisOpts = (SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES |
                           SPV_BINARY_TO_TEXT_OPTION_INDENT);

  if (!spirvTools.Disassemble(m, &assembly, spirvDisOpts)) {
    ERR << "SPIR-V disassembly failed\n";
    return DXC_E_GENERAL_INTERNAL_ERROR;
  }

  OS << assembly;

  return 0;
}
