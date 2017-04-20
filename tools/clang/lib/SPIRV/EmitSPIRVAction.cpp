//===--- EmitSPIRVAction.cpp - EmitSPIRVAction implementation -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/EmitSPIRVAction.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/SPIRV/ModuleBuilder.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

namespace {
spv::ExecutionModel getSpirvShaderStageFromHlslProfile(const char *profile) {
  // DXIL Models are:
  // Profile (DXIL Model) : HLSL Shader Kind : SPIR-V Shader Kind
  // vs_<version>         : Vertex Shader    : Vertex Shader
  // hs_<version>         : Hull Shader      : Tassellation Control Shader
  // ds_<version>         : Domain Shader    : Tessellation Evaluation Shader
  // gs_<version>         : Geometry Shader  : Geometry Shader
  // ps_<version>         : Pixel Shader     : Fragment Shader
  // cs_<version>         : Compute Shader   : Compute Shader
  switch (profile[0]) {
  case 'v': return spv::ExecutionModel::Vertex;
  case 'h': return spv::ExecutionModel::TessellationControl;
  case 'd': return spv::ExecutionModel::TessellationEvaluation;
  case 'g': return spv::ExecutionModel::Geometry;
  case 'p': return spv::ExecutionModel::Fragment;
  case 'c': return spv::ExecutionModel::GLCompute;
  default:
    assert(false && "Unknown HLSL Profile");
    return spv::ExecutionModel::Fragment;
  }
}

} // namespace

namespace clang {
namespace {

class SPIRVEmitter : public ASTConsumer {
public:
  explicit SPIRVEmitter(CompilerInstance &ci)
      : theCompilerInstance(ci), outStream(*ci.getOutStream()), theContext(),
        theBuilder(&theContext) {}

  void AddRequiredCapabilitiesForExecutionModel(spv::ExecutionModel em) {
    if (em == spv::ExecutionModel::TessellationControl ||
        em == spv::ExecutionModel::TessellationEvaluation) {
      theBuilder.requireCapability(spv::Capability::Tessellation);
      assert(false && "Tasselation Shaders are currently not supported.");
    } else if (em == spv::ExecutionModel::Geometry) {
      theBuilder.requireCapability(spv::Capability::Geometry);
      assert(false && "Geometry Shaders are currently not supported.");
    } else {
      theBuilder.requireCapability(spv::Capability::Shader);
    }
  }

  /// \brief Adds the execution mode for the given entry point based on the
  /// execution model.
  void AddExecutionModeForEntryPoint(spv::ExecutionModel execModel,
                                     uint32_t entryPointId) {
    if (execModel == spv::ExecutionModel::Fragment) {
      // TODO: Implement the logic to determine the proper Execution Mode for
      // fragment shaders. Currently using OriginUpperLeft as default.
      theBuilder.addExecutionMode(entryPointId,
                                  spv::ExecutionMode::OriginUpperLeft, {});
    }
    else {
      // TODO: Implement logic for adding proper execution mode for other shader
      // stages. Silently skipping for now.
    }
  }

  void HandleTranslationUnit(ASTContext &context) override {
    const spv::ExecutionModel em = getSpirvShaderStageFromHlslProfile(
        theCompilerInstance.getCodeGenOpts().HLSLProfile.c_str());
    AddRequiredCapabilitiesForExecutionModel(em);

    // Addressing and memory model are required in a valid SPIR-V module.
    theBuilder.setAddressingModel(spv::AddressingModel::Logical);
    theBuilder.setMemoryModel(spv::MemoryModel::GLSL450);

    // Process all top level Decls.
    for (auto *decl : context.getTranslationUnitDecl()->decls()) {
      doDecl(decl);
    }

    // Output the constructed module.
    std::vector<uint32_t> m = theBuilder.takeModule();
    outStream.write(reinterpret_cast<const char *>(m.data()), m.size() * 4);
  }

  void doDecl(Decl *decl) {
    if (auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
      doFunctionDecl(funcDecl);
    }
    // TODO: provide diagnostics of unimplemented features instead of silently
    // ignoring them here.
  }

  void doFunctionDecl(FunctionDecl *decl) {
    const uint32_t funcType = translateFunctionType(decl);
    const uint32_t retType = translateType(decl->getReturnType());

    const uint32_t funcId = theBuilder.beginFunction(funcType, retType);
    // TODO: handle function parameters
    // TODO: handle function body
    const uint32_t entryLabel = theBuilder.bbCreate();
    theBuilder.bbReturn(entryLabel);
    theBuilder.endFunction();

    // Add an entry point to the module if necessary
    const std::string hlslEntryFn =
        theCompilerInstance.getCodeGenOpts().HLSLEntryFunction;
    if (hlslEntryFn == decl->getNameInfo().getAsString()) {
      const spv::ExecutionModel em = getSpirvShaderStageFromHlslProfile(
          theCompilerInstance.getCodeGenOpts().HLSLProfile.c_str());
      // TODO: Pass correct input/output interfaces to addEntryPoint.
      theBuilder.addEntryPoint(em, funcId, hlslEntryFn, {});

      // OpExecutionMode declares an execution mode for an entry point.
      AddExecutionModeForEntryPoint(em, funcId);
    }
  }
  uint32_t translateFunctionType(FunctionDecl *decl) {
    const uint32_t retType = translateType(decl->getReturnType());
    std::vector<uint32_t> paramTypes;
    for (auto *param : decl->params()) {
      paramTypes.push_back(translateType(param->getType()));
    }
    return theBuilder.getFunctionType(retType, paramTypes);
  }

  uint32_t translateType(QualType type) {
    // In AST, vector types are TypedefType of TemplateSpecializationType,
    // which is nested deeply. So we do fast track check here.
    const auto symbol = type.getAsString();
    if (symbol == "float4") {
      const uint32_t floatType = theBuilder.getFloatType();
      return theBuilder.getVec4Type(floatType);
    } else if (symbol == "float3") {
      const uint32_t floatType = theBuilder.getFloatType();
      return theBuilder.getVec3Type(floatType);
    } else if (symbol == "float2") {
      const uint32_t floatType = theBuilder.getFloatType();
      return theBuilder.getVec2Type(floatType);
    } else if (auto *builtinType = dyn_cast<BuiltinType>(type.getTypePtr())) {
      switch (builtinType->getKind()) {
      case BuiltinType::Void:
        return theBuilder.getVoidType();
      case BuiltinType::Float:
        return theBuilder.getFloatType();
      default:
        // TODO: handle other primitive types
        assert(false && "unhandled builtin type");
        break;
      }
    } else {
      // TODO: handle other types
      assert(false && "unhandled clang type");
    }
    return 0;
  }

private:
  raw_ostream &outStream;
  spirv::SPIRVContext theContext;
  spirv::ModuleBuilder theBuilder;
  CompilerInstance &theCompilerInstance;
};

} // namespace

std::unique_ptr<ASTConsumer>
EmitSPIRVAction::CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {
  return llvm::make_unique<SPIRVEmitter>(CI);
}
} // end namespace clang
