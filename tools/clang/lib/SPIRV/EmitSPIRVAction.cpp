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
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/SPIRV/DeclResultIdMapper.h"
#include "clang/SPIRV/ModuleBuilder.h"
#include "clang/SPIRV/TypeTranslator.h"
#include "llvm/ADT/STLExtras.h"

namespace clang {
namespace spirv {

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
        const uint32_t indexId = theBuilder.getConstantInt32(fieldIndex++);
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
        const auto index =
            theBuilder.getConstantInt32(fieldDecl->getFieldIndex());
        const uint32_t fieldType =
            typeTranslator.translateType(fieldDecl->getType());
        const uint32_t ptrType =
            theBuilder.getPointerType(fieldType, spv::StorageClass::Function);
        return theBuilder.createAccessChain(ptrType, base, {index});
      } else {
        emitError("Decl '%0' in MemberExpr is not supported yet.")
            << memberDecl->getDeclKindName();
      }
    } else if (auto *cxxFunctionalCastExpr =
                   dyn_cast<CXXFunctionalCastExpr>(expr)) {
      // Explicit cast is a NO-OP (e.g. vector<float, 4> -> float4)
      if (cxxFunctionalCastExpr->getCastKind() == CK_NoOp) {
        return doExpr(cxxFunctionalCastExpr->getSubExpr());
      } else {
        emitError("Found unhandled CXXFunctionalCastExpr cast type: %0")
            << cxxFunctionalCastExpr->getCastKindName();
      }
    } else if (auto *initListExpr = dyn_cast<InitListExpr>(expr)) {
      const bool isConstantInitializer = expr->isConstantInitializer(
          theCompilerInstance.getASTContext(), false);
      const uint32_t resultType =
          typeTranslator.translateType(initListExpr->getType());
      std::vector<uint32_t> constituents;
      for (size_t i = 0; i < initListExpr->getNumInits(); ++i) {
        constituents.push_back(doExpr(initListExpr->getInit(i)));
      }
      if (isConstantInitializer) {
        return theBuilder.getConstantComposite(resultType, constituents);
      } else {
        // TODO: use OpCompositeConstruct if it is not a constant initializer
        // list.
        emitError("Non-const initializer lists are currently not supported.");
      }
    } else if (auto *floatingLiteral = dyn_cast<FloatingLiteral>(expr)) {
      // TODO: use floatingLiteral->getType() to also handle float64 cases.
      const float value = floatingLiteral->getValue().convertToFloat();
      return theBuilder.getConstantFloat32(value);
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

  SPIRVContext theContext;
  ModuleBuilder theBuilder;
  DeclResultIdMapper declIdMapper;
  TypeTranslator typeTranslator;

  /// <result-id> for the entry function. Initially it is zero and will be reset
  /// when starting to translate the entry function.
  uint32_t entryFunctionId;
  /// The current function under traversal.
  const FunctionDecl *curFunction;
};

} // end namespace spirv

std::unique_ptr<ASTConsumer>
EmitSPIRVAction::CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {
  return llvm::make_unique<spirv::SPIRVEmitter>(CI);
}
} // end namespace clang
