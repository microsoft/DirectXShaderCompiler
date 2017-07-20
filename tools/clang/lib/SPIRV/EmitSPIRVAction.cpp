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
#include "clang/AST/HlslTypes.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/SPIRV/ModuleBuilder.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {
namespace {

/// \brief Generates the corresponding SPIR-V type for the given Clang frontend
/// type and returns the <result-id>.
///
/// The translation is recursive; all the types that the target type depends on
/// will be generated.
uint32_t translateType(QualType type, spirv::ModuleBuilder &theBuilder) {
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

/// \brief The class containing mappings from Clang frontend Decls to their
/// corresponding SPIR-V <result-id>s.
///
/// All symbols defined in the AST should be "defined" or registered in this
/// class and have their <result-id>s queried from this class. In the process
/// of defining a Decl, the SPIR-V module builder passed into the constructor
/// will be used to generate all SPIR-V instructions required.
///
/// This class acts as a middle layer to handle the mapping between HLSL
/// semantics and Vulkan interface (stage builtin/input/output) variables.
/// Such mapping is required because of the semantic differences between DirectX
/// and Vulkan and the essence of HLSL as the front-end language for DirectX.
/// A normal variable attached with some semantic will be translated into a
/// single interface variables if it is of non-struct type. If it is of struct
/// type, the fields with attached semantics will need to be translated into
/// interface variables per Vulkan's requirements.
///
/// In the following class, we call a Decl or symbol as *remapped* when it is
/// translated into an interface variable; otherwise, we call it as *normal*.
class DeclResultIdMapper {
public:
  DeclResultIdMapper(spirv::ModuleBuilder *builder) : theBuilder(*builder) {}

  /// \brief Defines a function return value in this mapper and returns the
  /// <result-id> for the final return type.
  ///
  /// The final return type is the "residual" type after "stripping" all
  /// subtypes with attached semantics. For exmaple, stripping "float4 :
  /// SV_Target" will result in "void", and stripping "struct { float4 :
  /// SV_Target, float4 }" will result in "struct { float4 }".
  ///
  /// Proper SPIR-V instructions will be generated to create the corresponding
  /// interface variable if stripping happens.
  uint32_t defineFnReturn(FunctionDecl *funcDecl) {
    // SemanticDecl for the return value is attached to the FunctionDecl.
    const auto sk = getInterfaceVarSemanticAndKind(funcDecl);
    if (sk.second != InterfaceVariableKind::None) {
      // Found return value with semantic attached. This means we need to map
      // the return value to a single interface variable.
      const uint32_t retTypeId =
          translateType(funcDecl->getReturnType(), theBuilder);
      // TODO: Change to the correct interface variable kind here.
      const uint32_t varId = theBuilder.addStageIOVariable(
          retTypeId, spv::StorageClass::Output, llvm::None);

      stageOutputs.push_back(std::make_pair(varId, sk.first));
      remappedDecls[funcDecl] = varId;
      interfaceVars.insert(varId);

      return theBuilder.getVoidType();
    } else {
      // TODO: We need to handle struct return types here.
      return translateType(funcDecl->getReturnType(), theBuilder);
    }
  }

  /// \brief Defines a function parameter in this mapper and returns the
  /// <result-id> for the final parameter type. Returns 0 if the final type
  /// is void.
  ///
  /// The final parameter type is the "residual" type after "stripping" all
  /// subtypes will attached semantics. For exmaple, stripping "float4 :
  /// SV_Target" will result in "void", and stripping "struct { float4 :
  /// SV_Target, float4 }" will result in "struct { float4 }".
  ///
  /// Proper SPIR-V instructions will be generated to create the corresponding
  /// interface variable if stripping happens.
  uint32_t defineFnParam(ParmVarDecl *paramDecl) {
    const auto sk = getInterfaceVarSemanticAndKind(paramDecl);
    if (sk.second != InterfaceVariableKind::None) {
      // Found parameter with semantic attached. This means we need to map the
      // parameter to a single interface variable.
      const uint32_t paramTypeId =
          translateType(paramDecl->getType(), theBuilder);
      // TODO: Change to the correct interface variable kind here.
      const uint32_t varId = theBuilder.addStageIOVariable(
          paramTypeId, spv::StorageClass::Input, llvm::None);

      stageInputs.push_back(std::make_pair(varId, sk.first));
      remappedDecls[paramDecl] = varId;
      interfaceVars.insert(varId);

      return 0;
    } else {
      // TODO: We need to handle struct parameter types here.
      return translateType(paramDecl->getType(), theBuilder);
    }
  }

  /// \brief Registers a Decl's <result-id> without generating any SPIR-V
  /// instruction.
  void registerDeclResultId(const NamedDecl *symbol, uint32_t resultId) {
    normalDecls[symbol] = resultId;
  }

  /// \brief Returns true if the given <result-id> is for an interface variable.
  bool isInterfaceVariable(uint32_t varId) const {
    return interfaceVars.count(varId) != 0;
  }

  /// \brief Returns the <result-id> for the given Decl.
  uint32_t getDeclResultId(const NamedDecl *decl) const {
    if (const uint32_t id = getRemappedDeclResultId(decl))
      return id;
    if (const uint32_t id = getNormalDeclResultId(decl))
      return id;

    assert(false && "found unregistered Decl in DeclResultIdMapper");
    return 0;
  }

  /// \brief Returns the <result-id> for the given remapped Decl. Returns zero
  /// if it is not a registered remapped Decl.
  uint32_t getRemappedDeclResultId(const NamedDecl *decl) const {
    auto it = remappedDecls.find(decl);
    if (it != remappedDecls.end())
      return it->second;
    return 0;
  }

  /// \brief Returns the <result-id> for the given normal Decl. Returns zero if
  /// it is not a registered normal Decl.
  uint32_t getNormalDeclResultId(const NamedDecl *decl) const {
    auto it = normalDecls.find(decl);
    if (it != normalDecls.end())
      return it->second;
    return 0;
  }

  /// \brief Returns all defined stage input and ouput variables in this mapper.
  std::vector<uint32_t> collectStageIOVariables() {
    std::vector<uint32_t> stageIOVars;

    for (const auto &input : stageInputs) {
      stageIOVars.push_back(input.first);
    }
    for (const auto &output : stageOutputs) {
      stageIOVars.push_back(output.first);
    }

    return stageIOVars;
  }

  /// \brief Decorates all stage input and output variables with proper
  /// location.
  ///
  /// This method will writes the location assignment into the module under
  /// construction.
  void finalizeStageIOLocations() {
    uint32_t nextInputLocation = 0;
    uint32_t nextOutputLocation = 0;

    // TODO: sort the variables according to some criteria first, e.g.,
    // alphabetical order of semantic names.
    for (const auto &input : stageInputs) {
      theBuilder.decorateLocation(input.first, nextInputLocation++);
    }
    for (const auto &output : stageOutputs) {
      theBuilder.decorateLocation(output.first, nextOutputLocation++);
    }
  }

private:
  /// \brief Interface variable kind.
  ///
  /// By interface variable, I mean all stage builtin, input, and output
  /// variables. They participate in interface matching in Vulkan pipelines.
  enum class InterfaceVariableKind {
    None,   ///< Not an interface variable
    Input,  ///< Stage input variable
    Output, ///< Stage output variable
    IO, ///< Interface variable that can act as both stage input or stage output
    // TODO: builtins
  };

  using InterfaceVarIdSemanticPair = std::pair<uint32_t, llvm::StringRef>;

  /// \brief Returns the interface variable's semantic and kind for the given
  /// Decl.
  std::pair<llvm::StringRef, InterfaceVariableKind>
  getInterfaceVarSemanticAndKind(NamedDecl *decl) const {
    for (auto *annotation : decl->getUnusualAnnotations()) {
      if (auto *semantic = dyn_cast<hlsl::SemanticDecl>(annotation)) {
        const llvm::StringRef name = semantic->SemanticName;
        // TODO: We should check the semantic name ends with a number.
        if (name.startswith("SV_TARGET")) {
          return std::make_pair(name, InterfaceVariableKind::Output);
        }
        if (name.startswith("COLOR")) {
          return std::make_pair(name, InterfaceVariableKind::IO);
        }
      }
    }
    return std::make_pair("", InterfaceVariableKind::None);
  }

private:
  spirv::ModuleBuilder &theBuilder;

  /// Mapping of all remapped decls to their <result-id>s.
  llvm::DenseMap<const NamedDecl *, uint32_t> remappedDecls;
  /// Mapping of all normal decls to their <result-id>s.
  llvm::DenseMap<const NamedDecl *, uint32_t> normalDecls;
  /// <result-id>s of all defined interface variables.
  ///
  /// We need to keep a separate list here to avoid looping through the
  /// remappedDecls to find whether an <result-id> is for interface variable.
  llvm::SmallSet<uint32_t, 16> interfaceVars;

  /// Stage input/oupt/builtin variables and their kinds.
  ///
  /// We need to keep a separate list here in order to sort them at the end
  /// of the module building.
  llvm::SmallVector<InterfaceVarIdSemanticPair, 8> stageInputs;
  llvm::SmallVector<InterfaceVarIdSemanticPair, 8> stageOutputs;
  llvm::SmallVector<InterfaceVarIdSemanticPair, 8> stageBuiltins;
};

class SPIRVEmitter : public ASTConsumer {
public:
  explicit SPIRVEmitter(CompilerInstance &ci)
      : theCompilerInstance(ci), diags(ci.getDiagnostics()), theContext(),
        theBuilder(&theContext), declIdMapper(&theBuilder),
        entryFunctionName(ci.getCodeGenOpts().HLSLEntryFunction),
        shaderStage(getSpirvShaderStageFromHlslProfile(
            ci.getCodeGenOpts().HLSLProfile.c_str())),
        entryFunctionId(0), curFunction(nullptr) {}

  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this consumer
  template <unsigned N> DiagnosticBuilder emitError(const char (&message)[N]) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Error, message);
    return diags.Report(diagId);
  }

  /// \brief Wrapper method to create a warning message and report it
  /// in the diagnostic engine associated with this consumer
  template <unsigned N>
  DiagnosticBuilder emitWarning(const char (&message)[N]) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Warning, message);
    return diags.Report(diagId);
  }

  spv::ExecutionModel getSpirvShaderStageFromHlslProfile(const char *profile) {
    assert(profile && "nullptr passed as HLSL profile.");

    // DXIL Models are:
    // Profile (DXIL Model) : HLSL Shader Kind : SPIR-V Shader Stage
    // vs_<version>         : Vertex Shader    : Vertex Shader
    // hs_<version>         : Hull Shader      : Tassellation Control Shader
    // ds_<version>         : Domain Shader    : Tessellation Evaluation Shader
    // gs_<version>         : Geometry Shader  : Geometry Shader
    // ps_<version>         : Pixel Shader     : Fragment Shader
    // cs_<version>         : Compute Shader   : Compute Shader
    switch (profile[0]) {
    case 'v':
      return spv::ExecutionModel::Vertex;
    case 'h':
      return spv::ExecutionModel::TessellationControl;
    case 'd':
      return spv::ExecutionModel::TessellationEvaluation;
    case 'g':
      return spv::ExecutionModel::Geometry;
    case 'p':
      return spv::ExecutionModel::Fragment;
    case 'c':
      return spv::ExecutionModel::GLCompute;
    default:
      emitError("Unknown HLSL Profile: %0") << profile;
      return spv::ExecutionModel::Fragment;
    }
  }

  void AddRequiredCapabilitiesForExecutionModel(spv::ExecutionModel em) {
    if (em == spv::ExecutionModel::TessellationControl ||
        em == spv::ExecutionModel::TessellationEvaluation) {
      theBuilder.requireCapability(spv::Capability::Tessellation);
      emitError("Tasselation shaders are currently not supported.");
    } else if (em == spv::ExecutionModel::Geometry) {
      theBuilder.requireCapability(spv::Capability::Geometry);
      emitError("Geometry shaders are currently not supported.");
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
      emitWarning("Execution mode for fragment shaders is "
                  "currently set to OriginUpperLeft by default.");
    } else {
      emitWarning(
          "Execution mode is currently only defined for fragment shaders.");
      // TODO: Implement logic for adding proper execution mode for other
      // shader stages. Silently skipping for now.
    }
  }

  void HandleTranslationUnit(ASTContext &context) override {
    const spv::ExecutionModel em = getSpirvShaderStageFromHlslProfile(
        theCompilerInstance.getCodeGenOpts().HLSLProfile.c_str());
    AddRequiredCapabilitiesForExecutionModel(em);

    // Addressing and memory model are required in a valid SPIR-V module.
    theBuilder.setAddressingModel(spv::AddressingModel::Logical);
    theBuilder.setMemoryModel(spv::MemoryModel::GLSL450);

    TranslationUnitDecl *tu = context.getTranslationUnitDecl();

    // Process all top level Decls.
    for (auto *decl : tu->decls()) {
      doDecl(decl);
    }

    theBuilder.addEntryPoint(shaderStage, entryFunctionId, entryFunctionName,
                             declIdMapper.collectStageIOVariables());

    AddExecutionModeForEntryPoint(shaderStage, entryFunctionId);

    // Add Location decorations to stage input/output variables.
    declIdMapper.finalizeStageIOLocations();

    // Output the constructed module.
    std::vector<uint32_t> m = theBuilder.takeModule();
    theCompilerInstance.getOutStream()->write(
        reinterpret_cast<const char *>(m.data()), m.size() * 4);
  }

  void doDecl(Decl *decl) {
    if (auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
      doFunctionDecl(funcDecl);
    } else {
      emitWarning("Translation is not implemented for this decl type: %0")
          << std::string(decl->getDeclKindName());
      // TODO: Implement handling of other Decl types.
    }
  }

  void doFunctionDecl(FunctionDecl *decl) {
    curFunction = decl;

    uint32_t retType = declIdMapper.defineFnReturn(decl);

    // Process function parameters. We need to strip parameters mapping to stage
    // builtin/input/output variables and use what's left in the function type.
    std::vector<uint32_t> residualParamTypes;
    std::vector<ParmVarDecl *> residualParams;

    for (auto *param : decl->params()) {
      // Get the "residual" parameter type. If nothing left after stripping,
      // zero will be returned.
      const uint32_t paramType = declIdMapper.defineFnParam(param);
      if (paramType) {
        residualParamTypes.push_back(paramType);
        residualParams.push_back(param);
      }
    }

    const uint32_t funcType =
        theBuilder.getFunctionType(retType, residualParamTypes);
    const std::string funcName = decl->getNameInfo().getAsString();
    const uint32_t funcId =
        theBuilder.beginFunction(funcType, retType, funcName);

    // Register all the "residual" parameters into the mapper.
    for (uint32_t i = 0; i < residualParams.size(); ++i) {
      declIdMapper.registerDeclResultId(
          residualParams[i], theBuilder.addFnParameter(residualParamTypes[i]));
    }

    if (decl->hasBody()) {
      const uint32_t entryLabel = theBuilder.createBasicBlock();
      theBuilder.setInsertPoint(entryLabel);

      // Process all statments in the body.
      for (Stmt *stmt : cast<CompoundStmt>(decl->getBody())->body())
        doStmt(stmt);

      // We have processed all Stmts in this function and now in the last basic
      // block. Make sure we have OpReturn if this is a void(...) function.
      if (retType == theBuilder.getVoidType() &&
          !theBuilder.isCurrentBasicBlockTerminated()) {
        theBuilder.createReturn();
      }

      theBuilder.endFunction();
    }

    // Record the entry function's <result-id>.
    if (entryFunctionName == funcName) {
      entryFunctionId = funcId;
    }

    curFunction = nullptr;
  }

  void doStmt(Stmt *stmt) {
    if (auto *retStmt = dyn_cast<ReturnStmt>(stmt)) {
      doReturnStmt(retStmt);
    }
    // TODO: handle other statements
  }

  void doReturnStmt(ReturnStmt *stmt) {
    const uint32_t retValue = doExpr(stmt->getRetValue());
    const uint32_t interfaceVarId =
        declIdMapper.getRemappedDeclResultId(curFunction);

    if (interfaceVarId) {
      // The return value is mapped to an interface variable. We need to store
      // the value into the interface variable instead.
      theBuilder.createStore(interfaceVarId, retValue);
      theBuilder.createReturn();
    } else {
      theBuilder.createReturnValue(retValue);
    }
  }

  uint32_t doExpr(Expr *expr) {
    if (auto *delRefExpr = dyn_cast<DeclRefExpr>(expr)) {
      // Returns the <result-id> of the referenced Decl.
      const NamedDecl *referredDecl = delRefExpr->getFoundDecl();
      assert(referredDecl && "found non-NamedDecl referenced");
      return declIdMapper.getDeclResultId(referredDecl);
    } else if (auto *castExpr = dyn_cast<ImplicitCastExpr>(expr)) {
      const uint32_t fromValue = doExpr(castExpr->getSubExpr());
      // Using lvalue as rvalue will result in a ImplicitCast in Clang AST.
      // This place gives us a place to inject the code for handling interface
      // variables. Since using the <result-id> of an interface variable as
      // rvalue means OpLoad it first. For normal values, it is not required.
      if (declIdMapper.isInterfaceVariable(fromValue)) {
        const uint32_t resultType =
            translateType(castExpr->getType(), theBuilder);
        return theBuilder.createLoad(resultType, fromValue);
      }
      return fromValue;
    }
    // TODO: handle other expressions
    return 0;
  }

private:
  CompilerInstance &theCompilerInstance;
  DiagnosticsEngine &diags;
  spirv::SPIRVContext theContext;
  spirv::ModuleBuilder theBuilder;
  DeclResultIdMapper declIdMapper;

  /// Entry function name and shader stage. Both of them are derived from the
  /// command line and should be const.
  const llvm::StringRef entryFunctionName;
  const spv::ExecutionModel shaderStage;

  /// <result-id> for the entry function. Initially it is zero and will be reset
  /// when starting to translate the entry function.
  uint32_t entryFunctionId;
  /// The current function under traversal.
  const FunctionDecl *curFunction;
};

} // namespace

std::unique_ptr<ASTConsumer>
EmitSPIRVAction::CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {
  return llvm::make_unique<SPIRVEmitter>(CI);
}
} // end namespace clang
