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
#include "llvm/ADT/SetVector.h"

namespace clang {
namespace spirv {

namespace {

/// Returns true if the given type is a signed integer or vector of signed
/// integer type.
bool isSintOrVecOfSintType(QualType type) {
  return type->isSignedIntegerType() ||
         (hlsl::IsHLSLVecType(type) &&
          hlsl::GetHLSLVecElementType(type)->isSignedIntegerType());
}

/// Returns true if the given type is an unsigned integer or vector of unsigned
/// integer type.
bool isUintOrVecOfUintType(QualType type) {
  return type->isUnsignedIntegerType() ||
         (hlsl::IsHLSLVecType(type) &&
          hlsl::GetHLSLVecElementType(type)->isUnsignedIntegerType());
}

/// Returns true if the given type is a float or vector of float type.
bool isFloatOrVecOfFloatType(QualType type) {
  return type->isFloatingType() ||
         (hlsl::IsHLSLVecType(type) &&
          hlsl::GetHLSLVecElementType(type)->isFloatingType());
}

} // namespace

/// SPIR-V emitter class. It consumes the HLSL AST and emits SPIR-V words.
///
/// This class only overrides the HandleTranslationUnit() method; Traversing
/// through the AST is done manually instead of using ASTConsumer's harness.
class SPIRVEmitter : public ASTConsumer {
public:
  explicit SPIRVEmitter(CompilerInstance &ci)
      : theCompilerInstance(ci), astContext(ci.getASTContext()),
        diags(ci.getDiagnostics()),
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

    // The entry function is the seed of the queue.
    for (auto *decl : tu->decls()) {
      if (auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
        if (funcDecl->getName() == entryFunctionName) {
          workQueue.insert(funcDecl);
        }
      }
    }
    // TODO: enlarge the queue upon seeing a function call.

    // Translate all functions reachable from the entry function.
    // The queue can grow in the meanwhile; so need to keep evaluating
    // workQueue.size().
    for (uint32_t i = 0; i < workQueue.size(); ++i) {
      doDecl(workQueue[i]);
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

  void doDecl(const Decl *decl) {
    if (const auto *varDecl = dyn_cast<VarDecl>(decl)) {
      doVarDecl(varDecl);
    } else if (const auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
      doFunctionDecl(funcDecl);
    } else {
      // TODO: Implement handling of other Decl types.
      emitWarning("Decl type '%0' is not supported yet.")
          << std::string(decl->getDeclKindName());
    }
  }

  void doFunctionDecl(const FunctionDecl *decl) {
    curFunction = decl;

    const llvm::StringRef funcName = decl->getName();

    uint32_t funcId;

    if (funcName == entryFunctionName) {
      // First create stage variables for the entry point.
      declIdMapper.createStageVarFromFnReturn(decl);
      for (const auto *param : decl->params())
        declIdMapper.createStageVarFromFnParam(param);

      // Construct the function signature.
      const uint32_t voidType = theBuilder.getVoidType();
      const uint32_t funcType = theBuilder.getFunctionType(voidType, {});

      // The entry function surely does not have pre-assigned <result-id> for
      // it like other functions that got added to the work queue following
      // function calls.
      funcId = theBuilder.beginFunction(funcType, voidType, funcName);

      // Record the entry function's <result-id>.
      entryFunctionId = funcId;
    } else {
      const uint32_t retType =
          typeTranslator.translateType(decl->getReturnType());

      // Construct the function signature.
      llvm::SmallVector<uint32_t, 4> paramTypes;
      for (const auto *param : decl->params()) {
        const uint32_t valueType =
            typeTranslator.translateType(param->getType());
        const uint32_t ptrType =
            theBuilder.getPointerType(valueType, spv::StorageClass::Function);
        paramTypes.push_back(ptrType);
      }
      const uint32_t funcType = theBuilder.getFunctionType(retType, paramTypes);

      // Non-entry functions are added to the work queue following function
      // calls. We have already assigned <result-id>s for it when translating
      // its call site. Query it here.
      funcId = declIdMapper.getDeclResultId(decl);
      theBuilder.beginFunction(funcType, retType, funcName, funcId);

      // Create all parameters.
      for (uint32_t i = 0; i < decl->getNumParams(); ++i) {
        const ParmVarDecl *paramDecl = decl->getParamDecl(i);
        const uint32_t paramId =
            theBuilder.addFnParameter(paramTypes[i], paramDecl->getName());
        declIdMapper.registerDeclResultId(paramDecl, paramId);
      }
    }

    if (decl->hasBody()) {
      // The entry basic block.
      const uint32_t entryLabel = theBuilder.createBasicBlock("bb.entry");
      theBuilder.setInsertPoint(entryLabel);

      // Process all statments in the body.
      doStmt(decl->getBody());

      // We have processed all Stmts in this function and now in the last
      // basic block. Make sure we have OpReturn if missing.
      if (!theBuilder.isCurrentBasicBlockTerminated()) {
        theBuilder.createReturn();
      }
    }

    theBuilder.endFunction();

    curFunction = nullptr;
  }

  void doVarDecl(const VarDecl *decl) {
    if (decl->isLocalVarDecl()) {
      const uint32_t ptrType = theBuilder.getPointerType(
          typeTranslator.translateType(decl->getType()),
          spv::StorageClass::Function);

      // Handle initializer. SPIR-V requires that "initializer must be an <id>
      // from a constant instruction or a global (module scope) OpVariable
      // instruction."
      llvm::Optional<uint32_t> constInitializer = llvm::None;
      uint32_t varInitializer = 0;
      if (decl->hasInit()) {
        const Expr *declInit = decl->getInit();

        // First try to evaluate the initializer as a constant expression
        Expr::EvalResult evalResult;
        if (declInit->EvaluateAsRValue(evalResult, astContext) &&
            !evalResult.HasSideEffects) {
          constInitializer = llvm::Optional<uint32_t>(
              translateAPValue(evalResult.Val, decl->getType()));
        }

        // If we cannot evaluate the initializer as a constant expression, we'll
        // need use OpStore to write the initializer to the variable.
        if (!constInitializer.hasValue()) {
          varInitializer = doExpr(declInit);
        }
      }

      const uint32_t varId =
          theBuilder.addFnVariable(ptrType, decl->getName(), constInitializer);
      declIdMapper.registerDeclResultId(decl, varId);

      if (varInitializer) {
        theBuilder.createStore(varId, varInitializer);
      }
    } else {
      // TODO: handle global variables
      emitError("Global variables are not supported yet.");
    }
  }

  void doStmt(const Stmt *stmt) {
    if (const auto *compoundStmt = dyn_cast<CompoundStmt>(stmt)) {
      for (auto *st : compoundStmt->body())
        doStmt(st);
    } else if (const auto *retStmt = dyn_cast<ReturnStmt>(stmt)) {
      doReturnStmt(retStmt);
    } else if (const auto *declStmt = dyn_cast<DeclStmt>(stmt)) {
      for (auto *decl : declStmt->decls()) {
        doDecl(decl);
      }
    } else if (const auto *ifStmt = dyn_cast<IfStmt>(stmt)) {
      doIfStmt(ifStmt);
    } else if (const auto *forStmt = dyn_cast<ForStmt>(stmt)) {
      doForStmt(forStmt);
    } else if (const auto *nullStmt = dyn_cast<NullStmt>(stmt)) {
      // For the null statement ";". We don't need to do anything.
    } else if (const auto *expr = dyn_cast<Expr>(stmt)) {
      // All cases for expressions used as statements
      doExpr(expr);
    } else {
      emitError("Stmt '%0' is not supported yet.") << stmt->getStmtClassName();
    }
  }

  void doReturnStmt(const ReturnStmt *stmt) {
    // For normal functions, just return in the normal way.
    if (curFunction->getName() != entryFunctionName) {
      theBuilder.createReturnValue(doExpr(stmt->getRetValue()));
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
      theBuilder.createStore(stageVarId, doExpr(stmt->getRetValue()));
      theBuilder.createReturn();
      return;
    }

    QualType retType = stmt->getRetValue()->getType();

    if (const auto *structType = retType->getAsStructureType()) {
      // We are trying to return a value of struct type.

      // First get the return value. Clang AST will use an LValueToRValue cast
      // for returning a struct variable. We need to ignore the cast to avoid
      // creating OpLoad instruction since we need the pointer to the variable
      // for creating access chain later.
      const uint32_t retValue =
          doExpr(stmt->getRetValue()->IgnoreParenLValueCasts());

      // Then go through all fields.
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

  void doIfStmt(const IfStmt *ifStmt) {
    // if statements are composed of:
    //   if (<check>) { <then> } else { <else> }
    //
    // To translate if statements, we'll need to emit the <check> expressions
    // in the current basic block, and then create separate basic blocks for
    // <then> and <else>. Additionally, we'll need a <merge> block as per
    // SPIR-V's structured control flow requirements. Depending whether there
    // exists the else branch, the final CFG should normally be like the
    // following. Exceptions will occur with non-local exits like loop breaks
    // or early returns.
    //             +-------+                        +-------+
    //             | check |                        | check |
    //             +-------+                        +-------+
    //                 |                                |
    //         +-------+-------+                  +-----+-----+
    //         | true          | false            | true      | false
    //         v               v         or       v           |
    //     +------+         +------+           +------+       |
    //     | then |         | else |           | then |       |
    //     +------+         +------+           +------+       |
    //         |               |                  |           v
    //         |   +-------+   |                  |     +-------+
    //         +-> | merge | <-+                  +---> | merge |
    //             +-------+                            +-------+

    // First emit the instruction for evaluating the condition.
    const uint32_t condition = doExpr(ifStmt->getCond());

    // Then we need to emit the instruction for the conditional branch.
    // We'll need the <label-id> for the then/else/merge block to do so.
    const bool hasElse = ifStmt->getElse() != nullptr;
    const uint32_t thenBB = theBuilder.createBasicBlock("if.true");
    const uint32_t mergeBB = theBuilder.createBasicBlock("if.merge");
    const uint32_t elseBB =
        hasElse ? theBuilder.createBasicBlock("if.false") : mergeBB;

    // Create the branch instruction. This will end the current basic block.
    theBuilder.createConditionalBranch(condition, thenBB, elseBB, mergeBB);
    theBuilder.addSuccessor(thenBB);
    theBuilder.addSuccessor(elseBB);
    // The current basic block has the OpSelectionMerge instruction. We need
    // to record its merge target.
    theBuilder.setMergeTarget(mergeBB);

    // Handle the then branch
    theBuilder.setInsertPoint(thenBB);
    doStmt(ifStmt->getThen());
    if (!theBuilder.isCurrentBasicBlockTerminated())
      theBuilder.createBranch(mergeBB);
    theBuilder.addSuccessor(mergeBB);

    // Handle the else branch (if exists)
    if (hasElse) {
      theBuilder.setInsertPoint(elseBB);
      doStmt(ifStmt->getElse());
      if (!theBuilder.isCurrentBasicBlockTerminated())
        theBuilder.createBranch(mergeBB);
      theBuilder.addSuccessor(mergeBB);
    }

    // From now on, we'll emit instructions into the merge block.
    theBuilder.setInsertPoint(mergeBB);
  }

  void doForStmt(const ForStmt *forStmt) {
    // for loops are composed of:
    //   for (<init>; <check>; <continue>) <body>
    //
    // To translate a for loop, we'll need to emit all <init> statements
    // in the current basic block, and then have separate basic blocks for
    // <check>, <continue>, and <body>. Besides, since SPIR-V requires
    // structured control flow, we need two more basic blocks, <header>
    // and <merge>. <header> is the block before control flow diverges,
    // while <merge> is the block where control flow subsequently converges.
    // The <check> block can take the responsibility of the <header> block.
    // The final CFG should normally be like the following. Exceptions will
    // occur with non-local exits like loop breaks or early returns.
    //             +--------+
    //             |  init  |
    //             +--------+
    //                 |
    //                 v
    //            +----------+
    //            |  header  | <---------------+
    //            | (check)  |                 |
    //            +----------+                 |
    //                 |                       |
    //         +-------+-------+               |
    //         | false         | true          |
    //         |               v               |
    //         |            +------+     +----------+
    //         |            | body | --> | continue |
    //         v            +------+     +----------+
    //     +-------+
    //     | merge |
    //     +-------+
    //
    // For more details, see "2.11. Structured Control Flow" in the SPIR-V spec.

    // Create basic blocks
    const uint32_t checkBB = theBuilder.createBasicBlock("for.check");
    const uint32_t bodyBB = theBuilder.createBasicBlock("for.body");
    const uint32_t continueBB = theBuilder.createBasicBlock("for.continue");
    const uint32_t mergeBB = theBuilder.createBasicBlock("for.merge");

    // Process the <init> block
    if (const Stmt *initStmt = forStmt->getInit()) {
      doStmt(initStmt);
    }
    theBuilder.createBranch(checkBB);
    theBuilder.addSuccessor(checkBB);

    // Process the <check> block
    theBuilder.setInsertPoint(checkBB);
    uint32_t condition;
    if (const Expr *check = forStmt->getCond()) {
      condition = doExpr(check);
    } else {
      condition = theBuilder.getConstantBool(true);
    }
    theBuilder.createConditionalBranch(condition, bodyBB,
                                       /*false branch*/ mergeBB,
                                       /*merge*/ mergeBB, continueBB);
    theBuilder.addSuccessor(bodyBB);
    theBuilder.addSuccessor(mergeBB);
    // The current basic block has OpLoopMerge instruction. We need to set its
    // continue and merge target.
    theBuilder.setContinueTarget(continueBB);
    theBuilder.setMergeTarget(mergeBB);

    // Process the <body> block
    theBuilder.setInsertPoint(bodyBB);
    if (const Stmt *body = forStmt->getBody()) {
      doStmt(body);
    }
    theBuilder.createBranch(continueBB);
    theBuilder.addSuccessor(continueBB);

    // Process the <continue> block
    theBuilder.setInsertPoint(continueBB);
    if (const Expr *cont = forStmt->getInc()) {
      doExpr(cont);
    }
    theBuilder.createBranch(checkBB); // <continue> should jump back to header
    theBuilder.addSuccessor(checkBB);

    // Set insertion point to the <merge> block for subsequent statements
    theBuilder.setInsertPoint(mergeBB);
  }

  uint32_t doExpr(const Expr *expr) {
    if (const auto *delRefExpr = dyn_cast<DeclRefExpr>(expr)) {
      // Returns the <result-id> of the referenced Decl.
      const NamedDecl *referredDecl = delRefExpr->getFoundDecl();
      assert(referredDecl && "found non-NamedDecl referenced");
      return declIdMapper.getDeclResultId(referredDecl);
    }

    if (const auto *parenExpr = dyn_cast<ParenExpr>(expr)) {
      // Just need to return what's inside the parentheses.
      return doExpr(parenExpr->getSubExpr());
    }

    if (const auto *memberExpr = dyn_cast<MemberExpr>(expr)) {
      const uint32_t base = doExpr(memberExpr->getBase());
      const auto *memberDecl = memberExpr->getMemberDecl();
      if (const auto *fieldDecl = dyn_cast<FieldDecl>(memberDecl)) {
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
        return 0;
      }
    }

    if (const auto *castExpr = dyn_cast<ImplicitCastExpr>(expr)) {
      return doImplicitCastExpr(castExpr);
    }

    if (const auto *cxxFunctionalCastExpr =
            dyn_cast<CXXFunctionalCastExpr>(expr)) {
      // Explicit cast is a NO-OP (e.g. vector<float, 4> -> float4)
      if (cxxFunctionalCastExpr->getCastKind() == CK_NoOp) {
        return doExpr(cxxFunctionalCastExpr->getSubExpr());
      }
      emitError("Found unhandled CXXFunctionalCastExpr cast type: %0")
          << cxxFunctionalCastExpr->getCastKindName();
      return 0;
    }

    if (const auto *initListExpr = dyn_cast<InitListExpr>(expr)) {
      // First try to evaluate the expression as constant expression
      Expr::EvalResult evalResult;
      if (expr->EvaluateAsRValue(evalResult, astContext) &&
          !evalResult.HasSideEffects) {
        return translateAPValue(evalResult.Val, expr->getType());
      }

      const uint32_t resultType =
          typeTranslator.translateType(initListExpr->getType());

      // Special case for vectors of size 1.
      if (initListExpr->getNumInits() == 1) {
        return doExpr(initListExpr->getInit(0));
      }
      std::vector<uint32_t> constituents;
      for (size_t i = 0; i < initListExpr->getNumInits(); ++i) {
        constituents.push_back(doExpr(initListExpr->getInit(i)));
      }

      return theBuilder.createCompositeConstruct(resultType, constituents);
    }

    if (const auto *boolLiteral = dyn_cast<CXXBoolLiteralExpr>(expr)) {
      const bool value = boolLiteral->getValue();
      return theBuilder.getConstantBool(value);
    }

    if (const auto *intLiteral = dyn_cast<IntegerLiteral>(expr)) {
      return translateAPInt(intLiteral->getValue(), expr->getType());
    }

    if (const auto *floatLiteral = dyn_cast<FloatingLiteral>(expr)) {
      return translateAPFloat(floatLiteral->getValue(), expr->getType());
    }

    // CompoundAssignOperator is a subclass of BinaryOperator. It should be
    // checked before BinaryOperator.
    if (const auto *compoundAssignOp = dyn_cast<CompoundAssignOperator>(expr)) {
      return doCompoundAssignOperator(compoundAssignOp);
    }

    if (const auto *binOp = dyn_cast<BinaryOperator>(expr)) {
      return doBinaryOperator(binOp);
    }

    if (const auto *unaryOp = dyn_cast<UnaryOperator>(expr)) {
      return doUnaryOperator(unaryOp);
    }

    if (const auto *funcCall = dyn_cast<CallExpr>(expr)) {
      return doCallExpr(funcCall);
    }

    emitError("Expr '%0' is not supported yet.") << expr->getStmtClassName();
    // TODO: handle other expressions
    return 0;
  }

  uint32_t doBinaryOperator(const BinaryOperator *expr) {
    const auto opcode = expr->getOpcode();

    // Handle assignment first since we need to evaluate rhs before lhs.
    // For other binary operations, we need to evaluate lhs before rhs.
    if (opcode == BO_Assign) {
      const uint32_t rhs = doExpr(expr->getRHS());
      const uint32_t lhs = doExpr(expr->getLHS());

      theBuilder.createStore(lhs, rhs);
      // Assignment returns a rvalue.
      return rhs;
    }

    // Try to optimize floatN * float case
    if (opcode == BO_Mul) {
      if (const uint32_t result = tryToGenFloatVectorScale(expr))
        return result;
    }

    const uint32_t lhs = doExpr(expr->getLHS());
    const uint32_t rhs = doExpr(expr->getRHS());
    const uint32_t typeId = typeTranslator.translateType(expr->getType());
    const QualType elemType = expr->getLHS()->getType();

    switch (opcode) {
    case BO_Add:
    case BO_Sub:
    case BO_Mul:
    case BO_Div:
    case BO_Rem:
    case BO_LT:
    case BO_LE:
    case BO_GT:
    case BO_GE:
    case BO_EQ:
    case BO_NE: {
      const spv::Op spvOp = translateOp(opcode, elemType);
      return theBuilder.createBinaryOp(spvOp, typeId, lhs, rhs);
    }
    case BO_Assign: {
      llvm_unreachable("assignment already handled before");
    } break;
    default:
      break;
    }

    emitError("BinaryOperator '%0' is not supported yet.") << opcode;
    expr->dump();
    return 0;
  }

  uint32_t doCompoundAssignOperator(const CompoundAssignOperator *expr) {
    const auto opcode = expr->getOpcode();

    // Try to optimize floatN *= float case
    if (opcode == BO_MulAssign) {
      if (const uint32_t result = tryToGenFloatVectorScale(expr))
        return result;
    }

    const auto *rhs = expr->getRHS();
    const auto *lhs = expr->getLHS();

    switch (opcode) {
    case BO_AddAssign:
    case BO_SubAssign:
    case BO_MulAssign:
    case BO_DivAssign:
    case BO_RemAssign: {
      const uint32_t resultType = typeTranslator.translateType(expr->getType());

      // Evalute rhs before lhs
      const uint32_t rhsVal = doExpr(rhs);
      const uint32_t lhsPtr = doExpr(lhs);
      const uint32_t lhsVal = theBuilder.createLoad(resultType, lhsPtr);

      const spv::Op spvOp = translateOp(opcode, expr->getType());
      const uint32_t result =
          theBuilder.createBinaryOp(spvOp, resultType, lhsVal, rhsVal);
      theBuilder.createStore(lhsPtr, result);

      // Compound assign operators return lvalues.
      return lhsPtr;
    }
    default:
      emitError("CompoundAssignOperator '%0' unimplemented")
          << expr->getStmtClassName();
      return 0;
    }
  }

  uint32_t doUnaryOperator(const UnaryOperator *expr) {
    const auto opcode = expr->getOpcode();
    const auto *subExpr = expr->getSubExpr();
    const auto subType = subExpr->getType();
    const auto subValue = doExpr(subExpr);
    const auto subTypeId = typeTranslator.translateType(subType);

    switch (opcode) {
    case UO_PreInc: {
      const spv::Op spvOp = translateOp(BO_Add, subType);
      const uint32_t one = getValueOne(subType);
      const uint32_t originValue = theBuilder.createLoad(subTypeId, subValue);
      const uint32_t incValue =
          theBuilder.createBinaryOp(spvOp, subTypeId, originValue, one);
      theBuilder.createStore(subValue, incValue);
      // Prefix increment operator returns a lvalue.
      return subValue;
    }
    default:
      break;
    }

    emitError("unary operator '%0' unimplemented yet") << opcode;
    expr->dump();
    return 0;
  }

  uint32_t doImplicitCastExpr(const ImplicitCastExpr *expr) {
    const Expr *subExpr = expr->getSubExpr();
    const QualType toType = expr->getType();

    switch (expr->getCastKind()) {
    case CastKind::CK_IntegralCast: {
      // Integer literals in the AST are represented using 64bit APInt
      // themselves and then implicitly casted into the expected bitwidth.
      // We need special treatment of integer literals here because generating
      // a 64bit constant and then explicit casting in SPIR-V requires Int64
      // capability. We should avoid introducing unnecessary capabilities to
      // our best.
      llvm::APSInt intValue;
      if (expr->EvaluateAsInt(intValue, astContext, Expr::SE_NoSideEffects)) {
        return translateAPInt(intValue, toType);
      } else {
        emitError("Integral cast is not supported yet");
        return 0;
      }
    }
    case CastKind::CK_FloatingCast: {
      // First try to see if we can do constant folding for floating point
      // numbers like what we are doing for integers in the above.
      Expr::EvalResult evalResult;
      if (expr->EvaluateAsRValue(evalResult, astContext) &&
          !evalResult.HasSideEffects) {
        return translateAPFloat(evalResult.Val.getFloat(), toType);
      }
      emitError("floating cast unimplemented");
      return 0;
    }
    case CastKind::CK_LValueToRValue: {
      const uint32_t fromValue = doExpr(subExpr);
      // Using lvalue as rvalue means we need to OpLoad the contents from
      // the parameter/variable first.
      const uint32_t resultType = typeTranslator.translateType(toType);
      return theBuilder.createLoad(resultType, fromValue);
    }

    case CastKind::CK_HLSLVectorSplat: {
      const size_t size = hlsl::GetHLSLVecSize(expr->getType());
      const uint32_t scalarValue = doExpr(subExpr);

      // Just return the scalar value for vector splat with size 1
      if (size == 1) {
        return scalarValue;
      }

      const uint32_t vecTypeId = typeTranslator.translateType(toType);
      llvm::SmallVector<uint32_t, 4> elements(size, scalarValue);

      return theBuilder.createCompositeConstruct(vecTypeId, elements);
    }
    case CastKind::CK_HLSLVectorToScalarCast: {
      // We should have already treated vectors of size 1 as scalars.
      // So do nothing here.
      if (hlsl::GetHLSLVecSize(subExpr->getType()) == 1) {
        return doExpr(subExpr);
      }
      emitError("vector to scalar cast unimplemented");
      return 0;
    }
    case CastKind::CK_FunctionToPointerDecay:
      // Just need to return the function id
      return doExpr(subExpr);
    default:
      emitError("ImplictCast Kind '%0' is not supported yet.")
          << expr->getCastKindName();
      expr->dump();
      return 0;
    }
  }

  uint32_t doCallExpr(const CallExpr *callExpr) {
    const FunctionDecl *callee = callExpr->getDirectCallee();

    if (callee) {
      const uint32_t returnType =
          typeTranslator.translateType(callExpr->getType());

      // Get or forward declare the function <result-id>
      const uint32_t funcId = declIdMapper.getOrRegisterDeclResultId(callee);

      // Evaluate parameters
      llvm::SmallVector<uint32_t, 4> params;
      for (const auto *arg : callExpr->arguments()) {
        // We need to create variables for holding the values to be used as
        // arguments. The variables themselves are of pointer types.
        const uint32_t ptrType = theBuilder.getPointerType(
            typeTranslator.translateType(arg->getType()),
            spv::StorageClass::Function);

        const uint32_t tempVarId = theBuilder.addFnVariable(ptrType);
        theBuilder.createStore(tempVarId, doExpr(arg));

        params.push_back(tempVarId);
      }

      // Push the callee into the work queue if it is not there.
      if (!workQueue.count(callee)) {
        workQueue.insert(callee);
      }

      return theBuilder.createFunctionCall(returnType, funcId, params);
    }

    emitError("calling non-function unimplemented");
    return 0;
  }

  /// Translates a floatN * float multiplication into SPIR-V instructions and
  /// returns the <result-id>. Returns 0 if the given binary operation is not
  /// floatN * float.
  uint32_t tryToGenFloatVectorScale(const BinaryOperator *expr) {
    const QualType type = expr->getType();
    // We can only translate floatN * float into OpVectorTimesScalar.
    // So the result type must be floatN.
    if (!hlsl::IsHLSLVecType(type) ||
        !hlsl::GetHLSLVecElementType(type)->isFloatingType())
      return 0;

    const Expr *lhs = expr->getLHS();
    const Expr *rhs = expr->getRHS();

    // Multiplying a float vector with a float scalar will be represented in
    // AST via a binary operation with two float vectors as operands; one of
    // the operand is from an implicit cast with kind CK_HLSLVectorSplat.

    // vector * scalar
    if (hlsl::IsHLSLVecType(lhs->getType())) {
      if (const auto *cast = dyn_cast<ImplicitCastExpr>(rhs)) {
        if (cast->getCastKind() == CK_HLSLVectorSplat) {
          const uint32_t vecType =
              typeTranslator.translateType(expr->getType());
          if (isa<CompoundAssignOperator>(expr)) {
            // For floatN * float cases. We'll need to do the load/store and
            // return the lhs.
            const uint32_t rhsVal = doExpr(cast->getSubExpr());
            const uint32_t lhsPtr = doExpr(lhs);
            const uint32_t lhsVal = theBuilder.createLoad(vecType, lhsPtr);
            const uint32_t result = theBuilder.createBinaryOp(
                spv::Op::OpVectorTimesScalar, vecType, lhsVal, rhsVal);
            theBuilder.createStore(lhsPtr, result);
            return lhsPtr;
          } else {
            const uint32_t lhsId = doExpr(lhs);
            const uint32_t rhsId = doExpr(cast->getSubExpr());
            return theBuilder.createBinaryOp(spv::Op::OpVectorTimesScalar,
                                             vecType, lhsId, rhsId);
          }
        }
      }
    }

    // scalar * vector
    if (hlsl::IsHLSLVecType(rhs->getType())) {
      if (const auto *cast = dyn_cast<ImplicitCastExpr>(lhs)) {
        if (cast->getCastKind() == CK_HLSLVectorSplat) {
          const uint32_t vecType =
              typeTranslator.translateType(expr->getType());
          const uint32_t lhsId = doExpr(cast->getSubExpr());
          const uint32_t rhsId = doExpr(rhs);
          return theBuilder.createBinaryOp(spv::Op::OpVectorTimesScalar,
                                           vecType, rhsId, lhsId);
        }
      }
    }

    return 0;
  }

  /// Translates the given frontend binary operator into its SPIR-V equivalent
  /// taking consideration of the operand type.
  spv::Op translateOp(BinaryOperator::Opcode op, QualType type) {
    // TODO: the following is not considering vector types yet.
    const bool isSintType = isSintOrVecOfSintType(type);
    const bool isUintType = isUintOrVecOfUintType(type);
    const bool isFloatType = isFloatOrVecOfFloatType(type);

#define BIN_OP_CASE_INT_FLOAT(kind, intBinOp, floatBinOp)                      \
  \
case BO_##kind : {                                                             \
    if (isSintType || isUintType) {                                            \
      return spv::Op::Op##intBinOp;                                            \
    }                                                                          \
    if (isFloatType) {                                                         \
      return spv::Op::Op##floatBinOp;                                          \
    }                                                                          \
  }                                                                            \
  break

#define BIN_OP_CASE_SINT_UINT_FLOAT(kind, sintBinOp, uintBinOp, floatBinOp)    \
  \
case BO_##kind : {                                                             \
    if (isSintType) {                                                          \
      return spv::Op::Op##sintBinOp;                                           \
    }                                                                          \
    if (isUintType) {                                                          \
      return spv::Op::Op##uintBinOp;                                           \
    }                                                                          \
    if (isFloatType) {                                                         \
      return spv::Op::Op##floatBinOp;                                          \
    }                                                                          \
  }                                                                            \
  break

    switch (op) {
      BIN_OP_CASE_INT_FLOAT(Add, IAdd, FAdd);
      BIN_OP_CASE_INT_FLOAT(AddAssign, IAdd, FAdd);
      BIN_OP_CASE_INT_FLOAT(Sub, ISub, FSub);
      BIN_OP_CASE_INT_FLOAT(SubAssign, ISub, FSub);
      BIN_OP_CASE_INT_FLOAT(Mul, IMul, FMul);
      BIN_OP_CASE_INT_FLOAT(MulAssign, IMul, FMul);
      BIN_OP_CASE_SINT_UINT_FLOAT(Div, SDiv, UDiv, FDiv);
      BIN_OP_CASE_SINT_UINT_FLOAT(DivAssign, SDiv, UDiv, FDiv);
      // According to HLSL spec, "the modulus operator returns the remainder of
      // a division." "The % operator is defined only in cases where either both
      // sides are positive or both sides are negative."
      //
      // In SPIR-V, there are two reminder operations: Op*Rem and Op*Mod. With
      // the former, the sign of a non-0 result comes from Operand 1, while
      // with the latter, from Operand 2.
      //
      // For operands with different signs, technically we can map % to either
      // Op*Rem or Op*Mod since it's undefined behavior. But it is more
      // consistent with C (HLSL starts as a C derivative) and Clang frontend
      // const expression evaluation if we map % to Op*Rem.
      //
      // Note there is no OpURem in SPIR-V.
      BIN_OP_CASE_SINT_UINT_FLOAT(Rem, SRem, UMod, FRem);
      BIN_OP_CASE_SINT_UINT_FLOAT(RemAssign, SRem, UMod, FRem);
      BIN_OP_CASE_SINT_UINT_FLOAT(LT, SLessThan, ULessThan, FOrdLessThan);
      BIN_OP_CASE_SINT_UINT_FLOAT(LE, SLessThanEqual, ULessThanEqual,
                                  FOrdLessThanEqual);
      BIN_OP_CASE_SINT_UINT_FLOAT(GT, SGreaterThan, UGreaterThan,
                                  FOrdGreaterThan);
      BIN_OP_CASE_SINT_UINT_FLOAT(GE, SGreaterThanEqual, UGreaterThanEqual,
                                  FOrdGreaterThanEqual);
      BIN_OP_CASE_INT_FLOAT(EQ, IEqual, FOrdEqual);
      BIN_OP_CASE_INT_FLOAT(NE, INotEqual, FOrdNotEqual);
    default:
      break;
    }

#undef BIN_OP_CASE_INT_FLOAT
#undef BIN_OP_CASE_SINT_UINT_FLOAT

    emitError("translating binary operator '%0' unimplemented") << op;
    return spv::Op::OpNop;
  }

  /// Returns the <result-id> for constant value 1 of the given type.
  uint32_t getValueOne(QualType type) {
    if (type->isSignedIntegerType()) {
      return theBuilder.getConstantInt32(1);
    }

    if (type->isUnsignedIntegerType()) {
      return theBuilder.getConstantUint32(1);
    }

    if (type->isFloatingType()) {
      return theBuilder.getConstantFloat32(1.0);
    }

    if (hlsl::IsHLSLVecType(type)) {
      const QualType elemType = hlsl::GetHLSLVecElementType(type);
      const uint32_t elemOneId = getValueOne(elemType);

      const size_t size = hlsl::GetHLSLVecSize(type);
      if (size == 1)
        return elemOneId;

      llvm::SmallVector<uint32_t, 4> elements(size, elemOneId);

      const uint32_t vecTypeId = typeTranslator.translateType(type);
      return theBuilder.getConstantComposite(vecTypeId, elements);
    }

    emitError("getting value 1 for type '%0' unimplemented") << type;
    return 0;
  }

  /// Translates the given frontend APValue into its SPIR-V equivalent for the
  /// given targetType.
  uint32_t translateAPValue(const APValue &value, const QualType targetType) {
    if (targetType->isBooleanType()) {
      const bool boolValue = value.getInt().getBoolValue();
      return theBuilder.getConstantBool(boolValue);
    }

    if (targetType->isIntegerType()) {
      const llvm::APInt &intValue = value.getInt();
      return translateAPInt(intValue, targetType);
    }

    if (targetType->isFloatingType()) {
      const llvm::APFloat &floatValue = value.getFloat();
      return translateAPFloat(floatValue, targetType);
    }

    if (hlsl::IsHLSLVecType(targetType)) {
      const uint32_t vecType = typeTranslator.translateType(targetType);
      const QualType elemType = hlsl::GetHLSLVecElementType(targetType);

      const auto numElements = value.getVectorLength();
      // Special case for vectors of size 1. SPIR-V doesn't support this vector
      // size so we need to translate it to scalar values.
      if (numElements == 1) {
        return translateAPValue(value.getVectorElt(0), elemType);
      }

      llvm::SmallVector<uint32_t, 4> elements;
      for (uint32_t i = 0; i < numElements; ++i) {
        elements.push_back(translateAPValue(value.getVectorElt(i), elemType));
      }

      return theBuilder.getConstantComposite(vecType, elements);
    }

    emitError("APValue of type '%0' is not supported yet.") << value.getKind();
    value.dump();
    return 0;
  }

  /// Translates the given frontend APInt into its SPIR-V equivalent for the
  /// given targetType.
  uint32_t translateAPInt(const llvm::APInt &intValue, QualType targetType) {
    const auto bitwidth = astContext.getIntWidth(targetType);

    if (targetType->isSignedIntegerType()) {
      const int64_t value = intValue.getSExtValue();
      switch (bitwidth) {
      case 32:
        return theBuilder.getConstantInt32(static_cast<int32_t>(value));
      default:
        break;
      }
    } else {
      const uint64_t value = intValue.getZExtValue();
      switch (bitwidth) {
      case 32:
        return theBuilder.getConstantUint32(static_cast<uint32_t>(value));
      default:
        break;
      }
    }

    emitError("APInt for target bitwidth '%0' is not supported yet.")
        << bitwidth;
    return 0;
  }

  /// Translates the given frontend APFloat into its SPIR-V equivalent for the
  /// given targetType.
  uint32_t translateAPFloat(const llvm::APFloat &floatValue,
                            QualType targetType) {
    const auto &semantics = astContext.getFloatTypeSemantics(targetType);
    const auto bitwidth = llvm::APFloat::getSizeInBits(semantics);

    switch (bitwidth) {
    case 32:
      return theBuilder.getConstantFloat32(floatValue.convertToFloat());
    default:
      break;
    }

    emitError("APFloat for target bitwidth '%0' is not supported yet.")
        << bitwidth;
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
  ASTContext &astContext;
  DiagnosticsEngine &diags;

  /// Entry function name and shader stage. Both of them are derived from the
  /// command line and should be const.
  const llvm::StringRef entryFunctionName;
  const spv::ExecutionModel shaderStage;

  SPIRVContext theContext;
  ModuleBuilder theBuilder;
  DeclResultIdMapper declIdMapper;
  TypeTranslator typeTranslator;

  /// A queue of decls reachable from the entry function. Decls inserted into
  /// this queue will persist to avoid duplicated translations. And we'd like
  /// a deterministic order of iterating the queue for finding the next decl
  /// to translate. So we need SetVector here.
  llvm::SetVector<const DeclaratorDecl *> workQueue;
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
