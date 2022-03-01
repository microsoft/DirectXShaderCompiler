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

#include "clang/SPIRV/SpirvType.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"

#include "spirv-tools/libspirv.hpp"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CodeGenOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

namespace clang {
namespace dxil2spv {

Translator::Translator(CompilerInstance &instance)
    : ci(instance), diagnosticsEngine(ci.getDiagnostics()),
      spirvOptions(ci.getCodeGenOpts().SpirvOptions),
      featureManager(diagnosticsEngine, spirvOptions),
      spvBuilder(spvContext, spirvOptions, featureManager) {}

int Translator::Run() {
  // Read input file to memory buffer.
  std::string filename = ci.getCodeGenOpts().MainFileName;
  auto errorOrInputFile = llvm::MemoryBuffer::getFileOrSTDIN(filename);
  if (!errorOrInputFile) {
    emitError("Error reading %0: %1")
        << filename << errorOrInputFile.getError().message();
    return DXC_E_GENERAL_INTERNAL_ERROR;
  }
  llvm::MemoryBuffer *memoryBuffer = errorOrInputFile.get().release();
  const char *blobContext = memoryBuffer->getBufferStart();
  unsigned blobSize = memoryBuffer->getBufferSize();

  // Parse LLVM module from bitcode.
  llvm::LLVMContext llvmContext;
  std::unique_ptr<llvm::Module> module;
  const hlsl::DxilContainerHeader *blobHeader =
      reinterpret_cast<const hlsl::DxilContainerHeader *>(blobContext);
  if (hlsl::IsValidDxilContainer(blobHeader,
                                 blobHeader->ContainerSizeInBytes)) {

    // Get DXIL program from container.
    const hlsl::DxilPartHeader *partHeader =
        hlsl::GetDxilPartByType(blobHeader, hlsl::DxilFourCC::DFCC_DXIL);
    IFTBOOL(partHeader != nullptr, DXC_E_MISSING_PART);
    const hlsl::DxilProgramHeader *programHeader =
        reinterpret_cast<const hlsl::DxilProgramHeader *>(
            GetDxilPartData(partHeader));

    // Parse DXIL program to module.
    if (IsValidDxilProgramHeader(programHeader, partHeader->PartSize)) {
      std::string diagStr;
      GetDxilProgramBitcode(programHeader, &blobContext, &blobSize);
      module = hlsl::dxilutil::LoadModuleFromBitcode(
          llvm::StringRef(blobContext, blobSize), llvmContext, diagStr);
    }
  }
  // Parse LLVM module from IR.
  else {
    llvm::SMDiagnostic err;
    module = parseIR(memoryBuffer->getMemBufferRef(), err, llvmContext);
  }

  if (module == nullptr) {
    emitError("Could not parse DXIL module");
    return DXC_E_GENERAL_INTERNAL_ERROR;
  }

  // Construct DXIL module.
  hlsl::DxilModule &program = module->GetOrCreateDxilModule();

  const hlsl::ShaderModel *shaderModel = program.GetShaderModel();
  if (shaderModel->GetKind() == hlsl::ShaderModel::Kind::Invalid)
    emitError("Unknown shader model: %0") << shaderModel->GetName();

  // Set shader model kind and HLSL major/minor version.
  spvContext.setCurrentShaderModelKind(shaderModel->GetKind());
  spvContext.setMajorVersion(shaderModel->GetMajor());
  spvContext.setMinorVersion(shaderModel->GetMinor());

  // Set default addressing and memory model for SPIR-V module.
  spvBuilder.setMemoryModel(spv::AddressingModel::Logical,
                            spv::MemoryModel::GLSL450);

  // Add stage variable interface.
  for (auto &elem : program.GetInputSignature().GetElements()) {
    spvBuilder.addStageIOVar(
        spvContext.getPointerType(toSpirvType(elem.get()),
                                  spv::StorageClass::Input),
        spv::StorageClass::Input, elem->GetSemanticName(), false, {});
  }
  for (auto &elem : program.GetOutputSignature().GetElements()) {
    spvBuilder.addStageIOVar(
        spvContext.getPointerType(toSpirvType(elem.get()),
                                  spv::StorageClass::Output),
        spv::StorageClass::Output, elem->GetSemanticName(), false, {});
  }

  // Contsruct the SPIR-V module.
  std::vector<uint32_t> m = spvBuilder.takeModuleForDxilToSpv();

  // Disassemble SPIR-V for output.
  std::string assembly;
  spvtools::SpirvTools spirvTools(SPV_ENV_VULKAN_1_1);
  uint32_t spirvDisOpts = (SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES |
                           SPV_BINARY_TO_TEXT_OPTION_INDENT);

  if (!spirvTools.Disassemble(m, &assembly, spirvDisOpts)) {
    emitError("SPIR-V disassembly failed");
    return DXC_E_GENERAL_INTERNAL_ERROR;
  }

  *ci.getOutStream() << assembly;

  return 0;
}

const spirv::SpirvType *Translator::toSpirvType(hlsl::CompType compType) {
  if (compType.IsFloatTy() || compType.IsSNorm() || compType.IsUNorm())
    return spvContext.getFloatType(compType.GetSizeInBits());
  else if (compType.IsSIntTy())
    return spvContext.getSIntType(compType.GetSizeInBits());
  else if (compType.IsUIntTy())
    return spvContext.getUIntType(compType.GetSizeInBits());

  llvm_unreachable("Unhandled DXIL Component Type");
}

const spirv::SpirvType *
Translator::toSpirvType(hlsl::DxilSignatureElement *elem) {
  uint32_t rowCount = elem->GetRows();
  uint32_t colCount = elem->GetCols();
  const spirv::SpirvType *componentType = toSpirvType(elem->GetCompType());

  if (rowCount == 1 && colCount == 1)
    return componentType;

  const spirv::SpirvType *vecType =
      spvContext.getVectorType(componentType, colCount);

  if (rowCount == 1)
    return vecType;

  return spvContext.getMatrixType(vecType, rowCount);
}

template <unsigned N>
DiagnosticBuilder Translator::emitError(const char (&message)[N]) {
  const auto diagId =
      diagnosticsEngine.getCustomDiagID(DiagnosticsEngine::Error, message);
  return diagnosticsEngine.Report({}, diagId);
}

} // namespace dxil2spv
} // namespace clang
