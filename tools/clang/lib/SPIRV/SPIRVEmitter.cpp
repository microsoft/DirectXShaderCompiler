//===------- SPIRVEmitter.h - SPIR-V Binary Code Emitter --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
//
//  This file implements a SPIR-V emitter class that takes in HLSL AST and emits
//  SPIR-V binary words.
//
//===----------------------------------------------------------------------===//

#include "SPIRVEmitter.h"

#include "dxc/HlslIntrinsicOp.h"
#include "llvm/ADT/StringExtras.h"

#include "InitListHandler.h"

namespace clang {
namespace spirv {

namespace {

// TODO: Maybe we should move these type probing functions to TypeTranslator.

/// Returns true if the two types are the same scalar or vector type.
bool isSameScalarOrVecType(QualType type1, QualType type2) {
  {
    QualType scalarType1 = {}, scalarType2 = {};
    if (TypeTranslator::isScalarType(type1, &scalarType1) &&
        TypeTranslator::isScalarType(type2, &scalarType2))
      return scalarType1.getCanonicalType() == scalarType2.getCanonicalType();
  }

  {
    QualType elemType1 = {}, elemType2 = {};
    uint32_t count1 = {}, count2 = {};
    if (TypeTranslator::isVectorType(type1, &elemType1, &count1) &&
        TypeTranslator::isVectorType(type2, &elemType2, &count2))
      return count1 == count2 &&
             elemType1.getCanonicalType() == elemType2.getCanonicalType();
  }

  return false;
}

/// Returns true if the given type is a bool or vector of bool type.
bool isBoolOrVecOfBoolType(QualType type) {
  QualType elemType = {};
  return (TypeTranslator::isScalarType(type, &elemType) ||
          TypeTranslator::isVectorType(type, &elemType)) &&
         elemType->isBooleanType();
}

/// Returns true if the given type is a signed integer or vector of signed
/// integer type.
bool isSintOrVecOfSintType(QualType type) {
  QualType elemType = {};
  return (TypeTranslator::isScalarType(type, &elemType) ||
          TypeTranslator::isVectorType(type, &elemType)) &&
         elemType->isSignedIntegerType();
}

/// Returns true if the given type is an unsigned integer or vector of unsigned
/// integer type.
bool isUintOrVecOfUintType(QualType type) {
  QualType elemType = {};
  return (TypeTranslator::isScalarType(type, &elemType) ||
          TypeTranslator::isVectorType(type, &elemType)) &&
         elemType->isUnsignedIntegerType();
}

/// Returns true if the given type is a float or vector of float type.
bool isFloatOrVecOfFloatType(QualType type) {
  QualType elemType = {};
  return (TypeTranslator::isScalarType(type, &elemType) ||
          TypeTranslator::isVectorType(type, &elemType)) &&
         elemType->isFloatingType();
}

/// Returns true if the given type is a bool or vector/matrix of bool type.
bool isBoolOrVecMatOfBoolType(QualType type) {
  return isBoolOrVecOfBoolType(type) ||
         (hlsl::IsHLSLMatType(type) &&
          hlsl::GetHLSLMatElementType(type)->isBooleanType());
}

/// Returns true if the given type is a signed integer or vector/matrix of
/// signed integer type.
bool isSintOrVecMatOfSintType(QualType type) {
  return isSintOrVecOfSintType(type) ||
         (hlsl::IsHLSLMatType(type) &&
          hlsl::GetHLSLMatElementType(type)->isSignedIntegerType());
}

/// Returns true if the given type is an unsigned integer or vector/matrix of
/// unsigned integer type.
bool isUintOrVecMatOfUintType(QualType type) {
  return isUintOrVecOfUintType(type) ||
         (hlsl::IsHLSLMatType(type) &&
          hlsl::GetHLSLMatElementType(type)->isUnsignedIntegerType());
}

/// Returns true if the given type is a float or vector/matrix of float type.
bool isFloatOrVecMatOfFloatType(QualType type) {
  return isFloatOrVecOfFloatType(type) ||
         (hlsl::IsHLSLMatType(type) &&
          hlsl::GetHLSLMatElementType(type)->isFloatingType());
}

bool isSpirvMatrixOp(spv::Op opcode) {
  switch (opcode) {
  case spv::Op::OpMatrixTimesMatrix:
  case spv::Op::OpMatrixTimesVector:
  case spv::Op::OpMatrixTimesScalar:
    return true;
  default:
    break;
  }
  return false;
}

/// \brief Returns the statement that is the immediate parent AST node of the
/// given statement. Returns nullptr if there are no parents nodes.
const Stmt *getImmediateParent(ASTContext &astContext, const Stmt *stmt) {
  const auto &parents = astContext.getParents(*stmt);
  return parents.empty() ? nullptr : parents[0].get<Stmt>();
}

/// \brief Returns true if there are no unreachable statements after the given
/// statement. A "continue" statement and "break" statement cause a branch to a
/// loop header and a loop merge block, respectively. A "return" statement
/// causes a branch out of the function. In such situations, any statement that
/// follows the break/continue/return will not be executed. When this method
/// returns false, it indicates that there exists statements that are not going
/// to be executed.
bool isLastStmtBeforeControlFlowBranching(ASTContext &astContext,
                                          const Stmt *stmt) {
  const Stmt *parent = getImmediateParent(astContext, stmt);
  if (const auto *parentCS = dyn_cast_or_null<CompoundStmt>(parent)) {
    if (stmt == *(parentCS->body_rbegin())) {
      // The current statement is the last child node of the parent.

      // Handle nested compound statements. e.g.
      // while (cond) {
      //   StmtA;
      //   {
      //      StmtB;
      //      {{continue;}}
      //   }
      //   {StmtC;}
      //   StmtD;
      // }
      //
      // The continue statement is the last statement in its CompoundStmt scope,
      // but, if nested compound statements are flattened, the continue
      // statement is not the last statement in the current loop scope.
      const Stmt *grandparent = getImmediateParent(astContext, parent);
      if (grandparent && isa<CompoundStmt>(grandparent))
        return isLastStmtBeforeControlFlowBranching(astContext, parent);

      return true;
    }
  }

  return false;
}

bool isLoopStmt(const Stmt *stmt) {
  return isa<ForStmt>(stmt) || isa<WhileStmt>(stmt) || isa<DoStmt>(stmt);
}

} // namespace

SPIRVEmitter::SPIRVEmitter(CompilerInstance &ci)
    : theCompilerInstance(ci), astContext(ci.getASTContext()),
      diags(ci.getDiagnostics()),
      entryFunctionName(ci.getCodeGenOpts().HLSLEntryFunction),
      shaderModel(*hlsl::ShaderModel::GetByName(
          ci.getCodeGenOpts().HLSLProfile.c_str())),
      theContext(), theBuilder(&theContext),
      declIdMapper(shaderModel, theBuilder, diags),
      typeTranslator(theBuilder, diags), entryFunctionId(0),
      curFunction(nullptr) {
  if (shaderModel.GetKind() == hlsl::ShaderModel::Kind::Invalid)
    emitError("unknown shader module: %0") << shaderModel.GetName();
}

void SPIRVEmitter::HandleTranslationUnit(ASTContext &context) {
  AddRequiredCapabilitiesForShaderModel();

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
    } else if (auto *varDecl = dyn_cast<VarDecl>(decl)) {
      doVarDecl(varDecl);
    }
  }

  // Translate all functions reachable from the entry function.
  // The queue can grow in the meanwhile; so need to keep evaluating
  // workQueue.size().
  for (uint32_t i = 0; i < workQueue.size(); ++i) {
    doDecl(workQueue[i]);
  }

  theBuilder.addEntryPoint(getSpirvShaderStage(shaderModel), entryFunctionId,
                           entryFunctionName, declIdMapper.collectStageVars());

  AddExecutionModeForEntryPoint(entryFunctionId);

  // Add Location decorations to stage input/output variables.
  declIdMapper.finalizeStageIOLocations();

  // Output the constructed module.
  std::vector<uint32_t> m = theBuilder.takeModule();
  theCompilerInstance.getOutStream()->write(
      reinterpret_cast<const char *>(m.data()), m.size() * 4);
}

void SPIRVEmitter::doDecl(const Decl *decl) {
  if (const auto *varDecl = dyn_cast<VarDecl>(decl)) {
    doVarDecl(varDecl);
  } else if (const auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
    doFunctionDecl(funcDecl);
  } else {
    // TODO: Implement handling of other Decl types.
    emitWarning("Decl type '%0' is not supported yet.")
        << decl->getDeclKindName();
  }
}

void SPIRVEmitter::doStmt(const Stmt *stmt,
                          llvm::ArrayRef<const Attr *> attrs) {
  if (const auto *compoundStmt = dyn_cast<CompoundStmt>(stmt)) {
    for (auto *st : compoundStmt->body())
      doStmt(st);
  } else if (const auto *retStmt = dyn_cast<ReturnStmt>(stmt)) {
    doReturnStmt(retStmt);
  } else if (const auto *declStmt = dyn_cast<DeclStmt>(stmt)) {
    doDeclStmt(declStmt);
  } else if (const auto *ifStmt = dyn_cast<IfStmt>(stmt)) {
    doIfStmt(ifStmt);
  } else if (const auto *switchStmt = dyn_cast<SwitchStmt>(stmt)) {
    doSwitchStmt(switchStmt, attrs);
  } else if (const auto *caseStmt = dyn_cast<CaseStmt>(stmt)) {
    processCaseStmtOrDefaultStmt(stmt);
  } else if (const auto *defaultStmt = dyn_cast<DefaultStmt>(stmt)) {
    processCaseStmtOrDefaultStmt(stmt);
  } else if (const auto *breakStmt = dyn_cast<BreakStmt>(stmt)) {
    doBreakStmt(breakStmt);
  } else if (const auto *theDoStmt = dyn_cast<DoStmt>(stmt)) {
    doDoStmt(theDoStmt, attrs);
  } else if (const auto *continueStmt = dyn_cast<ContinueStmt>(stmt)) {
    doContinueStmt(continueStmt);
  } else if (const auto *whileStmt = dyn_cast<WhileStmt>(stmt)) {
    doWhileStmt(whileStmt, attrs);
  } else if (const auto *forStmt = dyn_cast<ForStmt>(stmt)) {
    doForStmt(forStmt, attrs);
  } else if (const auto *nullStmt = dyn_cast<NullStmt>(stmt)) {
    // For the null statement ";". We don't need to do anything.
  } else if (const auto *expr = dyn_cast<Expr>(stmt)) {
    // All cases for expressions used as statements
    doExpr(expr);
  } else if (const auto *attrStmt = dyn_cast<AttributedStmt>(stmt)) {
    doStmt(attrStmt->getSubStmt(), attrStmt->getAttrs());
  } else {
    emitError("Stmt '%0' is not supported yet.") << stmt->getStmtClassName();
  }
}

uint32_t SPIRVEmitter::doExpr(const Expr *expr) {
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
      const uint32_t ptrType = theBuilder.getPointerType(
          fieldType, declIdMapper.resolveStorageClass(memberExpr->getBase()));
      return theBuilder.createAccessChain(ptrType, base, {index});
    } else {
      emitError("Decl '%0' in MemberExpr is not supported yet.")
          << memberDecl->getDeclKindName();
      return 0;
    }
  }

  if (const auto *castExpr = dyn_cast<CastExpr>(expr)) {
    return doCastExpr(castExpr);
  }

  if (const auto *initListExpr = dyn_cast<InitListExpr>(expr)) {
    return doInitListExpr(initListExpr);
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

  if (const auto *vecElemExpr = dyn_cast<HLSLVectorElementExpr>(expr)) {
    return doHLSLVectorElementExpr(vecElemExpr);
  }

  if (const auto *matElemExpr = dyn_cast<ExtMatrixElementExpr>(expr)) {
    return doExtMatrixElementExpr(matElemExpr);
  }

  if (const auto *funcCall = dyn_cast<CallExpr>(expr)) {
    return doCallExpr(funcCall);
  }

  if (const auto *condExpr = dyn_cast<ConditionalOperator>(expr)) {
    return doConditionalOperator(condExpr);
  }

  emitError("Expr '%0' is not supported yet.") << expr->getStmtClassName();
  // TODO: handle other expressions
  return 0;
}

uint32_t SPIRVEmitter::loadIfGLValue(const Expr *expr) {
  const uint32_t result = doExpr(expr);
  if (expr->isGLValue()) {
    const uint32_t baseTyId = typeTranslator.translateType(expr->getType());
    return theBuilder.createLoad(baseTyId, result);
  }

  return result;
}

uint32_t SPIRVEmitter::castToType(uint32_t value, QualType fromType,
                                  QualType toType) {
  if (isFloatOrVecOfFloatType(toType))
    return castToFloat(value, fromType, toType);

  // Order matters here. Bool (vector) values will also be considered as uint
  // (vector) values. So given a bool (vector) argument, isUintOrVecOfUintType()
  // will also return true. We need to check bool before uint. The opposite is
  // not true.
  if (isBoolOrVecOfBoolType(toType))
    return castToBool(value, fromType, toType);

  if (isSintOrVecOfSintType(toType) || isUintOrVecOfUintType(toType))
    return castToInt(value, fromType, toType);

  emitError("casting to type %0 unimplemented") << toType;
  return 0;
}

void SPIRVEmitter::doFunctionDecl(const FunctionDecl *decl) {
  curFunction = decl;

  const llvm::StringRef funcName = decl->getName();

  uint32_t funcId = 0;

  if (funcName == entryFunctionName) {
    // First create stage variables for the entry point.
    if (!declIdMapper.createStageVarFromFnReturn(decl))
      return;

    for (const auto *param : decl->params())
      if (!declIdMapper.createStageVarFromFnParam(param))
        return;

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
      const uint32_t valueType = typeTranslator.translateType(param->getType());
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
          theBuilder.addFnParam(paramTypes[i], paramDecl->getName());
      declIdMapper.registerDeclResultId(paramDecl, paramId);
    }
  }

  if (decl->hasBody()) {
    // The entry basic block.
    const uint32_t entryLabel = theBuilder.createBasicBlock("bb.entry");
    theBuilder.setInsertPoint(entryLabel);

    // Initialize all global variables at the beginning of the entry function
    if (funcId == entryFunctionId)
      for (const VarDecl *varDecl : toInitGloalVars)
        theBuilder.createStore(declIdMapper.getDeclResultId(varDecl),
                               doExpr(varDecl->getInit()));

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

void SPIRVEmitter::doVarDecl(const VarDecl *decl) {
  // The contents in externally visible variables can be updated via the
  // pipeline. They should be handled differently from file and function scope
  // variables.
  // File scope variables (static "global" and "local" variables) belongs to
  // the Private storage class, while function scope variables (normal "local"
  // variables) belongs to the Function storage class.
  if (!decl->isExternallyVisible()) {
    // We already know the variable is not externally visible here. If it does
    // not have local storage, it should be file scope variable.
    const bool isFileScopeVar = !decl->hasLocalStorage();
    const uint32_t varType = typeTranslator.translateType(decl->getType());

    // Handle initializer. SPIR-V requires that "initializer must be an <id>
    // from a constant instruction or a global (module scope) OpVariable
    // instruction."
    llvm::Optional<uint32_t> constInit;
    if (decl->hasInit()) {
      // First try to evaluate the initializer as a constant expression
      Expr::EvalResult evalResult;
      if (decl->getInit()->EvaluateAsRValue(evalResult, astContext) &&
          !evalResult.HasSideEffects) {
        constInit = llvm::Optional<uint32_t>(
            translateAPValue(evalResult.Val, decl->getType()));
      }
    } else if (isFileScopeVar) {
      // For static variables, if no initializers are provided, we should
      // initialize them to zero values.
      constInit = llvm::Optional<uint32_t>(theBuilder.getConstantNull(varType));
    }

    const uint32_t varId =
        isFileScopeVar
            ? theBuilder.addFileVar(varType, decl->getName(), constInit)
            : theBuilder.addFnVar(varType, decl->getName(), constInit);
    declIdMapper.registerDeclResultId(decl, varId);

    // If we cannot evaluate the initializer as a constant expression, we'll
    // need to use OpStore to write the initializer to the variable.
    // Also we should only evaluate the initializer once for a static variable.
    if (decl->hasInit() && !constInit.hasValue()) {
      if (isFileScopeVar) {
        if (decl->isStaticLocal()) {
          initOnce(decl->getName(), varId, decl->getInit());
        } else {
          // Defer to initialize these global variables at the beginning of the
          // entry function.
          toInitGloalVars.push_back(decl);
        }
      } else {
        theBuilder.createStore(varId, doExpr(decl->getInit()));
      }
    }
  } else {
    emitError("Global variables are not supported yet.");
  }
}

spv::LoopControlMask SPIRVEmitter::translateLoopAttribute(const Attr &attr) {
  switch (attr.getKind()) {
  case attr::HLSLLoop:
  case attr::HLSLFastOpt:
    return spv::LoopControlMask::DontUnroll;
  case attr::HLSLUnroll:
    return spv::LoopControlMask::Unroll;
  case attr::HLSLAllowUAVCondition:
    emitWarning("Unsupported allow_uav_condition attribute ignored.");
    break;
  default:
    emitError("Found unknown loop attribute.");
  }
  return spv::LoopControlMask::MaskNone;
}

void SPIRVEmitter::doDoStmt(const DoStmt *theDoStmt,
                            llvm::ArrayRef<const Attr *> attrs) {
  // do-while loops are composed of:
  //
  // do {
  //   <body>
  // } while(<check>);
  //
  // SPIR-V requires loops to have a merge basic block as well as a continue
  // basic block. Even though do-while loops do not have an explicit continue
  // block as in for-loops, we still do need to create a continue block.
  //
  // Since SPIR-V requires structured control flow, we need two more basic
  // blocks, <header> and <merge>. <header> is the block before control flow
  // diverges, and <merge> is the block where control flow subsequently
  // converges. The <check> can be performed in the <continue> basic block.
  // The final CFG should normally be like the following. Exceptions
  // will occur with non-local exits like loop breaks or early returns.
  //
  //            +----------+
  //            |  header  | <-----------------------------------+
  //            +----------+                                     |
  //                 |                                           |  (true)
  //                 v                                           |
  //             +------+       +--------------------+           |
  //             | body | ----> | continue (<check>) |-----------+
  //             +------+       +--------------------+
  //                                     |
  //                                     | (false)
  //             +-------+               |
  //             | merge | <-------------+
  //             +-------+
  //
  // For more details, see "2.11. Structured Control Flow" in the SPIR-V spec.

  const spv::LoopControlMask loopControl =
      attrs.empty() ? spv::LoopControlMask::MaskNone
                    : translateLoopAttribute(*attrs.front());

  // Create basic blocks
  const uint32_t headerBB = theBuilder.createBasicBlock("do_while.header");
  const uint32_t bodyBB = theBuilder.createBasicBlock("do_while.body");
  const uint32_t continueBB = theBuilder.createBasicBlock("do_while.continue");
  const uint32_t mergeBB = theBuilder.createBasicBlock("do_while.merge");

  // Make sure any continue statements branch to the continue block, and any
  // break statements branch to the merge block.
  continueStack.push(continueBB);
  breakStack.push(mergeBB);

  // Branch from the current insert point to the header block.
  theBuilder.createBranch(headerBB);
  theBuilder.addSuccessor(headerBB);

  // Process the <header> block
  // The header block must always branch to the body.
  theBuilder.setInsertPoint(headerBB);
  theBuilder.createBranch(bodyBB, mergeBB, continueBB, loopControl);
  theBuilder.addSuccessor(bodyBB);
  // The current basic block has OpLoopMerge instruction. We need to set its
  // continue and merge target.
  theBuilder.setContinueTarget(continueBB);
  theBuilder.setMergeTarget(mergeBB);

  // Process the <body> block
  theBuilder.setInsertPoint(bodyBB);
  if (const Stmt *body = theDoStmt->getBody()) {
    doStmt(body);
  }
  if (!theBuilder.isCurrentBasicBlockTerminated())
    theBuilder.createBranch(continueBB);
  theBuilder.addSuccessor(continueBB);

  // Process the <continue> block. The check for whether the loop should
  // continue lies in the continue block.
  // *NOTE*: There's a SPIR-V rule that when a conditional branch is to occur in
  // a continue block of a loop, there should be no OpSelectionMerge. Only an
  // OpBranchConditional must be specified.
  theBuilder.setInsertPoint(continueBB);
  uint32_t condition = 0;
  if (const Expr *check = theDoStmt->getCond()) {
    condition = doExpr(check);
  } else {
    condition = theBuilder.getConstantBool(true);
  }
  theBuilder.createConditionalBranch(condition, headerBB, mergeBB);
  theBuilder.addSuccessor(headerBB);
  theBuilder.addSuccessor(mergeBB);

  // Set insertion point to the <merge> block for subsequent statements
  theBuilder.setInsertPoint(mergeBB);

  // Done with the current scope's continue block and merge block.
  continueStack.pop();
  breakStack.pop();
}

void SPIRVEmitter::doContinueStmt(const ContinueStmt *continueStmt) {
  assert(!theBuilder.isCurrentBasicBlockTerminated());
  const uint32_t continueTargetBB = continueStack.top();
  theBuilder.createBranch(continueTargetBB);
  theBuilder.addSuccessor(continueTargetBB);

  // If any statements follow a continue statement in a loop, they will not be
  // executed. For example, StmtB and StmtC below are never executed:
  //
  // while (true) {
  //   StmtA;
  //   continue;
  //   StmtB;
  //   StmtC;
  // }
  //
  // To handle such cases, we do not stop tranlsation. We create a new basic
  // block in which StmtB and StmtC will be translated.
  // Note that since this basic block is unreachable, BlockReadableOrderVisitor
  // will not emit it in the final module binary.
  if (!isLastStmtBeforeControlFlowBranching(astContext, continueStmt)) {
    const uint32_t unreachableBB =
        theBuilder.createBasicBlock("unreachable", /*isReachable*/ false);
    theBuilder.setInsertPoint(unreachableBB);
  }
}

void SPIRVEmitter::doWhileStmt(const WhileStmt *whileStmt,
                               llvm::ArrayRef<const Attr *> attrs) {
  // While loops are composed of:
  //   while (<check>)  { <body> }
  //
  // SPIR-V requires loops to have a merge basic block as well as a continue
  // basic block. Even though while loops do not have an explicit continue
  // block as in for-loops, we still do need to create a continue block.
  //
  // Since SPIR-V requires structured control flow, we need two more basic
  // blocks, <header> and <merge>. <header> is the block before control flow
  // diverges, and <merge> is the block where control flow subsequently
  // converges. The <check> block can take the responsibility of the <header>
  // block. The final CFG should normally be like the following. Exceptions
  // will occur with non-local exits like loop breaks or early returns.
  //
  //            +----------+
  //            |  header  | <------------------+
  //            | (check)  |                    |
  //            +----------+                    |
  //                 |                          |
  //         +-------+-------+                  |
  //         | false         | true             |
  //         |               v                  |
  //         |            +------+     +------------------+
  //         |            | body | --> | continue (no-op) |
  //         v            +------+     +------------------+
  //     +-------+
  //     | merge |
  //     +-------+
  //
  // For more details, see "2.11. Structured Control Flow" in the SPIR-V spec.

  const spv::LoopControlMask loopControl =
      attrs.empty() ? spv::LoopControlMask::MaskNone
                    : translateLoopAttribute(*attrs.front());

  // Create basic blocks
  const uint32_t checkBB = theBuilder.createBasicBlock("while.check");
  const uint32_t bodyBB = theBuilder.createBasicBlock("while.body");
  const uint32_t continueBB = theBuilder.createBasicBlock("while.continue");
  const uint32_t mergeBB = theBuilder.createBasicBlock("while.merge");

  // Make sure any continue statements branch to the continue block, and any
  // break statements branch to the merge block.
  continueStack.push(continueBB);
  breakStack.push(mergeBB);

  // Process the <check> block
  theBuilder.createBranch(checkBB);
  theBuilder.addSuccessor(checkBB);
  theBuilder.setInsertPoint(checkBB);

  // If we have:
  // while (int a = foo()) {...}
  // we should evaluate 'a' by calling 'foo()' every single time the check has
  // to occur.
  if (const auto *condVarDecl = whileStmt->getConditionVariableDeclStmt())
    doStmt(condVarDecl);

  uint32_t condition = 0;
  if (const Expr *check = whileStmt->getCond()) {
    condition = doExpr(check);
  } else {
    condition = theBuilder.getConstantBool(true);
  }
  theBuilder.createConditionalBranch(condition, bodyBB,
                                     /*false branch*/ mergeBB,
                                     /*merge*/ mergeBB, continueBB,
                                     spv::SelectionControlMask::MaskNone,
                                     loopControl);
  theBuilder.addSuccessor(bodyBB);
  theBuilder.addSuccessor(mergeBB);
  // The current basic block has OpLoopMerge instruction. We need to set its
  // continue and merge target.
  theBuilder.setContinueTarget(continueBB);
  theBuilder.setMergeTarget(mergeBB);

  // Process the <body> block
  theBuilder.setInsertPoint(bodyBB);
  if (const Stmt *body = whileStmt->getBody()) {
    doStmt(body);
  }
  if (!theBuilder.isCurrentBasicBlockTerminated())
    theBuilder.createBranch(continueBB);
  theBuilder.addSuccessor(continueBB);

  // Process the <continue> block. While loops do not have an explicit
  // continue block. The continue block just branches to the <check> block.
  theBuilder.setInsertPoint(continueBB);
  theBuilder.createBranch(checkBB);
  theBuilder.addSuccessor(checkBB);

  // Set insertion point to the <merge> block for subsequent statements
  theBuilder.setInsertPoint(mergeBB);

  // Done with the current scope's continue and merge blocks.
  continueStack.pop();
  breakStack.pop();
}

void SPIRVEmitter::doForStmt(const ForStmt *forStmt,
                             llvm::ArrayRef<const Attr *> attrs) {
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
  const spv::LoopControlMask loopControl =
      attrs.empty() ? spv::LoopControlMask::MaskNone
                    : translateLoopAttribute(*attrs.front());

  // Create basic blocks
  const uint32_t checkBB = theBuilder.createBasicBlock("for.check");
  const uint32_t bodyBB = theBuilder.createBasicBlock("for.body");
  const uint32_t continueBB = theBuilder.createBasicBlock("for.continue");
  const uint32_t mergeBB = theBuilder.createBasicBlock("for.merge");

  // Make sure any continue statements branch to the continue block, and any
  // break statements branch to the merge block.
  continueStack.push(continueBB);
  breakStack.push(mergeBB);

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
                                     /*merge*/ mergeBB, continueBB,
                                     spv::SelectionControlMask::MaskNone,
                                     loopControl);
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
  if (!theBuilder.isCurrentBasicBlockTerminated())
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

  // Done with the current scope's continue block and merge block.
  continueStack.pop();
  breakStack.pop();
}

void SPIRVEmitter::doIfStmt(const IfStmt *ifStmt) {
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

  if (const auto *declStmt = ifStmt->getConditionVariableDeclStmt())
    doDeclStmt(declStmt);

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

void SPIRVEmitter::doReturnStmt(const ReturnStmt *stmt) {
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

  const uint32_t stageVarId = declIdMapper.getRemappedDeclResultId(curFunction);

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
    const Expr *retValue = stmt->getRetValue()->IgnoreParenLValueCasts();

    // Then go through all fields.
    uint32_t fieldIndex = 0;
    for (const auto *field : structType->getDecl()->fields()) {
      // Load the value from the current field.
      const uint32_t valueType = typeTranslator.translateType(field->getType());
      const uint32_t ptrType = theBuilder.getPointerType(
          typeTranslator.translateType(field->getType()),
          declIdMapper.resolveStorageClass(retValue));
      const uint32_t indexId = theBuilder.getConstantInt32(fieldIndex++);
      const uint32_t valuePtr =
          theBuilder.createAccessChain(ptrType, doExpr(retValue), {indexId});
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

bool SPIRVEmitter::breakStmtIsLastStmtInCaseStmt(const BreakStmt *breakStmt,
                                                 const SwitchStmt *switchStmt) {
  std::vector<const Stmt *> flatSwitch;
  flattenSwitchStmtAST(switchStmt->getBody(), &flatSwitch);
  auto iter =
      std::find(flatSwitch.begin(), flatSwitch.end(), cast<Stmt>(breakStmt));
  assert(iter != std::end(flatSwitch));

  // We are interested to know what comes after the BreakStmt.
  ++iter;

  // The break statement is the last statement in its case statement iff:
  // it is the last statement in the switch, or,
  // its next statement is either 'default' or 'case'
  return iter == flatSwitch.end() || isa<CaseStmt>(*iter) ||
         isa<DefaultStmt>(*iter);
}

const Stmt *SPIRVEmitter::breakStmtScope(const BreakStmt *breakStmt) {
  const Stmt *curNode = breakStmt;
  do {
    curNode = getImmediateParent(astContext, curNode);
  } while (curNode && !isLoopStmt(curNode) && !isa<SwitchStmt>(curNode));

  return curNode;
}

void SPIRVEmitter::doBreakStmt(const BreakStmt *breakStmt) {
  assert(!theBuilder.isCurrentBasicBlockTerminated());
  uint32_t breakTargetBB = breakStack.top();
  theBuilder.addSuccessor(breakTargetBB);
  theBuilder.createBranch(breakTargetBB);

  // If any statements follow a break statement in a loop or switch, they will
  // not be executed. For example, StmtB and StmtC below are never executed:
  //
  // while (true) {
  //   StmtA;
  //   break;
  //   StmtB;
  //   StmtC;
  // }
  //
  // To handle such cases, we do not stop tranlsation. We create a new basic
  // block in which StmtB and StmtC will be translated.
  // Note that since this basic block is unreachable, BlockReadableOrderVisitor
  // will not emit it in the final module binary.
  const Stmt *scope = breakStmtScope(breakStmt);
  if (
      // Have unreachable instructions after a break statement in a case of a
      // switch statement
      (isa<SwitchStmt>(scope) &&
       !breakStmtIsLastStmtInCaseStmt(breakStmt,
                                      dyn_cast<SwitchStmt>(scope))) ||
      // Have unreachable instructions after a break statement in a loop
      // statement
      (isLoopStmt(scope) &&
       !isLastStmtBeforeControlFlowBranching(astContext, breakStmt))) {
    const uint32_t unreachableBB =
        theBuilder.createBasicBlock("unreachable", false);
    theBuilder.setInsertPoint(unreachableBB);
  }
}

void SPIRVEmitter::doSwitchStmt(const SwitchStmt *switchStmt,
                                llvm::ArrayRef<const Attr *> attrs) {
  // Switch statements are composed of:
  //   switch (<condition variable>) {
  //     <CaseStmt>
  //     <CaseStmt>
  //     <CaseStmt>
  //     <DefaultStmt> (optional)
  //   }
  //
  //                             +-------+
  //                             | check |
  //                             +-------+
  //                                 |
  //         +-------+-------+----------------+---------------+
  //         | 1             | 2              | 3             | (others)
  //         v               v                v               v
  //     +-------+      +-------------+     +-------+     +------------+
  //     | case1 |      | case2       |     | case3 | ... | default    |
  //     |       |      |(fallthrough)|---->|       |     | (optional) |
  //     +-------+      |+------------+     +-------+     +------------+
  //         |                                  |                |
  //         |                                  |                |
  //         |   +-------+                      |                |
  //         |   |       | <--------------------+                |
  //         +-> | merge |                                       |
  //             |       | <-------------------------------------+
  //             +-------+

  // If no attributes are given, or if "forcecase" attribute was provided,
  // we'll do our best to use OpSwitch if possible.
  // If any of the cases compares to a variable (rather than an integer
  // literal), we cannot use OpSwitch because OpSwitch expects literal
  // numbers as parameters.
  const bool isAttrForceCase =
      !attrs.empty() && attrs.front()->getKind() == attr::HLSLForceCase;
  const bool canUseSpirvOpSwitch =
      (attrs.empty() || isAttrForceCase) &&
      allSwitchCasesAreIntegerLiterals(switchStmt->getBody());

  if (isAttrForceCase && !canUseSpirvOpSwitch)
    emitWarning("Ignored 'forcecase' attribute for the switch statement "
                "since one or more case values are not integer literals.");

  if (canUseSpirvOpSwitch)
    processSwitchStmtUsingSpirvOpSwitch(switchStmt);
  else
    processSwitchStmtUsingIfStmts(switchStmt);
}

uint32_t SPIRVEmitter::doBinaryOperator(const BinaryOperator *expr) {
  const auto opcode = expr->getOpcode();

  // Handle assignment first since we need to evaluate rhs before lhs.
  // For other binary operations, we need to evaluate lhs before rhs.
  if (opcode == BO_Assign)
    return processAssignment(expr->getLHS(), doExpr(expr->getRHS()), false);

  // Try to optimize floatMxN * float and floatN * float case
  if (opcode == BO_Mul) {
    if (const uint32_t result = tryToGenFloatMatrixScale(expr))
      return result;
    if (const uint32_t result = tryToGenFloatVectorScale(expr))
      return result;
  }

  const uint32_t resultType = typeTranslator.translateType(expr->getType());
  return processBinaryOp(expr->getLHS(), expr->getRHS(), opcode, resultType);
}

uint32_t SPIRVEmitter::doCallExpr(const CallExpr *callExpr) {
  if (const auto *operatorCall = dyn_cast<CXXOperatorCallExpr>(callExpr))
    return doCXXOperatorCallExpr(operatorCall);

  const FunctionDecl *callee = callExpr->getDirectCallee();

  // Intrinsic functions such as 'dot' or 'mul'
  if (hlsl::IsIntrinsicOp(callee)) {
    return processIntrinsicCallExpr(callExpr);
  }

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
      const uint32_t varType = typeTranslator.translateType(arg->getType());
      const uint32_t tempVarId = theBuilder.addFnVar(varType);
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

uint32_t SPIRVEmitter::doCastExpr(const CastExpr *expr) {
  const Expr *subExpr = expr->getSubExpr();
  const QualType toType = expr->getType();

  switch (expr->getCastKind()) {
  case CastKind::CK_LValueToRValue: {
    const uint32_t fromValue = doExpr(subExpr);
    if (isVectorShuffle(subExpr) || isa<ExtMatrixElementExpr>(subExpr)) {
      // By reaching here, it means the vector/matrix element accessing
      // operation is an lvalue. For vector element accessing, if we generated
      // a vector shuffle for it and trying to use it as a rvalue, we cannot
      // do the load here as normal. Need the upper nodes in the AST tree to
      // handle it properly. For matrix element accessing, load should have
      // already happened after creating access chain for each element.
      return fromValue;
    }

    // Using lvalue as rvalue means we need to OpLoad the contents from
    // the parameter/variable first.
    const uint32_t resultType = typeTranslator.translateType(toType);
    return theBuilder.createLoad(resultType, fromValue);
  }
  case CastKind::CK_NoOp:
    return doExpr(subExpr);
  case CastKind::CK_IntegralCast:
  case CastKind::CK_FloatingToIntegral:
  case CastKind::CK_HLSLCC_IntegralCast:
  case CastKind::CK_HLSLCC_FloatingToIntegral: {
    // Integer literals in the AST are represented using 64bit APInt
    // themselves and then implicitly casted into the expected bitwidth.
    // We need special treatment of integer literals here because generating
    // a 64bit constant and then explicit casting in SPIR-V requires Int64
    // capability. We should avoid introducing unnecessary capabilities to
    // our best.
    llvm::APSInt intValue;
    if (expr->EvaluateAsInt(intValue, astContext, Expr::SE_NoSideEffects)) {
      return translateAPInt(intValue, toType);
    }

    return castToInt(doExpr(subExpr), subExpr->getType(), toType);
  }
  case CastKind::CK_FloatingCast:
  case CastKind::CK_IntegralToFloating:
  case CastKind::CK_HLSLCC_FloatingCast:
  case CastKind::CK_HLSLCC_IntegralToFloating: {
    // First try to see if we can do constant folding for floating point
    // numbers like what we are doing for integers in the above.
    Expr::EvalResult evalResult;
    if (expr->EvaluateAsRValue(evalResult, astContext) &&
        !evalResult.HasSideEffects) {
      return translateAPFloat(evalResult.Val.getFloat(), toType);
    }

    return castToFloat(doExpr(subExpr), subExpr->getType(), toType);
  }
  case CastKind::CK_IntegralToBoolean:
  case CastKind::CK_FloatingToBoolean:
  case CastKind::CK_HLSLCC_IntegralToBoolean:
  case CastKind::CK_HLSLCC_FloatingToBoolean: {
    // First try to see if we can do constant folding.
    bool boolVal;
    if (!expr->HasSideEffects(astContext) &&
        expr->EvaluateAsBooleanCondition(boolVal, astContext)) {
      return theBuilder.getConstantBool(boolVal);
    }

    return castToBool(doExpr(subExpr), subExpr->getType(), toType);
  }
  case CastKind::CK_HLSLVectorSplat: {
    const size_t size = hlsl::GetHLSLVecSize(expr->getType());

    return createVectorSplat(subExpr, size);
  }
  case CastKind::CK_HLSLVectorTruncationCast: {
    const uint32_t toVecTypeId = typeTranslator.translateType(toType);
    const uint32_t elemTypeId =
        typeTranslator.translateType(hlsl::GetHLSLVecElementType(toType));
    const auto toSize = hlsl::GetHLSLVecSize(toType);

    const uint32_t composite = doExpr(subExpr);
    llvm::SmallVector<uint32_t, 4> elements;

    for (uint32_t i = 0; i < toSize; ++i) {
      elements.push_back(
          theBuilder.createCompositeExtract(elemTypeId, composite, {i}));
    }

    if (toSize == 1) {
      return elements.front();
    }

    return theBuilder.createCompositeConstruct(toVecTypeId, elements);
  }
  case CastKind::CK_HLSLVectorToScalarCast: {
    // The underlying should already be a vector of size 1.
    assert(hlsl::GetHLSLVecSize(subExpr->getType()) == 1);
    return doExpr(subExpr);
  }
  case CastKind::CK_HLSLVectorToMatrixCast: {
    // The target type should already be a 1xN matrix type.
    assert(TypeTranslator::is1xNMatrix(toType));
    return doExpr(subExpr);
  }
  case CastKind::CK_HLSLMatrixSplat: {
    // From scalar to matrix
    uint32_t rowCount = 0, colCount = 0;
    hlsl::GetHLSLMatRowColCount(toType, rowCount, colCount);

    // Handle degenerated cases first
    if (rowCount == 1 && colCount == 1)
      return doExpr(subExpr);

    if (colCount == 1)
      return createVectorSplat(subExpr, rowCount);

    bool isConstVec = false;
    const uint32_t vecSplat = createVectorSplat(subExpr, colCount, &isConstVec);
    if (rowCount == 1)
      return vecSplat;

    const uint32_t matType = typeTranslator.translateType(toType);
    llvm::SmallVector<uint32_t, 4> vectors(size_t(rowCount), vecSplat);

    if (isConstVec) {
      return theBuilder.getConstantComposite(matType, vectors);
    } else {
      return theBuilder.createCompositeConstruct(matType, vectors);
    }
  }
  case CastKind::CK_HLSLMatrixToScalarCast: {
    // The underlying should already be a matrix of 1x1.
    assert(TypeTranslator::is1x1Matrix(subExpr->getType()));
    return doExpr(subExpr);
  }
  case CastKind::CK_HLSLMatrixToVectorCast: {
    // The underlying should already be a matrix of 1xN.
    assert(TypeTranslator::is1xNMatrix(subExpr->getType()) ||
           TypeTranslator::isMx1Matrix(subExpr->getType()));
    return doExpr(subExpr);
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

uint32_t
SPIRVEmitter::doCompoundAssignOperator(const CompoundAssignOperator *expr) {
  const auto opcode = expr->getOpcode();

  // Try to optimize floatMxN *= float and floatN *= float case
  if (opcode == BO_MulAssign) {
    if (const uint32_t result = tryToGenFloatMatrixScale(expr))
      return result;
    if (const uint32_t result = tryToGenFloatVectorScale(expr))
      return result;
  }

  const auto *rhs = expr->getRHS();
  const auto *lhs = expr->getLHS();

  uint32_t lhsPtr = 0;
  const uint32_t resultType = typeTranslator.translateType(expr->getType());
  const uint32_t result =
      processBinaryOp(lhs, rhs, opcode, resultType, &lhsPtr);
  return processAssignment(lhs, result, true, lhsPtr);
}

uint32_t SPIRVEmitter::doConditionalOperator(const ConditionalOperator *expr) {
  // According to HLSL doc, all sides of the ?: expression are always
  // evaluated.
  const uint32_t type = typeTranslator.translateType(expr->getType());
  const uint32_t condition = doExpr(expr->getCond());
  const uint32_t trueBranch = doExpr(expr->getTrueExpr());
  const uint32_t falseBranch = doExpr(expr->getFalseExpr());

  return theBuilder.createSelect(type, condition, trueBranch, falseBranch);
}

uint32_t SPIRVEmitter::doCXXOperatorCallExpr(const CXXOperatorCallExpr *expr) {
  { // First try to handle vector/matrix indexing
    const Expr *baseExpr = nullptr;
    const Expr *index0Expr = nullptr;
    const Expr *index1Expr = nullptr;

    if (isVecMatIndexing(expr, &baseExpr, &index0Expr, &index1Expr)) {
      const auto baseType = baseExpr->getType();
      llvm::SmallVector<uint32_t, 2> indices;

      if (hlsl::IsHLSLMatType(baseType)) {
        uint32_t rowCount = 0, colCount = 0;
        hlsl::GetHLSLMatRowColCount(baseType, rowCount, colCount);

        // Collect indices for this matrix indexing
        if (rowCount > 1) {
          indices.push_back(doExpr(index0Expr));
        }
        // Evalute index1Expr iff it is not nullptr
        if (colCount > 1 && index1Expr) {
          indices.push_back(doExpr(index1Expr));
        }
      } else { // Indexing into vector
        if (hlsl::GetHLSLVecSize(baseType) > 1) {
          indices.push_back(doExpr(index0Expr));
        }
      }

      if (indices.size() == 0) {
        return doExpr(baseExpr);
      }

      uint32_t base = doExpr(baseExpr);
      // If we are indexing into a rvalue, to use OpAccessChain, we first need
      // to create a local variable to hold the rvalue.
      //
      // TODO: We can optimize the codegen by emitting OpCompositeExtract if
      // all indices are contant integers.
      if (!baseExpr->isGLValue()) {
        const uint32_t compositeType = typeTranslator.translateType(baseType);
        const uint32_t tempVar = theBuilder.addFnVar(compositeType, "temp.var");
        theBuilder.createStore(tempVar, base);
        base = tempVar;
      }

      const uint32_t elemType = typeTranslator.translateType(expr->getType());
      const uint32_t ptrType = theBuilder.getPointerType(
          elemType, declIdMapper.resolveStorageClass(baseExpr));

      return theBuilder.createAccessChain(ptrType, base, indices);
    }
  }

  emitError("unimplemented C++ operator call: %0") << expr->getOperator();
  expr->dump();
  return 0;
}

uint32_t
SPIRVEmitter::doExtMatrixElementExpr(const ExtMatrixElementExpr *expr) {
  const Expr *baseExpr = expr->getBase();
  const uint32_t base = doExpr(baseExpr);
  const auto accessor = expr->getEncodedElementAccess();
  const uint32_t elemType = typeTranslator.translateType(
      hlsl::GetHLSLMatElementType(baseExpr->getType()));

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(baseExpr->getType(), rowCount, colCount);

  // Construct a temporary vector out of all elements accessed:
  // 1. Create access chain for each element using OpAccessChain
  // 2. Load each element using OpLoad
  // 3. Create the vector using OpCompositeConstruct

  llvm::SmallVector<uint32_t, 4> elements;
  for (uint32_t i = 0; i < accessor.Count; ++i) {
    uint32_t row = 0, col = 0, elem = 0;
    accessor.GetPosition(i, &row, &col);

    llvm::SmallVector<uint32_t, 2> indices;
    // If the matrix only has one row/column, we are indexing into a vector
    // then. Only one index is needed for such cases.
    if (rowCount > 1)
      indices.push_back(row);
    if (colCount > 1)
      indices.push_back(col);

    if (baseExpr->isGLValue()) {
      for (uint32_t i = 0; i < indices.size(); ++i)
        indices[i] = theBuilder.getConstantInt32(indices[i]);

      const uint32_t ptrType = theBuilder.getPointerType(
          elemType, declIdMapper.resolveStorageClass(baseExpr));
      if (!indices.empty()) {
        // Load the element via access chain
        elem = theBuilder.createAccessChain(ptrType, base, indices);
      } else {
        // The matrix is of size 1x1. No need to use access chain, base should
        // be the source pointer.
        elem = base;
      }
      elem = theBuilder.createLoad(elemType, elem);
    } else { // e.g., (mat1 + mat2)._m11
      elem = theBuilder.createCompositeExtract(elemType, base, indices);
    }
    elements.push_back(elem);
  }

  if (elements.size() == 1)
    return elements.front();

  const uint32_t vecType = theBuilder.getVecType(elemType, elements.size());
  return theBuilder.createCompositeConstruct(vecType, elements);
}

uint32_t
SPIRVEmitter::doHLSLVectorElementExpr(const HLSLVectorElementExpr *expr) {
  const Expr *baseExpr = nullptr;
  hlsl::VectorMemberAccessPositions accessor;
  condenseVectorElementExpr(expr, &baseExpr, &accessor);

  const QualType baseType = baseExpr->getType();
  assert(hlsl::IsHLSLVecType(baseType));
  const auto baseSize = hlsl::GetHLSLVecSize(baseType);

  const uint32_t type = typeTranslator.translateType(expr->getType());
  const auto accessorSize = accessor.Count;

  // Depending on the number of elements selected, we emit different
  // instructions.
  // For vectors of size greater than 1, if we are only selecting one element,
  // typical access chain or composite extraction should be fine. But if we
  // are selecting more than one elements, we must resolve to vector specific
  // operations.
  // For size-1 vectors, if we are selecting their single elements multiple
  // times, we need composite construct instructions.

  if (accessorSize == 1) {
    if (baseSize == 1) {
      // Selecting one element from a size-1 vector. The underlying vector is
      // already treated as a scalar.
      return doExpr(baseExpr);
    }

    // If the base is an lvalue, we should emit an access chain instruction
    // so that we can load/store the specified element. For rvalue base,
    // we should use composite extraction. We should check the immediate base
    // instead of the original base here since we can have something like
    // v.xyyz to turn a lvalue v into rvalue.
    if (expr->getBase()->isGLValue()) { // E.g., v.x;
      const uint32_t ptrType = theBuilder.getPointerType(
          type, declIdMapper.resolveStorageClass(baseExpr));
      const uint32_t index = theBuilder.getConstantInt32(accessor.Swz0);
      // We need a lvalue here. Do not try to load.
      return theBuilder.createAccessChain(ptrType, doExpr(baseExpr), {index});
    } else { // E.g., (v + w).x;
      // The original base vector may not be a rvalue. Need to load it if
      // it is lvalue since ImplicitCastExpr (LValueToRValue) will be missing
      // for that case.
      return theBuilder.createCompositeExtract(type, loadIfGLValue(baseExpr),
                                               {accessor.Swz0});
    }
  }

  if (baseSize == 1) {
    // Selecting one element from a size-1 vector. Construct the vector.
    llvm::SmallVector<uint32_t, 4> components(static_cast<size_t>(accessorSize),
                                              loadIfGLValue(baseExpr));
    return theBuilder.createCompositeConstruct(type, components);
  }

  llvm::SmallVector<uint32_t, 4> selectors;
  selectors.resize(accessorSize);
  // Whether we are selecting elements in the original order
  bool originalOrder = baseSize == accessorSize;
  for (uint32_t i = 0; i < accessorSize; ++i) {
    accessor.GetPosition(i, &selectors[i]);
    // We can select more elements than the vector provides. This handles
    // that case too.
    originalOrder &= selectors[i] == i;
  }

  if (originalOrder)
    return doExpr(baseExpr);

  const uint32_t baseVal = loadIfGLValue(baseExpr);
  // Use base for both vectors. But we are only selecting values from the
  // first one.
  return theBuilder.createVectorShuffle(type, baseVal, baseVal, selectors);
}

uint32_t SPIRVEmitter::doInitListExpr(const InitListExpr *expr) {
  // First try to evaluate the expression as constant expression
  Expr::EvalResult evalResult;
  if (expr->EvaluateAsRValue(evalResult, astContext) &&
      !evalResult.HasSideEffects) {
    return translateAPValue(evalResult.Val, expr->getType());
  }

  return InitListHandler(*this).process(expr);
}

uint32_t SPIRVEmitter::doUnaryOperator(const UnaryOperator *expr) {
  const auto opcode = expr->getOpcode();
  const auto *subExpr = expr->getSubExpr();
  const auto subType = subExpr->getType();
  const auto subValue = doExpr(subExpr);
  const auto subTypeId = typeTranslator.translateType(subType);

  switch (opcode) {
  case UO_PreInc:
  case UO_PreDec:
  case UO_PostInc:
  case UO_PostDec: {
    const bool isPre = opcode == UO_PreInc || opcode == UO_PreDec;
    const bool isInc = opcode == UO_PreInc || opcode == UO_PostInc;

    const spv::Op spvOp = translateOp(isInc ? BO_Add : BO_Sub, subType);
    const uint32_t originValue = theBuilder.createLoad(subTypeId, subValue);
    const uint32_t one = hlsl::IsHLSLMatType(subType)
                             ? getMatElemValueOne(subType)
                             : getValueOne(subType);
    uint32_t incValue = 0;
    if (TypeTranslator::isSpirvAcceptableMatrixType(subType)) {
      // For matrices, we can only increment/decrement each vector of it.
      const auto actOnEachVec = [this, spvOp, one](
          uint32_t /*index*/, uint32_t vecType, uint32_t lhsVec) {
        return theBuilder.createBinaryOp(spvOp, vecType, lhsVec, one);
      };
      incValue = processEachVectorInMatrix(subExpr, originValue, actOnEachVec);
    } else {
      incValue = theBuilder.createBinaryOp(spvOp, subTypeId, originValue, one);
    }
    theBuilder.createStore(subValue, incValue);

    // Prefix increment/decrement operator returns a lvalue, while postfix
    // increment/decrement returns a rvalue.
    return isPre ? subValue : originValue;
  }
  case UO_Not:
    return theBuilder.createUnaryOp(spv::Op::OpNot, subTypeId, subValue);
  case UO_LNot:
    // Parsing will do the necessary casting to make sure we are applying the
    // ! operator on boolean values.
    return theBuilder.createUnaryOp(spv::Op::OpLogicalNot, subTypeId, subValue);
  case UO_Plus:
    // No need to do anything for the prefix + operator.
    return subValue;
  case UO_Minus: {
    // SPIR-V have two opcodes for negating values: OpSNegate and OpFNegate.
    const spv::Op spvOp = isFloatOrVecOfFloatType(subType) ? spv::Op::OpFNegate
                                                           : spv::Op::OpSNegate;
    return theBuilder.createUnaryOp(spvOp, subTypeId, subValue);
  }
  default:
    break;
  }

  emitError("unary operator '%0' unimplemented yet")
      << expr->getOpcodeStr(opcode);
  expr->dump();
  return 0;
}

spv::Op SPIRVEmitter::translateOp(BinaryOperator::Opcode op, QualType type) {
  const bool isSintType = isSintOrVecMatOfSintType(type);
  const bool isUintType = isUintOrVecMatOfUintType(type);
  const bool isFloatType = isFloatOrVecMatOfFloatType(type);

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

#define BIN_OP_CASE_SINT_UINT(kind, sintBinOp, uintBinOp)                      \
  \
case BO_##kind : {                                                             \
    if (isSintType) {                                                          \
      return spv::Op::Op##sintBinOp;                                           \
    }                                                                          \
    if (isUintType) {                                                          \
      return spv::Op::Op##uintBinOp;                                           \
    }                                                                          \
  }                                                                            \
  break

  switch (op) {
  case BO_EQ: {
    if (isBoolOrVecMatOfBoolType(type))
      return spv::Op::OpLogicalEqual;
    if (isSintType || isUintType)
      return spv::Op::OpIEqual;
    if (isFloatType)
      return spv::Op::OpFOrdEqual;
  } break;
  case BO_NE: {
    if (isBoolOrVecMatOfBoolType(type))
      return spv::Op::OpLogicalNotEqual;
    if (isSintType || isUintType)
      return spv::Op::OpINotEqual;
    if (isFloatType)
      return spv::Op::OpFOrdNotEqual;
  } break;
  // According to HLSL doc, all sides of the && and || expression are always
  // evaluated.
  case BO_LAnd:
    return spv::Op::OpLogicalAnd;
  case BO_LOr:
    return spv::Op::OpLogicalOr;
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
    BIN_OP_CASE_SINT_UINT(And, BitwiseAnd, BitwiseAnd);
    BIN_OP_CASE_SINT_UINT(AndAssign, BitwiseAnd, BitwiseAnd);
    BIN_OP_CASE_SINT_UINT(Or, BitwiseOr, BitwiseOr);
    BIN_OP_CASE_SINT_UINT(OrAssign, BitwiseOr, BitwiseOr);
    BIN_OP_CASE_SINT_UINT(Xor, BitwiseXor, BitwiseXor);
    BIN_OP_CASE_SINT_UINT(XorAssign, BitwiseXor, BitwiseXor);
    BIN_OP_CASE_SINT_UINT(Shl, ShiftLeftLogical, ShiftLeftLogical);
    BIN_OP_CASE_SINT_UINT(ShlAssign, ShiftLeftLogical, ShiftLeftLogical);
    BIN_OP_CASE_SINT_UINT(Shr, ShiftRightArithmetic, ShiftRightLogical);
    BIN_OP_CASE_SINT_UINT(ShrAssign, ShiftRightArithmetic, ShiftRightLogical);
  default:
    break;
  }

#undef BIN_OP_CASE_INT_FLOAT
#undef BIN_OP_CASE_SINT_UINT_FLOAT
#undef BIN_OP_CASE_SINT_UINT

  emitError("translating binary operator '%0' unimplemented")
      << BinaryOperator::getOpcodeStr(op);
  return spv::Op::OpNop;
}

uint32_t SPIRVEmitter::processAssignment(const Expr *lhs, const uint32_t rhs,
                                         bool isCompoundAssignment,
                                         uint32_t lhsPtr) {
  // Assigning to vector swizzling should be handled differently.
  if (const uint32_t result = tryToAssignToVectorElements(lhs, rhs)) {
    return result;
  }
  // Assigning to matrix swizzling should be handled differently.
  if (const uint32_t result = tryToAssignToMatrixElements(lhs, rhs)) {
    return result;
  }

  // Normal assignment procedure
  if (lhsPtr == 0)
    lhsPtr = doExpr(lhs);

  theBuilder.createStore(lhsPtr, rhs);
  // Plain assignment returns a rvalue, while compound assignment returns
  // lvalue.
  return isCompoundAssignment ? lhsPtr : rhs;
}

uint32_t SPIRVEmitter::processBinaryOp(const Expr *lhs, const Expr *rhs,
                                       const BinaryOperatorKind opcode,
                                       const uint32_t resultType,
                                       uint32_t *lhsResultId,
                                       const spv::Op mandateGenOpcode) {
  // If the operands are of matrix type, we need to dispatch the operation
  // onto each element vector iff the operands are not degenerated matrices
  // and we don't have a matrix specific SPIR-V instruction for the operation.
  if (!isSpirvMatrixOp(mandateGenOpcode) &&
      TypeTranslator::isSpirvAcceptableMatrixType(lhs->getType())) {
    return processMatrixBinaryOp(lhs, rhs, opcode);
  }

  const spv::Op spvOp = (mandateGenOpcode == spv::Op::Max)
                            ? translateOp(opcode, lhs->getType())
                            : mandateGenOpcode;

  uint32_t rhsVal, lhsPtr, lhsVal;
  if (BinaryOperator::isCompoundAssignmentOp(opcode)) {
    // Evalute rhs before lhs
    rhsVal = doExpr(rhs);
    lhsVal = lhsPtr = doExpr(lhs);
    // This is a compound assignment. We need to load the lhs value if lhs
    // does not generate a vector shuffle.
    if (!isVectorShuffle(lhs)) {
      const uint32_t lhsTy = typeTranslator.translateType(lhs->getType());
      lhsVal = theBuilder.createLoad(lhsTy, lhsPtr);
    }
  } else {
    // Evalute lhs before rhs
    lhsVal = lhsPtr = doExpr(lhs);
    rhsVal = doExpr(rhs);
  }

  if (lhsResultId)
    *lhsResultId = lhsPtr;

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
  case BO_NE:
  case BO_And:
  case BO_Or:
  case BO_Xor:
  case BO_Shl:
  case BO_Shr:
  case BO_LAnd:
  case BO_LOr:
  case BO_AddAssign:
  case BO_SubAssign:
  case BO_MulAssign:
  case BO_DivAssign:
  case BO_RemAssign:
  case BO_AndAssign:
  case BO_OrAssign:
  case BO_XorAssign:
  case BO_ShlAssign:
  case BO_ShrAssign:
    return theBuilder.createBinaryOp(spvOp, resultType, lhsVal, rhsVal);
  case BO_Assign:
    llvm_unreachable("assignment should not be handled here");
  default:
    break;
  }

  emitError("BinaryOperator '%0' is not supported yet.")
      << BinaryOperator::getOpcodeStr(opcode);
  return 0;
}

void SPIRVEmitter::initOnce(std::string varName, uint32_t varPtr,
                            const Expr *varInit) {
  const uint32_t boolType = theBuilder.getBoolType();
  varName = "init.done." + varName;

  // Create a file/module visible variable to hold the initialization state.
  const uint32_t initDoneVar = theBuilder.addFileVar(
      boolType, varName, theBuilder.getConstantBool(false));

  const uint32_t condition = theBuilder.createLoad(boolType, initDoneVar);

  const uint32_t thenBB = theBuilder.createBasicBlock("if.true");
  const uint32_t mergeBB = theBuilder.createBasicBlock("if.merge");

  theBuilder.createConditionalBranch(condition, thenBB, mergeBB, mergeBB);
  theBuilder.addSuccessor(thenBB);
  theBuilder.addSuccessor(mergeBB);
  theBuilder.setMergeTarget(mergeBB);

  theBuilder.setInsertPoint(thenBB);
  // Do initialization and mark done
  theBuilder.createStore(varPtr, doExpr(varInit));
  theBuilder.createStore(initDoneVar, theBuilder.getConstantBool(true));
  theBuilder.createBranch(mergeBB);
  theBuilder.addSuccessor(mergeBB);

  theBuilder.setInsertPoint(mergeBB);
}

bool SPIRVEmitter::isVectorShuffle(const Expr *expr) {
  // TODO: the following check is essentially duplicated from
  // doHLSLVectorElementExpr. Should unify them.
  if (const auto *vecElemExpr = dyn_cast<HLSLVectorElementExpr>(expr)) {
    const Expr *base = nullptr;
    hlsl::VectorMemberAccessPositions accessor;
    condenseVectorElementExpr(vecElemExpr, &base, &accessor);

    const auto accessorSize = accessor.Count;
    if (accessorSize == 1) {
      // Selecting only one element. OpAccessChain or OpCompositeExtract for
      // such cases.
      return false;
    }

    const auto baseSize = hlsl::GetHLSLVecSize(base->getType());
    if (accessorSize != baseSize)
      return true;

    for (uint32_t i = 0; i < accessorSize; ++i) {
      uint32_t position;
      accessor.GetPosition(i, &position);
      if (position != i)
        return true;
    }

    // Selecting exactly the original vector. No vector shuffle generated.
    return false;
  }

  return false;
}

bool SPIRVEmitter::isVecMatIndexing(const CXXOperatorCallExpr *vecIndexExpr,
                                    const Expr **base, const Expr **index0,
                                    const Expr **index1) {
  // Must be operator[]
  if (vecIndexExpr->getOperator() != OverloadedOperatorKind::OO_Subscript)
    return false;

  // Get the base of this outer operator[]
  const Expr *vecBase = vecIndexExpr->getArg(0);
  // If the base of the outer operator[] is a vector, try to see if we have
  // another inner operator[] on a matrix, i.e., two levels of indexing into
  // the matrix.
  if (hlsl::IsHLSLVecType(vecBase->getType())) {
    const auto *matIndexExpr = dyn_cast<CXXOperatorCallExpr>(vecBase);

    if (!matIndexExpr) {
      // No inner operator[]. So this is just indexing into a vector.
      *base = vecBase;
      *index0 = vecIndexExpr->getArg(1);
      *index1 = nullptr;

      return true;
    }

    // Must be operator[]
    if (matIndexExpr->getOperator() != OverloadedOperatorKind::OO_Subscript)
      return false;

    // Get the base of this inner operator[]
    const Expr *matBase = matIndexExpr->getArg(0);
    if (!hlsl::IsHLSLMatType(matBase->getType()))
      return false;

    *base = matBase;
    *index0 = matIndexExpr->getArg(1);
    *index1 = vecIndexExpr->getArg(1);

    return true;
  }

  // The base of the outside operator[] is not a vector. Try to see whether it
  // is a matrix. If true, it means we have only one level of indexing.
  if (hlsl::IsHLSLMatType(vecBase->getType())) {
    *base = vecBase;
    *index0 = vecIndexExpr->getArg(1);
    *index1 = nullptr;

    return true;
  }

  return false;
}

void SPIRVEmitter::condenseVectorElementExpr(
    const HLSLVectorElementExpr *expr, const Expr **basePtr,
    hlsl::VectorMemberAccessPositions *flattenedAccessor) {
  llvm::SmallVector<hlsl::VectorMemberAccessPositions, 2> accessors;
  accessors.push_back(expr->getEncodedElementAccess());

  // Recursively descending until we find the true base vector. In the
  // meanwhile, collecting accessors in the reverse order.
  *basePtr = expr->getBase();
  while (const auto *vecElemBase = dyn_cast<HLSLVectorElementExpr>(*basePtr)) {
    accessors.push_back(vecElemBase->getEncodedElementAccess());
    *basePtr = vecElemBase->getBase();
  }

  *flattenedAccessor = accessors.back();
  for (int32_t i = accessors.size() - 2; i >= 0; --i) {
    const auto &currentAccessor = accessors[i];

    // Apply the current level of accessor to the flattened accessor of all
    // previous levels of ones.
    hlsl::VectorMemberAccessPositions combinedAccessor;
    for (uint32_t j = 0; j < currentAccessor.Count; ++j) {
      uint32_t currentPosition = 0;
      currentAccessor.GetPosition(j, &currentPosition);
      uint32_t previousPosition = 0;
      flattenedAccessor->GetPosition(currentPosition, &previousPosition);
      combinedAccessor.SetPosition(j, previousPosition);
    }
    combinedAccessor.Count = currentAccessor.Count;
    combinedAccessor.IsValid =
        flattenedAccessor->IsValid && currentAccessor.IsValid;

    *flattenedAccessor = combinedAccessor;
  }
}

uint32_t SPIRVEmitter::createVectorSplat(const Expr *scalarExpr, uint32_t size,
                                         bool *resultIsConstant) {
  bool isConstVal = false;
  uint32_t scalarVal = 0;

  // Try to evaluate the element as constant first. If successful, then we
  // can generate constant instructions for this vector splat.
  Expr::EvalResult evalResult;
  if (scalarExpr->EvaluateAsRValue(evalResult, astContext) &&
      !evalResult.HasSideEffects) {
    isConstVal = true;
    scalarVal = translateAPValue(evalResult.Val, scalarExpr->getType());
  } else {
    scalarVal = doExpr(scalarExpr);
  }

  if (resultIsConstant)
    *resultIsConstant = isConstVal;

  // Just return the scalar value for vector splat with size 1
  if (size == 1)
    return scalarVal;

  const uint32_t vecType = theBuilder.getVecType(
      typeTranslator.translateType(scalarExpr->getType()), size);
  llvm::SmallVector<uint32_t, 4> elements(size_t(size), scalarVal);

  if (isConstVal) {
    return theBuilder.getConstantComposite(vecType, elements);
  } else {
    return theBuilder.createCompositeConstruct(vecType, elements);
  }
}

uint32_t SPIRVEmitter::tryToGenFloatVectorScale(const BinaryOperator *expr) {
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
        const uint32_t vecType = typeTranslator.translateType(expr->getType());
        if (isa<CompoundAssignOperator>(expr)) {
          uint32_t lhsPtr = 0;
          const uint32_t result =
              processBinaryOp(lhs, cast->getSubExpr(), expr->getOpcode(),
                              vecType, &lhsPtr, spv::Op::OpVectorTimesScalar);
          return processAssignment(lhs, result, true, lhsPtr);
        } else {
          return processBinaryOp(lhs, cast->getSubExpr(), expr->getOpcode(),
                                 vecType, nullptr,
                                 spv::Op::OpVectorTimesScalar);
        }
      }
    }
  }

  // scalar * vector
  if (hlsl::IsHLSLVecType(rhs->getType())) {
    if (const auto *cast = dyn_cast<ImplicitCastExpr>(lhs)) {
      if (cast->getCastKind() == CK_HLSLVectorSplat) {
        const uint32_t vecType = typeTranslator.translateType(expr->getType());
        // We need to switch the positions of lhs and rhs here because
        // OpVectorTimesScalar requires the first operand to be a vector and
        // the second to be a scalar.
        return processBinaryOp(rhs, cast->getSubExpr(), expr->getOpcode(),
                               vecType, nullptr, spv::Op::OpVectorTimesScalar);
      }
    }
  }

  return 0;
}

uint32_t SPIRVEmitter::tryToGenFloatMatrixScale(const BinaryOperator *expr) {
  const QualType type = expr->getType();
  // We can only translate floatMxN * float into OpMatrixTimesScalar.
  // So the result type must be floatMxN.
  if (!hlsl::IsHLSLMatType(type) ||
      !hlsl::GetHLSLMatElementType(type)->isFloatingType())
    return 0;

  const Expr *lhs = expr->getLHS();
  const Expr *rhs = expr->getRHS();
  const QualType lhsType = lhs->getType();
  const QualType rhsType = rhs->getType();

  const auto selectOpcode = [](const QualType ty) {
    return TypeTranslator::isMx1Matrix(ty) || TypeTranslator::is1xNMatrix(ty)
               ? spv::Op::OpVectorTimesScalar
               : spv::Op::OpMatrixTimesScalar;
  };

  // Multiplying a float matrix with a float scalar will be represented in
  // AST via a binary operation with two float matrices as operands; one of
  // the operand is from an implicit cast with kind CK_HLSLMatrixSplat.

  // matrix * scalar
  if (hlsl::IsHLSLMatType(lhsType)) {
    if (const auto *cast = dyn_cast<ImplicitCastExpr>(rhs)) {
      if (cast->getCastKind() == CK_HLSLMatrixSplat) {
        const uint32_t matType = typeTranslator.translateType(expr->getType());
        const spv::Op opcode = selectOpcode(lhsType);
        if (isa<CompoundAssignOperator>(expr)) {
          uint32_t lhsPtr = 0;
          const uint32_t result =
              processBinaryOp(lhs, cast->getSubExpr(), expr->getOpcode(),
                              matType, &lhsPtr, opcode);
          return processAssignment(lhs, result, true, lhsPtr);
        } else {
          return processBinaryOp(lhs, cast->getSubExpr(), expr->getOpcode(),
                                 matType, nullptr, opcode);
        }
      }
    }
  }

  // scalar * matrix
  if (hlsl::IsHLSLMatType(rhsType)) {
    if (const auto *cast = dyn_cast<ImplicitCastExpr>(lhs)) {
      if (cast->getCastKind() == CK_HLSLMatrixSplat) {
        const uint32_t matType = typeTranslator.translateType(expr->getType());
        const spv::Op opcode = selectOpcode(rhsType);
        // We need to switch the positions of lhs and rhs here because
        // OpMatrixTimesScalar requires the first operand to be a matrix and
        // the second to be a scalar.
        return processBinaryOp(rhs, cast->getSubExpr(), expr->getOpcode(),
                               matType, nullptr, opcode);
      }
    }
  }

  return 0;
}

uint32_t SPIRVEmitter::tryToAssignToVectorElements(const Expr *lhs,
                                                   const uint32_t rhs) {
  // Assigning to a vector swizzling lhs is tricky if we are neither
  // writing to one element nor all elements in their original order.
  // Under such cases, we need to create a new vector swizzling involving
  // both the lhs and rhs vectors and then write the result of this swizzling
  // into the base vector of lhs.
  // For example, for vec4.yz = vec2, we nee to do the following:
  //
  //   %vec4Val = OpLoad %v4float %vec4
  //   %vec2Val = OpLoad %v2float %vec2
  //   %shuffle = OpVectorShuffle %v4float %vec4Val %vec2Val 0 4 5 3
  //   OpStore %vec4 %shuffle
  //
  // When doing the vector shuffle, we use the lhs base vector as the first
  // vector and the rhs vector as the second vector. Therefore, all elements
  // in the second vector will be selected into the shuffle result.

  const auto *lhsExpr = dyn_cast<HLSLVectorElementExpr>(lhs);

  if (!lhsExpr)
    return 0;

  if (!isVectorShuffle(lhs)) {
    // No vector shuffle needed to be generated for this assignment.
    // Should fall back to the normal handling of assignment.
    return 0;
  }

  const Expr *base = nullptr;
  hlsl::VectorMemberAccessPositions accessor;
  condenseVectorElementExpr(lhsExpr, &base, &accessor);

  const QualType baseType = base->getType();
  assert(hlsl::IsHLSLVecType(baseType));
  const auto baseSizse = hlsl::GetHLSLVecSize(baseType);

  llvm::SmallVector<uint32_t, 4> selectors;
  selectors.resize(baseSizse);
  // Assume we are selecting all original elements first.
  for (uint32_t i = 0; i < baseSizse; ++i) {
    selectors[i] = i;
  }

  // Now fix up the elements that actually got overwritten by the rhs vector.
  // Since we are using the rhs vector as the second vector, their index
  // should be offset'ed by the size of the lhs base vector.
  for (uint32_t i = 0; i < accessor.Count; ++i) {
    uint32_t position;
    accessor.GetPosition(i, &position);
    selectors[position] = baseSizse + i;
  }

  const uint32_t baseTypeId = typeTranslator.translateType(baseType);
  const uint32_t vec1 = doExpr(base);
  const uint32_t vec1Val = theBuilder.createLoad(baseTypeId, vec1);
  const uint32_t shuffle =
      theBuilder.createVectorShuffle(baseTypeId, vec1Val, rhs, selectors);

  theBuilder.createStore(vec1, shuffle);

  // TODO: OK, this return value is incorrect for compound assignments, for
  // which cases we should return lvalues. Should at least emit errors if
  // this return value is used (can be checked via ASTContext.getParents).
  return rhs;
}

uint32_t SPIRVEmitter::tryToAssignToMatrixElements(const Expr *lhs,
                                                   uint32_t rhs) {
  const auto *lhsExpr = dyn_cast<ExtMatrixElementExpr>(lhs);
  if (!lhsExpr)
    return 0;

  const Expr *baseMat = lhsExpr->getBase();
  const uint32_t base = doExpr(baseMat);
  const QualType elemType = hlsl::GetHLSLMatElementType(baseMat->getType());
  const uint32_t elemTypeId = typeTranslator.translateType(elemType);

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(baseMat->getType(), rowCount, colCount);

  // For each lhs element written to:
  // 1. Extract the corresponding rhs element using OpCompositeExtract
  // 2. Create access chain for the lhs element using OpAccessChain
  // 3. Write using OpStore

  const auto accessor = lhsExpr->getEncodedElementAccess();
  for (uint32_t i = 0; i < accessor.Count; ++i) {
    uint32_t row = 0, col = 0;
    accessor.GetPosition(i, &row, &col);

    llvm::SmallVector<uint32_t, 2> indices;
    // If the matrix only have one row/column, we are indexing into a vector
    // then. Only one index is needed for such cases.
    if (rowCount > 1)
      indices.push_back(row);
    if (colCount > 1)
      indices.push_back(col);

    for (uint32_t i = 0; i < indices.size(); ++i)
      indices[i] = theBuilder.getConstantInt32(indices[i]);

    // If we are writing to only one element, the rhs should already be a
    // scalar value.
    uint32_t rhsElem = rhs;
    if (accessor.Count > 1)
      rhsElem = theBuilder.createCompositeExtract(elemTypeId, rhs, {i});

    const uint32_t ptrType = theBuilder.getPointerType(
        elemTypeId, declIdMapper.resolveStorageClass(baseMat));

    // If the lhs is actually a matrix of size 1x1, we don't need the access
    // chain. base is already the dest pointer.
    uint32_t lhsElemPtr = base;
    if (!indices.empty()) {
      // Load the element via access chain
      lhsElemPtr = theBuilder.createAccessChain(ptrType, base, indices);
    }

    theBuilder.createStore(lhsElemPtr, rhsElem);
  }

  // TODO: OK, this return value is incorrect for compound assignments, for
  // which cases we should return lvalues. Should at least emit errors if
  // this return value is used (can be checked via ASTContext.getParents).
  return rhs;
}

uint32_t SPIRVEmitter::processEachVectorInMatrix(
    const Expr *matrix, const uint32_t matrixVal,
    llvm::function_ref<uint32_t(uint32_t, uint32_t, uint32_t)>
        actOnEachVector) {
  const auto matType = matrix->getType();
  assert(TypeTranslator::isSpirvAcceptableMatrixType(matType));
  const uint32_t vecType = typeTranslator.getComponentVectorType(matType);

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(matType, rowCount, colCount);

  llvm::SmallVector<uint32_t, 4> vectors;
  // Extract each component vector and do operation on it
  for (uint32_t i = 0; i < rowCount; ++i) {
    const uint32_t lhsVec =
        theBuilder.createCompositeExtract(vecType, matrixVal, {i});
    vectors.push_back(actOnEachVector(i, vecType, lhsVec));
  }

  // Construct the result matrix
  return theBuilder.createCompositeConstruct(
      typeTranslator.translateType(matType), vectors);
}

uint32_t SPIRVEmitter::processMatrixBinaryOp(const Expr *lhs, const Expr *rhs,
                                             const BinaryOperatorKind opcode) {
  // TODO: some code are duplicated from processBinaryOp. Try to unify them.
  const auto lhsType = lhs->getType();
  assert(TypeTranslator::isSpirvAcceptableMatrixType(lhsType));
  const spv::Op spvOp = translateOp(opcode, lhsType);

  uint32_t rhsVal, lhsPtr, lhsVal;
  if (BinaryOperator::isCompoundAssignmentOp(opcode)) {
    // Evalute rhs before lhs
    rhsVal = doExpr(rhs);
    lhsPtr = doExpr(lhs);
    const uint32_t lhsTy = typeTranslator.translateType(lhsType);
    lhsVal = theBuilder.createLoad(lhsTy, lhsPtr);
  } else {
    // Evalute lhs before rhs
    lhsVal = lhsPtr = doExpr(lhs);
    rhsVal = doExpr(rhs);
  }

  switch (opcode) {
  case BO_Add:
  case BO_Sub:
  case BO_Mul:
  case BO_Div:
  case BO_Rem:
  case BO_AddAssign:
  case BO_SubAssign:
  case BO_MulAssign:
  case BO_DivAssign:
  case BO_RemAssign: {
    const uint32_t vecType = typeTranslator.getComponentVectorType(lhsType);
    const auto actOnEachVec = [this, spvOp, rhsVal](
        uint32_t index, uint32_t vecType, uint32_t lhsVec) {
      // For each vector of lhs, we need to load the corresponding vector of
      // rhs and do the operation on them.
      const uint32_t rhsVec =
          theBuilder.createCompositeExtract(vecType, rhsVal, {index});
      return theBuilder.createBinaryOp(spvOp, vecType, lhsVec, rhsVec);

    };
    return processEachVectorInMatrix(lhs, lhsVal, actOnEachVec);
  }
  case BO_Assign:
    llvm_unreachable("assignment should not be handled here");
  default:
    break;
  }

  emitError("BinaryOperator '%0' for matrices not supported yet")
      << BinaryOperator::getOpcodeStr(opcode);
  return 0;
}

uint32_t SPIRVEmitter::castToBool(const uint32_t fromVal, QualType fromType,
                                  QualType toBoolType) {
  if (isSameScalarOrVecType(fromType, toBoolType))
    return fromVal;

  // Converting to bool means comparing with value zero.
  const spv::Op spvOp = translateOp(BO_NE, fromType);
  const uint32_t boolType = typeTranslator.translateType(toBoolType);
  const uint32_t zeroVal = getValueZero(fromType);

  return theBuilder.createBinaryOp(spvOp, boolType, fromVal, zeroVal);
}

uint32_t SPIRVEmitter::castToInt(const uint32_t fromVal, QualType fromType,
                                 QualType toIntType) {
  if (isSameScalarOrVecType(fromType, toIntType))
    return fromVal;

  const uint32_t intType = typeTranslator.translateType(toIntType);
  if (isBoolOrVecOfBoolType(fromType)) {
    const uint32_t one = getValueOne(toIntType);
    const uint32_t zero = getValueZero(toIntType);
    return theBuilder.createSelect(intType, fromVal, one, zero);
  }

  if (isSintOrVecOfSintType(fromType) || isUintOrVecOfUintType(fromType)) {
    // TODO: handle different bitwidths
    return theBuilder.createUnaryOp(spv::Op::OpBitcast, intType, fromVal);
  }

  if (isFloatOrVecOfFloatType(fromType)) {
    if (isSintOrVecOfSintType(toIntType)) {
      return theBuilder.createUnaryOp(spv::Op::OpConvertFToS, intType, fromVal);
    } else if (isUintOrVecOfUintType(toIntType)) {
      return theBuilder.createUnaryOp(spv::Op::OpConvertFToU, intType, fromVal);
    } else {
      emitError("unimplemented casting to integer from floating point");
    }
  } else {
    emitError("unimplemented casting to integer");
  }

  return 0;
}

uint32_t SPIRVEmitter::castToFloat(const uint32_t fromVal, QualType fromType,
                                   QualType toFloatType) {
  if (isSameScalarOrVecType(fromType, toFloatType))
    return fromVal;

  const uint32_t floatType = typeTranslator.translateType(toFloatType);

  if (isBoolOrVecOfBoolType(fromType)) {
    const uint32_t one = getValueOne(toFloatType);
    const uint32_t zero = getValueZero(toFloatType);
    return theBuilder.createSelect(floatType, fromVal, one, zero);
  }

  if (isSintOrVecOfSintType(fromType)) {
    return theBuilder.createUnaryOp(spv::Op::OpConvertSToF, floatType, fromVal);
  }

  if (isUintOrVecOfUintType(fromType)) {
    return theBuilder.createUnaryOp(spv::Op::OpConvertUToF, floatType, fromVal);
  }

  if (isFloatOrVecOfFloatType(fromType)) {
    emitError("casting between different fp bitwidth unimplemented");
    return 0;
  }

  emitError("unimplemented casting to floating point");
  return 0;
}

uint32_t SPIRVEmitter::processIntrinsicCallExpr(const CallExpr *callExpr) {
  const FunctionDecl *callee = callExpr->getDirectCallee();
  assert(hlsl::IsIntrinsicOp(callee) &&
         "doIntrinsicCallExpr was called for a non-intrinsic function.");

  const bool isFloatArg =
      isFloatOrVecMatOfFloatType(callExpr->getArg(0)->getType());
  GLSLstd450 glslOpcode = GLSLstd450Bad;
  bool actOnEachVecInMat = false;

#define INTRINSIC_OP_CASE(intrinsicOp, glslOp, doEachVec)                      \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = GLSLstd450::GLSLstd450##glslOp;                               \
    actOnEachVecInMat = doEachVec;                                             \
  } break

#define INTRINSIC_OP_CASE_INT_FLOAT(intrinsicOp, glslIntOp, glslFloatOp,       \
                                    doEachVec)                                 \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = isFloatArg ? GLSLstd450::GLSLstd450##glslFloatOp              \
                            : GLSLstd450::GLSLstd450##glslIntOp;               \
    actOnEachVecInMat = doEachVec;                                             \
  } break

  // Figure out which intrinsic function to translate.
  llvm::StringRef group;
  uint32_t opcode = static_cast<uint32_t>(hlsl::IntrinsicOp::Num_Intrinsics);
  hlsl::GetIntrinsicOp(callee, opcode, group);
  switch (static_cast<hlsl::IntrinsicOp>(opcode)) {
  case hlsl::IntrinsicOp::IOP_dot:
    return processIntrinsicDot(callExpr);
  case hlsl::IntrinsicOp::IOP_all:
    return processIntrinsicAllOrAny(callExpr, spv::Op::OpAll);
  case hlsl::IntrinsicOp::IOP_any:
    return processIntrinsicAllOrAny(callExpr, spv::Op::OpAny);
  case hlsl::IntrinsicOp::IOP_asfloat:
  case hlsl::IntrinsicOp::IOP_asint:
  case hlsl::IntrinsicOp::IOP_asuint:
    return processIntrinsicAsType(callExpr);
  case hlsl::IntrinsicOp::IOP_sign: {
    if (isFloatArg)
      return processIntrinsicFloatSign(callExpr);
    else
      return processIntrinsicUsingGLSLInst(callExpr,
                                           GLSLstd450::GLSLstd450SSign,
                                           /*actPerRowForMatrices*/ true);
  }
    INTRINSIC_OP_CASE(round, Round, true);
    INTRINSIC_OP_CASE_INT_FLOAT(abs, SAbs, FAbs, true);
    INTRINSIC_OP_CASE(acos, Acos, true);
    INTRINSIC_OP_CASE(asin, Asin, true);
    INTRINSIC_OP_CASE(atan, Atan, true);
    INTRINSIC_OP_CASE(ceil, Ceil, true);
    INTRINSIC_OP_CASE(cos, Cos, true);
    INTRINSIC_OP_CASE(cosh, Cosh, true);
    INTRINSIC_OP_CASE(degrees, Degrees, true);
    INTRINSIC_OP_CASE(radians, Radians, true);
    INTRINSIC_OP_CASE(determinant, Determinant, false);
    INTRINSIC_OP_CASE(exp, Exp, true);
    INTRINSIC_OP_CASE(exp2, Exp2, true);
    INTRINSIC_OP_CASE(floor, Floor, true);
    INTRINSIC_OP_CASE(length, Length, false);
    INTRINSIC_OP_CASE(log, Log, true);
    INTRINSIC_OP_CASE(log2, Log2, true);
    INTRINSIC_OP_CASE(normalize, Normalize, false);
    INTRINSIC_OP_CASE(rsqrt, InverseSqrt, true);
    INTRINSIC_OP_CASE(sin, Sin, true);
    INTRINSIC_OP_CASE(sinh, Sinh, true);
    INTRINSIC_OP_CASE(tan, Tan, true);
    INTRINSIC_OP_CASE(tanh, Tanh, true);
    INTRINSIC_OP_CASE(sqrt, Sqrt, true);
    INTRINSIC_OP_CASE(trunc, Trunc, true);
  default:
    emitError("Intrinsic function '%0' not yet implemented.")
        << callee->getName();
    return 0;
  }

#undef INTRINSIC_OP_CASE
#undef INTRINSIC_OP_CASE_INT_FLOAT

  return processIntrinsicUsingGLSLInst(callExpr, glslOpcode, actOnEachVecInMat);
}

uint32_t SPIRVEmitter::processIntrinsicDot(const CallExpr *callExpr) {
  const QualType returnType = callExpr->getType();
  const uint32_t returnTypeId =
      typeTranslator.translateType(callExpr->getType());

  // Get the function parameters. Expect 2 vectors as parameters.
  assert(callExpr->getNumArgs() == 2u);
  const Expr *arg0 = callExpr->getArg(0);
  const Expr *arg1 = callExpr->getArg(1);
  const uint32_t arg0Id = doExpr(arg0);
  const uint32_t arg1Id = doExpr(arg1);
  QualType arg0Type = arg0->getType();
  QualType arg1Type = arg1->getType();
  const size_t vec0Size = hlsl::GetHLSLVecSize(arg0Type);
  const size_t vec1Size = hlsl::GetHLSLVecSize(arg1Type);
  const QualType vec0ComponentType = hlsl::GetHLSLVecElementType(arg0Type);
  const QualType vec1ComponentType = hlsl::GetHLSLVecElementType(arg1Type);
  assert(returnType == vec1ComponentType);
  assert(vec0ComponentType == vec1ComponentType);
  assert(vec0Size == vec1Size);
  assert(vec0Size >= 1 && vec0Size <= 4);

  // According to HLSL reference, the dot function only works on integers
  // and floats.
  assert(returnType->isFloatingType() || returnType->isIntegerType());

  // Special case: dot product of two vectors, each of size 1. That is
  // basically the same as regular multiplication of 2 scalars.
  if (vec0Size == 1) {
    const spv::Op spvOp = translateOp(BO_Mul, arg0Type);
    return theBuilder.createBinaryOp(spvOp, returnTypeId, arg0Id, arg1Id);
  }

  // If the vectors are of type Float, we can use OpDot.
  if (returnType->isFloatingType()) {
    return theBuilder.createBinaryOp(spv::Op::OpDot, returnTypeId, arg0Id,
                                     arg1Id);
  }
  // Vector component type is Integer (signed or unsigned).
  // Create all instructions necessary to perform a dot product on
  // two integer vectors. SPIR-V OpDot does not support integer vectors.
  // Therefore, we use other SPIR-V instructions (addition and
  // multiplication).
  else {
    uint32_t result = 0;
    llvm::SmallVector<uint32_t, 4> multIds;
    const spv::Op multSpvOp = translateOp(BO_Mul, arg0Type);
    const spv::Op addSpvOp = translateOp(BO_Add, arg0Type);

    // Extract members from the two vectors and multiply them.
    for (unsigned int i = 0; i < vec0Size; ++i) {
      const uint32_t vec0member =
          theBuilder.createCompositeExtract(returnTypeId, arg0Id, {i});
      const uint32_t vec1member =
          theBuilder.createCompositeExtract(returnTypeId, arg1Id, {i});
      const uint32_t multId = theBuilder.createBinaryOp(multSpvOp, returnTypeId,
                                                        vec0member, vec1member);
      multIds.push_back(multId);
    }
    // Add all the multiplications.
    result = multIds[0];
    for (unsigned int i = 1; i < vec0Size; ++i) {
      const uint32_t additionId =
          theBuilder.createBinaryOp(addSpvOp, returnTypeId, result, multIds[i]);
      result = additionId;
    }
    return result;
  }
}

uint32_t SPIRVEmitter::processIntrinsicAllOrAny(const CallExpr *callExpr,
                                                spv::Op spvOp) {
  // 'all' and 'any' take only 1 parameter.
  assert(callExpr->getNumArgs() == 1u);
  const QualType returnType = callExpr->getType();
  const uint32_t returnTypeId = typeTranslator.translateType(returnType);
  const Expr *arg = callExpr->getArg(0);
  const QualType argType = arg->getType();

  // Handle scalars, vectors of size 1, and 1x1 matrices as arguments.
  // Optimization: can directly cast them to boolean. No need for OpAny/OpAll.
  {
    QualType scalarType = {};
    if (TypeTranslator::isScalarType(argType, &scalarType) &&
        (scalarType->isBooleanType() || scalarType->isFloatingType() ||
         scalarType->isIntegerType()))
      return castToBool(doExpr(arg), argType, returnType);
  }

  // Handle vectors larger than 1, Mx1 matrices, and 1xN matrices as arguments.
  // Cast the vector to a boolean vector, then run OpAny/OpAll on it.
  {
    QualType elemType = {};
    uint32_t size = 0;
    if (TypeTranslator::isVectorType(argType, &elemType, &size)) {
      const QualType castToBoolType =
          astContext.getExtVectorType(returnType, size);
      uint32_t castedToBoolId =
          castToBool(doExpr(arg), argType, castToBoolType);
      return theBuilder.createUnaryOp(spvOp, returnTypeId, castedToBoolId);
    }
  }

  // Handle MxN matrices as arguments.
  {
    QualType elemType = {};
    uint32_t matRowCount = 0, matColCount = 0;
    if (TypeTranslator::isMxNMatrix(argType, &elemType, &matRowCount,
                                    &matColCount)) {
      if (!elemType->isFloatingType()) {
        emitError("'all' and 'any' currently do not take non-floating point "
                  "matrices as argument.");
        return 0;
      }

      uint32_t matrixId = doExpr(arg);
      const uint32_t vecType = typeTranslator.getComponentVectorType(argType);
      llvm::SmallVector<uint32_t, 4> rowResults;
      for (uint32_t i = 0; i < matRowCount; ++i) {
        // Extract the row which is a float vector of size matColCount.
        const uint32_t rowFloatVec =
            theBuilder.createCompositeExtract(vecType, matrixId, {i});
        // Cast the float vector to boolean vector.
        const auto rowFloatQualType =
            astContext.getExtVectorType(elemType, matColCount);
        const auto rowBoolQualType =
            astContext.getExtVectorType(returnType, matColCount);
        const uint32_t rowBoolVec =
            castToBool(rowFloatVec, rowFloatQualType, rowBoolQualType);
        // Perform OpAny/OpAll on the boolean vector.
        rowResults.push_back(
            theBuilder.createUnaryOp(spvOp, returnTypeId, rowBoolVec));
      }
      // Create a new vector that is the concatenation of results of all rows.
      uint32_t boolId = theBuilder.getBoolType();
      uint32_t vecOfBoolsId = theBuilder.getVecType(boolId, matRowCount);
      const uint32_t rowResultsId =
          theBuilder.createCompositeConstruct(vecOfBoolsId, rowResults);

      // Run OpAny/OpAll on the newly-created vector.
      return theBuilder.createUnaryOp(spvOp, returnTypeId, rowResultsId);
    }
  }

  // All types should be handled already.
  llvm_unreachable("Unknown argument type passed to all()/any().");
  return 0;
}

uint32_t SPIRVEmitter::processIntrinsicAsType(const CallExpr *callExpr) {
  const QualType returnType = callExpr->getType();
  const uint32_t returnTypeId = typeTranslator.translateType(returnType);
  assert(callExpr->getNumArgs() == 1u);
  const Expr *arg = callExpr->getArg(0);
  const QualType argType = arg->getType();

  // asfloat may take a float or a float vector or a float matrix as argument.
  // These cases would be a no-op.
  if (returnType.getCanonicalType() == argType.getCanonicalType())
    return doExpr(arg);

  // SPIR-V does not support non-floating point matrices. So 'asint' and
  // 'asuint' for MxN matrices are currently not supported.
  if (TypeTranslator::isMxNMatrix(argType)) {
    emitError("SPIR-V does not support non-floating point matrices. Thus, "
              "'asint' and 'asuint' currently do not take matrix arguments.");
    return 0;
  }

  return theBuilder.createUnaryOp(spv::Op::OpBitcast, returnTypeId,
                                  doExpr(arg));
}

uint32_t SPIRVEmitter::processIntrinsicFloatSign(const CallExpr *callExpr) {
  // Import the GLSL.std.450 extended instruction set.
  const uint32_t glslInstSetId = theBuilder.getGLSLExtInstSet();
  const Expr *arg = callExpr->getArg(0);
  const QualType returnType = callExpr->getType();
  const QualType argType = arg->getType();
  assert(isFloatOrVecMatOfFloatType(argType));
  const uint32_t argTypeId = typeTranslator.translateType(argType);
  const uint32_t argId = doExpr(arg);
  uint32_t floatSignResultId = 0;

  // For matrices, we can perform the instruction on each vector of the matrix.
  if (TypeTranslator::isSpirvAcceptableMatrixType(argType)) {
    const auto actOnEachVec = [this, glslInstSetId](
        uint32_t /*index*/, uint32_t vecType, uint32_t curRowId) {
      return theBuilder.createExtInst(vecType, glslInstSetId,
                                      GLSLstd450::GLSLstd450FSign, {curRowId});
    };
    floatSignResultId = processEachVectorInMatrix(arg, argId, actOnEachVec);
  } else {
    floatSignResultId = theBuilder.createExtInst(
        argTypeId, glslInstSetId, GLSLstd450::GLSLstd450FSign, {argId});
  }

  return castToInt(floatSignResultId, arg->getType(), returnType);
}

uint32_t SPIRVEmitter::processIntrinsicUsingGLSLInst(
    const CallExpr *callExpr, GLSLstd450 opcode, bool actPerRowForMatrices) {
  // Import the GLSL.std.450 extended instruction set.
  const uint32_t glslInstSetId = theBuilder.getGLSLExtInstSet();

  if (callExpr->getNumArgs() == 1u) {
    const uint32_t returnType =
        typeTranslator.translateType(callExpr->getType());
    const Expr *arg = callExpr->getArg(0);
    const uint32_t argId = doExpr(arg);

    // If the instruction does not operate on matrices, we can perform the
    // instruction on each vector of the matrix.
    if (actPerRowForMatrices &&
        TypeTranslator::isSpirvAcceptableMatrixType(arg->getType())) {
      const auto actOnEachVec = [this, glslInstSetId, opcode](
          uint32_t /*index*/, uint32_t vecType, uint32_t curRowId) {
        return theBuilder.createExtInst(vecType, glslInstSetId, opcode,
                                        {curRowId});
      };
      return processEachVectorInMatrix(arg, argId, actOnEachVec);
    }

    return theBuilder.createExtInst(returnType, glslInstSetId, opcode, {argId});
  }

  emitError("Unsupported intrinsic function %0.")
      << cast<DeclRefExpr>(callExpr->getCallee())->getNameInfo().getAsString();
  return 0;
}

uint32_t SPIRVEmitter::getValueZero(QualType type) {
  {
    QualType scalarType = {};
    if (TypeTranslator::isScalarType(type, &scalarType)) {
      if (scalarType->isSignedIntegerType()) {
        return theBuilder.getConstantInt32(0);
      }

      if (scalarType->isUnsignedIntegerType()) {
        return theBuilder.getConstantUint32(0);
      }

      if (scalarType->isFloatingType()) {
        return theBuilder.getConstantFloat32(0.0);
      }
    }
  }

  {
    QualType elemType = {};
    uint32_t size = {};
    if (TypeTranslator::isVectorType(type, &elemType, &size)) {
      return getVecValueZero(elemType, size);
    }
  }

  // TODO: Handle getValueZero for MxN matrices.

  emitError("getting value 0 for type '%0' unimplemented")
      << type.getAsString();
  return 0;
}

uint32_t SPIRVEmitter::getVecValueZero(QualType elemType, uint32_t size) {
  const uint32_t elemZeroId = getValueZero(elemType);

  if (size == 1)
    return elemZeroId;

  llvm::SmallVector<uint32_t, 4> elements(size_t(size), elemZeroId);
  const uint32_t vecType =
      theBuilder.getVecType(typeTranslator.translateType(elemType), size);

  return theBuilder.getConstantComposite(vecType, elements);
}

uint32_t SPIRVEmitter::getValueOne(QualType type) {
  {
    QualType scalarType = {};
    if (TypeTranslator::isScalarType(type, &scalarType)) {
      if (scalarType->isSignedIntegerType()) {
        return theBuilder.getConstantInt32(1);
      }

      if (scalarType->isUnsignedIntegerType()) {
        return theBuilder.getConstantUint32(1);
      }

      if (scalarType->isFloatingType()) {
        return theBuilder.getConstantFloat32(1.0);
      }
    }
  }

  {
    QualType elemType = {};
    uint32_t size = {};
    if (TypeTranslator::isVectorType(type, &elemType, &size)) {
      return getVecValueOne(elemType, size);
    }
  }

  emitError("getting value 1 for type '%0' unimplemented") << type;
  return 0;
}

uint32_t SPIRVEmitter::getVecValueOne(QualType elemType, uint32_t size) {
  const uint32_t elemOneId = getValueOne(elemType);

  if (size == 1)
    return elemOneId;

  llvm::SmallVector<uint32_t, 4> elements(size_t(size), elemOneId);
  const uint32_t vecType =
      theBuilder.getVecType(typeTranslator.translateType(elemType), size);

  return theBuilder.getConstantComposite(vecType, elements);
}

uint32_t SPIRVEmitter::getMatElemValueOne(QualType type) {
  assert(hlsl::IsHLSLMatType(type));
  const auto elemType = hlsl::GetHLSLMatElementType(type);

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  if (rowCount == 1 && colCount == 1)
    return getValueOne(elemType);
  if (colCount == 1)
    return getVecValueOne(elemType, rowCount);
  return getVecValueOne(elemType, colCount);
}

uint32_t SPIRVEmitter::translateAPValue(const APValue &value,
                                        const QualType targetType) {
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

uint32_t SPIRVEmitter::translateAPInt(const llvm::APInt &intValue,
                                      QualType targetType) {
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

  emitError("APInt for target bitwidth '%0' is not supported yet.") << bitwidth;
  return 0;
}

uint32_t SPIRVEmitter::translateAPFloat(const llvm::APFloat &floatValue,
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

spv::ExecutionModel
SPIRVEmitter::getSpirvShaderStage(const hlsl::ShaderModel &model) {
  // DXIL Models are:
  // Profile (DXIL Model) : HLSL Shader Kind : SPIR-V Shader Stage
  // vs_<version>         : Vertex Shader    : Vertex Shader
  // hs_<version>         : Hull Shader      : Tassellation Control Shader
  // ds_<version>         : Domain Shader    : Tessellation Evaluation Shader
  // gs_<version>         : Geometry Shader  : Geometry Shader
  // ps_<version>         : Pixel Shader     : Fragment Shader
  // cs_<version>         : Compute Shader   : Compute Shader
  switch (model.GetKind()) {
  case hlsl::ShaderModel::Kind::Vertex:
    return spv::ExecutionModel::Vertex;
  case hlsl::ShaderModel::Kind::Hull:
    return spv::ExecutionModel::TessellationControl;
  case hlsl::ShaderModel::Kind::Domain:
    return spv::ExecutionModel::TessellationEvaluation;
  case hlsl::ShaderModel::Kind::Geometry:
    return spv::ExecutionModel::Geometry;
  case hlsl::ShaderModel::Kind::Pixel:
    return spv::ExecutionModel::Fragment;
  case hlsl::ShaderModel::Kind::Compute:
    return spv::ExecutionModel::GLCompute;
  default:
    break;
  }
  llvm_unreachable("unknown shader model");
}

void SPIRVEmitter::AddRequiredCapabilitiesForShaderModel() {
  if (shaderModel.IsHS() || shaderModel.IsDS()) {
    theBuilder.requireCapability(spv::Capability::Tessellation);
    emitError("Tasselation shaders are currently not supported.");
  } else if (shaderModel.IsGS()) {
    theBuilder.requireCapability(spv::Capability::Geometry);
    emitError("Geometry shaders are currently not supported.");
  } else {
    theBuilder.requireCapability(spv::Capability::Shader);
  }
}

void SPIRVEmitter::AddExecutionModeForEntryPoint(uint32_t entryPointId) {
  if (shaderModel.IsPS()) {
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

bool SPIRVEmitter::allSwitchCasesAreIntegerLiterals(const Stmt *root) {
  if (!root)
    return false;

  const auto *caseStmt = dyn_cast<CaseStmt>(root);
  const auto *compoundStmt = dyn_cast<CompoundStmt>(root);
  if (!caseStmt && !compoundStmt)
    return true;

  if (caseStmt) {
    const Expr *caseExpr = caseStmt->getLHS();
    return caseExpr && caseExpr->isEvaluatable(astContext);
  }

  // Recurse down if facing a compound statement.
  for (auto *st : compoundStmt->body())
    if (!allSwitchCasesAreIntegerLiterals(st))
      return false;

  return true;
}

void SPIRVEmitter::discoverAllCaseStmtInSwitchStmt(
    const Stmt *root, uint32_t *defaultBB,
    std::vector<std::pair<uint32_t, uint32_t>> *targets) {
  if (!root)
    return;

  // A switch case can only appear in DefaultStmt, CaseStmt, or
  // CompoundStmt. For the rest, we can just return.
  const auto *defaultStmt = dyn_cast<DefaultStmt>(root);
  const auto *caseStmt = dyn_cast<CaseStmt>(root);
  const auto *compoundStmt = dyn_cast<CompoundStmt>(root);
  if (!defaultStmt && !caseStmt && !compoundStmt)
    return;

  // Recurse down if facing a compound statement.
  if (compoundStmt) {
    for (auto *st : compoundStmt->body())
      discoverAllCaseStmtInSwitchStmt(st, defaultBB, targets);
    return;
  }

  std::string caseLabel;
  uint32_t caseValue = 0;
  if (defaultStmt) {
    // This is the default branch.
    caseLabel = "switch.default";
  } else if (caseStmt) {
    // This is a non-default case.
    // When using OpSwitch, we only allow integer literal cases. e.g:
    // case <literal_integer>: {...; break;}
    const Expr *caseExpr = caseStmt->getLHS();
    assert(caseExpr && caseExpr->isEvaluatable(astContext));
    auto bitWidth = astContext.getIntWidth(caseExpr->getType());
    if (bitWidth != 32)
      emitError("Switch statement translation currently only supports 32-bit "
                "integer case values.");
    Expr::EvalResult evalResult;
    caseExpr->EvaluateAsRValue(evalResult, astContext);
    const int64_t value = evalResult.Val.getInt().getSExtValue();
    caseValue = static_cast<uint32_t>(value);
    caseLabel = "switch." + std::string(value < 0 ? "n" : "") +
                llvm::itostr(std::abs(value));
  }
  const uint32_t caseBB = theBuilder.createBasicBlock(caseLabel);
  theBuilder.addSuccessor(caseBB);
  stmtBasicBlock[root] = caseBB;

  // Add all cases to the 'targets' vector.
  if (caseStmt)
    targets->emplace_back(caseValue, caseBB);

  // The default label is not part of the 'targets' vector that is passed
  // to the OpSwitch instruction.
  // If default statement was discovered, return its label via defaultBB.
  if (defaultStmt)
    *defaultBB = caseBB;

  // Process cases nested in other cases. It happens when we have fall through
  // cases. For example:
  // case 1: case 2: ...; break;
  // will result in the CaseSmt for case 2 nested in the one for case 1.
  discoverAllCaseStmtInSwitchStmt(caseStmt ? caseStmt->getSubStmt()
                                           : defaultStmt->getSubStmt(),
                                  defaultBB, targets);
}

void SPIRVEmitter::flattenSwitchStmtAST(const Stmt *root,
                                        std::vector<const Stmt *> *flatSwitch) {
  const auto *caseStmt = dyn_cast<CaseStmt>(root);
  const auto *compoundStmt = dyn_cast<CompoundStmt>(root);
  const auto *defaultStmt = dyn_cast<DefaultStmt>(root);

  if (!compoundStmt) {
    flatSwitch->push_back(root);
  }

  if (compoundStmt) {
    for (const auto *st : compoundStmt->body())
      flattenSwitchStmtAST(st, flatSwitch);
  } else if (caseStmt) {
    flattenSwitchStmtAST(caseStmt->getSubStmt(), flatSwitch);
  } else if (defaultStmt) {
    flattenSwitchStmtAST(defaultStmt->getSubStmt(), flatSwitch);
  }
}

void SPIRVEmitter::processCaseStmtOrDefaultStmt(const Stmt *stmt) {
  auto *caseStmt = dyn_cast<CaseStmt>(stmt);
  auto *defaultStmt = dyn_cast<DefaultStmt>(stmt);
  assert(caseStmt || defaultStmt);

  uint32_t caseBB = stmtBasicBlock[stmt];
  if (!theBuilder.isCurrentBasicBlockTerminated()) {
    // We are about to handle the case passed in as parameter. If the current
    // basic block is not terminated, it means the previous case is a fall
    // through case. We need to link it to the case to be processed.
    theBuilder.createBranch(caseBB);
    theBuilder.addSuccessor(caseBB);
  }
  theBuilder.setInsertPoint(caseBB);
  doStmt(caseStmt ? caseStmt->getSubStmt() : defaultStmt->getSubStmt());
}

void SPIRVEmitter::processSwitchStmtUsingSpirvOpSwitch(
    const SwitchStmt *switchStmt) {
  // First handle the condition variable DeclStmt if one exists.
  // For example: handle 'int a = b' in the following:
  // switch (int a = b) {...}
  if (const auto *condVarDeclStmt = switchStmt->getConditionVariableDeclStmt())
    doDeclStmt(condVarDeclStmt);

  const uint32_t selector = doExpr(switchStmt->getCond());

  // We need a merge block regardless of the number of switch cases.
  // Since OpSwitch always requires a default label, if the switch statement
  // does not have a default branch, we use the merge block as the default
  // target.
  const uint32_t mergeBB = theBuilder.createBasicBlock("switch.merge");
  theBuilder.setMergeTarget(mergeBB);
  breakStack.push(mergeBB);
  uint32_t defaultBB = mergeBB;

  // (literal, labelId) pairs to pass to the OpSwitch instruction.
  std::vector<std::pair<uint32_t, uint32_t>> targets;
  discoverAllCaseStmtInSwitchStmt(switchStmt->getBody(), &defaultBB, &targets);

  // Create the OpSelectionMerge and OpSwitch.
  theBuilder.createSwitch(mergeBB, selector, defaultBB, targets);

  // Handle the switch body.
  doStmt(switchStmt->getBody());

  if (!theBuilder.isCurrentBasicBlockTerminated())
    theBuilder.createBranch(mergeBB);
  theBuilder.setInsertPoint(mergeBB);
  breakStack.pop();
}

void SPIRVEmitter::processSwitchStmtUsingIfStmts(const SwitchStmt *switchStmt) {
  std::vector<const Stmt *> flatSwitch;
  flattenSwitchStmtAST(switchStmt->getBody(), &flatSwitch);

  // First handle the condition variable DeclStmt if one exists.
  // For example: handle 'int a = b' in the following:
  // switch (int a = b) {...}
  if (const auto *condVarDeclStmt = switchStmt->getConditionVariableDeclStmt())
    doDeclStmt(condVarDeclStmt);

  // Figure out the indexes of CaseStmts (and DefaultStmt if it exists) in
  // the flattened switch AST.
  // For instance, for the following flat vector:
  // +-----+-----+-----+-----+-----+-----+-----+-----+-----+-------+-----+
  // |Case1|Stmt1|Case2|Stmt2|Break|Case3|Case4|Stmt4|Break|Default|Stmt5|
  // +-----+-----+-----+-----+-----+-----+-----+-----+-----+-------+-----+
  // The indexes are: {0, 2, 5, 6, 9}
  std::vector<uint32_t> caseStmtLocs;
  for (uint32_t i = 0; i < flatSwitch.size(); ++i)
    if (isa<CaseStmt>(flatSwitch[i]) || isa<DefaultStmt>(flatSwitch[i]))
      caseStmtLocs.push_back(i);

  IfStmt *prevIfStmt = nullptr;
  IfStmt *rootIfStmt = nullptr;
  CompoundStmt *defaultBody = nullptr;

  // For each case, start at its index in the vector, and go forward
  // accumulating statements until BreakStmt or end of vector is reached.
  for (auto curCaseIndex : caseStmtLocs) {
    const Stmt *curCase = flatSwitch[curCaseIndex];

    // CompoundStmt to hold all statements for this case.
    CompoundStmt *cs = new (astContext) CompoundStmt(Stmt::EmptyShell());

    // Accumulate all non-case/default/break statements as the body for the
    // current case.
    std::vector<Stmt *> statements;
    for (int i = curCaseIndex + 1;
         i < flatSwitch.size() && !isa<BreakStmt>(flatSwitch[i]); ++i) {
      if (!isa<CaseStmt>(flatSwitch[i]) && !isa<DefaultStmt>(flatSwitch[i]))
        statements.push_back(const_cast<Stmt *>(flatSwitch[i]));
    }
    if (!statements.empty())
      cs->setStmts(astContext, statements.data(), statements.size());

    // For non-default cases, generate the IfStmt that compares the switch
    // value to the case value.
    if (auto *caseStmt = dyn_cast<CaseStmt>(curCase)) {
      IfStmt *curIf = new (astContext) IfStmt(Stmt::EmptyShell());
      BinaryOperator *bo = new (astContext) BinaryOperator(Stmt::EmptyShell());
      bo->setLHS(const_cast<Expr *>(switchStmt->getCond()));
      bo->setRHS(const_cast<Expr *>(caseStmt->getLHS()));
      bo->setOpcode(BO_EQ);
      bo->setType(astContext.getLogicalOperationType());
      curIf->setCond(bo);
      curIf->setThen(cs);
      // No conditional variable associated with this faux if statement.
      curIf->setConditionVariable(astContext, nullptr);
      // Each If statement is the "else" of the previous if statement.
      if (prevIfStmt)
        prevIfStmt->setElse(curIf);
      else
        rootIfStmt = curIf;
      prevIfStmt = curIf;
    } else {
      // Record the DefaultStmt body as it will be used as the body of the
      // "else" block in the if-elseif-...-else pattern.
      defaultBody = cs;
    }
  }

  // If a default case exists, it is the "else" of the last if statement.
  if (prevIfStmt)
    prevIfStmt->setElse(defaultBody);

  // Since all else-if and else statements are the child nodes of the first
  // IfStmt, we only need to call doStmt for the first IfStmt.
  if (rootIfStmt)
    doStmt(rootIfStmt);
  // If there are no CaseStmt and there is only 1 DefaultStmt, there will be
  // no if statements. The switch in that case only executes the body of the
  // default case.
  else if (defaultBody)
    doStmt(defaultBody);
}

} // end namespace spirv
} // end namespace clang
