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
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/SPIRV/ModuleBuilder.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {
namespace {

/// The class responsible to translate Clang frontend types into SPIR-V type
/// instructions.
///
/// SPIR-V type instructions generated during translation will be emitted to
/// the SPIR-V module builder passed into the constructor.
/// Warnings and errors during the translation will be reported to the
/// DiagnosticEngine passed into the constructor.
class TypeTranslator {
public:
  TypeTranslator(spirv::ModuleBuilder &builder, DiagnosticsEngine &diag)
      : theBuilder(builder), diags(diag) {}

  /// \brief Generates the corresponding SPIR-V type for the given Clang
  /// frontend type and returns the type's <result-id>. On failure, reports
  /// the error and returns 0.
  ///
  /// The translation is recursive; all the types that the target type depends
  /// on will be generated.
  uint32_t translateType(QualType type) {
    const auto *typePtr = type.getTypePtr();

    // Primitive types
    if (const auto *builtinType = dyn_cast<BuiltinType>(typePtr)) {
      switch (builtinType->getKind()) {
      case BuiltinType::Void:
        return theBuilder.getVoidType();
      case BuiltinType::Float:
        return theBuilder.getFloatType();
      default:
        emitError("Primitive type '%0' is not supported yet.")
            << builtinType->getTypeClassName();
        return 0;
      }
    }

    // In AST, vector types are TypedefType of TemplateSpecializationType.
    // We handle them via HLSL type inspection functions.
    if (hlsl::IsHLSLVecType(type)) {
      const auto elemType = hlsl::GetHLSLVecElementType(type);
      const auto elemCount = hlsl::GetHLSLVecSize(type);
      return theBuilder.getVecType(translateType(elemType), elemCount);
    }

    // Struct type
    if (const auto *structType = dyn_cast<RecordType>(typePtr)) {
      const auto *decl = structType->getDecl();

      // Collect all fields' types.
      std::vector<uint32_t> fieldTypes;
      for (const auto *field : decl->fields()) {
        fieldTypes.push_back(translateType(field->getType()));
      }

      return theBuilder.getStructType(fieldTypes);
    }

    emitError("Type '%0' is not supported yet.") << type->getTypeClassName();
    return 0;
  }

private:
  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N> DiagnosticBuilder emitError(const char (&message)[N]) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Error, message);
    return diags.Report(diagId);
  }

private:
  spirv::ModuleBuilder &theBuilder;
  DiagnosticsEngine &diags;
};

/// \brief The class containing mappings from Clang frontend Decls to their
/// corresponding SPIR-V <result-id>s.
///
/// All symbols defined in the AST should be "defined" or registered in this
/// class and have their <result-id>s queried from this class. In the process
/// of defining a Decl, the SPIR-V module builder passed into the constructor
/// will be used to generate all SPIR-V instructions required.
///
/// This class acts as a middle layer to handle the mapping between HLSL
/// semantics and Vulkan stage (builtin/input/output) variables. Such mapping
/// is required because of the semantic differences between DirectX and
/// Vulkan and the essence of HLSL as the front-end language for DirectX.
/// A normal variable attached with some semantic will be translated into a
/// single stage variables if it is of non-struct type. If it is of struct
/// type, the fields with attached semantics will need to be translated into
/// stage variables per Vulkan's requirements.
///
/// In the following class, we call a Decl as *remapped* when it is translated
/// into a stage variable; otherwise, we call it as *normal*. Remapped decls
/// include:
/// * FunctionDecl if the return value is attached with a semantic
/// * ParmVarDecl if the parameter is attached with a semantic
/// * FieldDecl if the field is attached with a semantic.
class DeclResultIdMapper {
public:
  DeclResultIdMapper(spv::ExecutionModel stage, spirv::ModuleBuilder &builder,
                     DiagnosticsEngine &diag)
      : shaderStage(stage), theBuilder(builder), typeTranslator(builder, diag) {
  }

  /// \brief Creates the stage variables by parsing the semantics attached to
  /// the given function's return value.
  void createStageVarFromFnReturn(FunctionDecl *funcDecl) {
    // SemanticDecl for the return value is attached to the FunctionDecl.
    createStageVariables(funcDecl, false);
  }

  /// \brief Creates the stage variables by parsing the semantics attached to
  /// the given function's parameter.
  void createStageVarFromFnParam(ParmVarDecl *paramDecl) {
    // TODO: We cannot treat all parameters as stage inputs because of
    // out/input modifiers.
    createStageVariables(paramDecl, true);
  }

  /// \brief Registers a Decl's <result-id> without generating any SPIR-V
  /// instruction.
  void registerDeclResultId(const NamedDecl *symbol, uint32_t resultId) {
    normalDecls[symbol] = resultId;
  }

  /// \brief Returns true if the given <result-id> is for a stage variable.
  bool isStageVariable(uint32_t varId) const {
    return stageVars.count(varId) != 0;
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

  /// \brief Returns all defined stage (builtin/input/ouput) variables in this
  /// mapper.
  std::vector<uint32_t> collectStageVariables() const {
    std::vector<uint32_t> stageVars;

    for (const auto &builtin : stageBuiltins) {
      stageVars.push_back(builtin.first);
    }
    for (const auto &input : stageInputs) {
      stageVars.push_back(input.first);
    }
    for (const auto &output : stageOutputs) {
      stageVars.push_back(output.first);
    }

    return stageVars;
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
  /// \brief Stage variable kind.
  ///
  /// Stage variables include builtin, input, and output variables.
  /// They participate in interface matching in Vulkan pipelines.
  enum class StageVarKind {
    None,
    Arbitary,
    Position,
    Color,
    Target,
    // TODO: other possible kinds
  };

  using StageVarIdSemanticPair = std::pair<uint32_t, std::string>;

  /// Returns the type of the given decl. If the given decl is a FunctionDecl,
  /// returns its result type.
  QualType getFnParamOrRetType(const DeclaratorDecl *decl) const {
    if (const auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
      return funcDecl->getReturnType();
    }
    return decl->getType();
  }

  /// Creates all the stage variables mapped from semantics on the given decl.
  ///
  /// Assumes the decl has semantic attached to itself or to its fields.
  void createStageVariables(const DeclaratorDecl *decl, bool actAsInput) {
    QualType type = getFnParamOrRetType(decl);

    if (type->isVoidType()) {
      // No stage variables will be created for void type.
      return;
    }

    const std::string semantic = getStageVarSemantic(decl);
    if (!semantic.empty()) {
      // Found semantic attached directly to this Decl. This means we need to
      // map this decl to a single stage variable.
      const uint32_t typeId = typeTranslator.translateType(type);
      const auto kind = getStageVarKind(semantic);

      if (actAsInput) {
        // Stage (builtin) input variable cases
        const uint32_t varId =
            theBuilder.addStageIOVariable(typeId, spv::StorageClass::Input);

        stageInputs.push_back(std::make_pair(varId, semantic));
        remappedDecls[decl] = varId;
        stageVars.insert(varId);
      } else {
        // Handle output builtin variables first
        if (shaderStage == spv::ExecutionModel::Vertex &&
            kind == StageVarKind::Position) {
          const uint32_t varId = theBuilder.addStageBuiltinVariable(
              typeId, spv::BuiltIn::Position);

          stageBuiltins.push_back(std::make_pair(varId, semantic));
          remappedDecls[decl] = varId;
          stageVars.insert(varId);
        } else {
          // The rest are normal stage output variables
          const uint32_t varId =
              theBuilder.addStageIOVariable(typeId, spv::StorageClass::Output);

          stageOutputs.push_back(std::make_pair(varId, semantic));
          remappedDecls[decl] = varId;
          stageVars.insert(varId);
        }
      }
    } else {
      // If the decl itself doesn't have semantic, it should be a struct having
      // all its fields with semantics.
      assert(type->isStructureType() &&
             "found non-struct decls without semantics");

      const auto *structDecl = cast<RecordType>(type.getTypePtr())->getDecl();

      // Recursively handle all the fields.
      for (const auto *field : structDecl->fields()) {
        createStageVariables(field, actAsInput);
      }
    }
  }

  /// \brief Returns the stage variable's kind for the given semantic.
  StageVarKind getStageVarKind(llvm::StringRef semantic) const {
    return llvm::StringSwitch<StageVarKind>(semantic)
        .Case("", StageVarKind::None)
        .StartsWith("COLOR", StageVarKind::Color)
        .StartsWith("POSITION", StageVarKind::Position)
        .StartsWith("SV_POSITION", StageVarKind::Position)
        .StartsWith("SV_TARGET", StageVarKind::Target)
        .Default(StageVarKind::Arbitary);
  }

  /// \brief Returns the stage variable's semantic for the given Decl.
  std::string getStageVarSemantic(const NamedDecl *decl) const {
    for (auto *annotation : decl->getUnusualAnnotations()) {
      if (auto *semantic = dyn_cast<hlsl::SemanticDecl>(annotation)) {
        return semantic->SemanticName.upper();
      }
    }
    return "";
  }

private:
  const spv::ExecutionModel shaderStage;
  spirv::ModuleBuilder &theBuilder;
  TypeTranslator typeTranslator;

  /// Mapping of all remapped decls to their <result-id>s.
  llvm::DenseMap<const NamedDecl *, uint32_t> remappedDecls;
  /// Mapping of all normal decls to their <result-id>s.
  llvm::DenseMap<const NamedDecl *, uint32_t> normalDecls;
  /// <result-id>s of all defined stage variables.
  ///
  /// We need to keep a separate list here to avoid looping through the
  /// remappedDecls to find whether an <result-id> is for a stage variable.
  llvm::SmallSet<uint32_t, 16> stageVars;

  /// Stage input/oupt/builtin variables and their kinds.
  ///
  /// We need to keep a separate list here in order to sort them at the end
  /// of the module building.
  llvm::SmallVector<StageVarIdSemanticPair, 8> stageInputs;
  llvm::SmallVector<StageVarIdSemanticPair, 8> stageOutputs;
  llvm::SmallVector<StageVarIdSemanticPair, 8> stageBuiltins;
};

/// SPIR-V emitter class. It consumes the HLSL AST and emits SPIR-V words.
///
/// This class only overrides the HandleTranslationUnit() method; Traversing
/// through the AST is done manually instead of using ASTConsumer's harness.
class SPIRVEmitter : public ASTConsumer {
public:
  explicit SPIRVEmitter(CompilerInstance &ci)
      : theCompilerInstance(ci), diags(ci.getDiagnostics()),
        entryFunctionName(ci.getCodeGenOpts().HLSLEntryFunction),
        shaderStage(getSpirvShaderStageFromHlslProfile(
            ci.getCodeGenOpts().HLSLProfile.c_str())),
        theContext(), theBuilder(&theContext),
        declIdMapper(shaderStage, theBuilder, diags),
        typeTranslator(theBuilder, diags), entryFunctionId(0),
        curFunction(nullptr) {}

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

    // A queue of functions we need to translate.
    std::deque<FunctionDecl *> workQueue;

    // The entry function is the seed of the queue.
    for (auto *decl : tu->decls()) {
      if (auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
        if (funcDecl->getName() == entryFunctionName) {
          workQueue.push_back(funcDecl);
        }
      }
    }
    // TODO: enlarge the queue upon seeing a function call.

    // Translate all functions reachable from the entry function.
    while (!workQueue.empty()) {
      doFunctionDecl(workQueue.front());
      workQueue.pop_front();
    }

    theBuilder.addEntryPoint(shaderStage, entryFunctionId, entryFunctionName,
                             declIdMapper.collectStageVariables());

    AddExecutionModeForEntryPoint(shaderStage, entryFunctionId);

    // Add Location decorations to stage input/output variables.
    declIdMapper.finalizeStageIOLocations();

    // Output the constructed module.
    std::vector<uint32_t> m = theBuilder.takeModule();
    theCompilerInstance.getOutStream()->write(
        reinterpret_cast<const char *>(m.data()), m.size() * 4);
  }

  void doDecl(Decl *decl) {
    if (auto *varDecl = dyn_cast<VarDecl>(decl)) {
      doVarDecl(varDecl);
    } else {
      // TODO: Implement handling of other Decl types.
      emitWarning("Decl type '%0' is not supported yet.")
          << std::string(decl->getDeclKindName());
    }
  }

  void doFunctionDecl(FunctionDecl *decl) {
    curFunction = decl;

    const llvm::StringRef funcName = decl->getName();

    if (funcName == entryFunctionName) {
      // First create stage variables for the entry point.
      declIdMapper.createStageVarFromFnReturn(decl);
      for (auto *param : decl->params())
        declIdMapper.createStageVarFromFnParam(param);

      // Construct the function signature.
      const uint32_t voidType = theBuilder.getVoidType();
      const uint32_t funcType = theBuilder.getFunctionType(voidType, {});
      const uint32_t funcId =
          theBuilder.beginFunction(funcType, voidType, funcName);

      if (decl->hasBody()) {
        // The entry basic block.
        const uint32_t entryLabel = theBuilder.createBasicBlock();
        theBuilder.setInsertPoint(entryLabel);

        // Process all statments in the body.
        for (Stmt *stmt : cast<CompoundStmt>(decl->getBody())->body())
          doStmt(stmt);

        // We have processed all Stmts in this function and now in the last
        // basic block. Make sure we have OpReturn if missing.
        if (!theBuilder.isCurrentBasicBlockTerminated()) {
          theBuilder.createReturn();
        }
      }

      theBuilder.endFunction();

      // Record the entry function's <result-id>.
      entryFunctionId = funcId;
    } else {
      emitError("Non-entry functions are not supported yet.");
    }

    curFunction = nullptr;
  }

  void doVarDecl(VarDecl *decl) {
    if (decl->isLocalVarDecl()) {
      const uint32_t ptrType = theBuilder.getPointerType(
          typeTranslator.translateType(decl->getType()),
          spv::StorageClass::Function);
      const uint32_t varId = theBuilder.addFnVariable(ptrType);
      declIdMapper.registerDeclResultId(decl, varId);
    } else {
      // TODO: handle global variables
      emitError("Global variables are not supported yet.");
    }
  }

  void doStmt(Stmt *stmt) {
    if (auto *retStmt = dyn_cast<ReturnStmt>(stmt)) {
      doReturnStmt(retStmt);
    } else if (auto *declStmt = dyn_cast<DeclStmt>(stmt)) {
      for (auto *decl : declStmt->decls()) {
        doDecl(decl);
      }
    } else if (auto *binOp = dyn_cast<BinaryOperator>(stmt)) {
      const auto opcode = binOp->getOpcode();
      const uint32_t lhs = doExpr(binOp->getLHS());
      const uint32_t rhs = doExpr(binOp->getRHS());

      doBinaryOperator(opcode, lhs, rhs);
    } else {
      emitError("Stmt '%0' is not supported yet.") << stmt->getStmtClassName();
    }
    // TODO: handle other statements
  }

  void doBinaryOperator(BinaryOperatorKind opcode, uint32_t lhs, uint32_t rhs) {
    if (opcode == BO_Assign) {
      theBuilder.createStore(lhs, rhs);
    } else {
      emitError("BinaryOperator '%0' is not supported yet.") << opcode;
    }
  }

  void doReturnStmt(ReturnStmt *stmt) {
    // First get the <result-id> of the value we want to return.
    const uint32_t retValue = doExpr(stmt->getRetValue());

    if (curFunction->getName() != entryFunctionName) {
      theBuilder.createReturnValue(retValue);
      return;
    }

    // SPIR-V requires the signature of entry functions to be void(), while
    // in HLSL we can have non-void parameter and return types for entry points.
    // So we should treat the ReturnStmt in entry functions specially.
    //
    // We need to walk through the return type, and for each subtype attached
    // with semantics, write out the value to the corresponding stage variable
    // mapped to the semantic.
    const uint32_t stageVarId =
        declIdMapper.getRemappedDeclResultId(curFunction);

    if (stageVarId) {
      // The return value is mapped to a single stage variable. We just need
      // to store the value into the stage variable instead.
      theBuilder.createStore(stageVarId, retValue);
      theBuilder.createReturn();
      return;
    }

    QualType retType = stmt->getRetValue()->getType();

    if (const auto *structType = retType->getAsStructureType()) {
      // We are trying to return a value of struct type. Go through all fields.
      uint32_t fieldIndex = 0;
      for (const auto *field : structType->getDecl()->fields()) {
        // Load the value from the current field.
        const uint32_t valueType =
            typeTranslator.translateType(field->getType());
        // TODO: We may need to change the storage class accordingly.
        const uint32_t ptrType = theBuilder.getPointerType(
            typeTranslator.translateType(field->getType()),
            spv::StorageClass::Function);
        const uint32_t indexId = theBuilder.getInt32Value(fieldIndex++);
        const uint32_t valuePtr =
            theBuilder.createAccessChain(ptrType, retValue, {indexId});
        const uint32_t value = theBuilder.createLoad(valueType, valuePtr);
        // Store it to the corresponding stage variable.
        const uint32_t targetVar = declIdMapper.getDeclResultId(field);
        theBuilder.createStore(targetVar, value);
      }
    } else {
      emitError("Return type '%0' for entry function is not supported yet.")
          << retType->getTypeClassName();
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
      // This place gives us a place to inject the code for handling stage
      // variables. Since using the <result-id> of a stage variable as
      // rvalue means OpLoad it first. For normal values, it is not required.
      if (declIdMapper.isStageVariable(fromValue)) {
        const uint32_t resultType =
            typeTranslator.translateType(castExpr->getType());
        return theBuilder.createLoad(resultType, fromValue);
      }
      return fromValue;
    } else if (auto *memberExpr = dyn_cast<MemberExpr>(expr)) {
      const uint32_t base = doExpr(memberExpr->getBase());
      auto *memberDecl = memberExpr->getMemberDecl();
      if (auto *fieldDecl = dyn_cast<FieldDecl>(memberDecl)) {
        const auto index = theBuilder.getInt32Value(fieldDecl->getFieldIndex());
        const uint32_t fieldType =
            typeTranslator.translateType(fieldDecl->getType());
        const uint32_t ptrType =
            theBuilder.getPointerType(fieldType, spv::StorageClass::Function);
        return theBuilder.createAccessChain(ptrType, base, {index});
      } else {
        emitError("Decl '%0' in MemberExpr is not supported yet.")
            << memberDecl->getDeclKindName();
      }
    }
    emitError("Expr '%0' is not supported yet.") << expr->getStmtClassName();
    // TODO: handle other expressions
    return 0;
  }

private:
  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this consumer.
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

private:
  CompilerInstance &theCompilerInstance;
  DiagnosticsEngine &diags;

  /// Entry function name and shader stage. Both of them are derived from the
  /// command line and should be const.
  const llvm::StringRef entryFunctionName;
  const spv::ExecutionModel shaderStage;

  spirv::SPIRVContext theContext;
  spirv::ModuleBuilder theBuilder;
  DeclResultIdMapper declIdMapper;
  TypeTranslator typeTranslator;

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
