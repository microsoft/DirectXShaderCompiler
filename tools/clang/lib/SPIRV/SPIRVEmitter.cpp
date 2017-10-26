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
#include "spirv-tools/optimizer.hpp"
#include "llvm/ADT/StringExtras.h"

#include "InitListHandler.h"

namespace clang {
namespace spirv {

namespace {

// Returns true if the given decl has the given semantic.
bool hasSemantic(const DeclaratorDecl *decl,
                 hlsl::DXIL::SemanticKind semanticKind) {
  using namespace hlsl;
  for (auto *annotation : decl->getUnusualAnnotations()) {
    if (auto *semanticDecl = dyn_cast<SemanticDecl>(annotation)) {
      llvm::StringRef semanticName;
      uint32_t semanticIndex = 0;
      Semantic::DecomposeNameAndIndex(semanticDecl->SemanticName, &semanticName,
                                      &semanticIndex);
      const auto *semantic = Semantic::GetByName(semanticName);
      if (semantic->GetKind() == semanticKind)
        return true;
    }
  }
  return false;
}

bool patchConstFuncTakesHullOutputPatch(FunctionDecl *pcf) {
  for (const auto *param : pcf->parameters())
    if (TypeTranslator::isOutputPatch(param->getType()))
      return true;
  return false;
}

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

/// If expr is a (RW)StructuredBuffer.Load(), returns the object and writes
/// index. Otherwiser, returns false.
// TODO: The following doesn't handle Load(int, int) yet. And it is basically a
// duplicate of doCXXMemberCallExpr.
const Expr *isStructuredBufferLoad(const Expr *expr, const Expr **index) {
  using namespace hlsl;

  if (const auto *indexing = dyn_cast<CXXMemberCallExpr>(expr)) {
    const auto *callee = indexing->getDirectCallee();
    uint32_t opcode = static_cast<uint32_t>(IntrinsicOp::Num_Intrinsics);
    llvm::StringRef group;

    if (GetIntrinsicOp(callee, opcode, group)) {
      if (static_cast<IntrinsicOp>(opcode) == IntrinsicOp::MOP_Load) {
        const auto *object = indexing->getImplicitObjectArgument();
        if (TypeTranslator::isStructuredBuffer(object->getType())) {
          *index = indexing->getArg(0);
          return indexing->getImplicitObjectArgument();
        }
      }
    }
  }

  return nullptr;
}

bool spirvToolsOptimize(std::vector<uint32_t> *module, std::string *messages) {
  spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_0);

  optimizer.SetMessageConsumer(
      [messages](spv_message_level_t /*level*/, const char * /*source*/,
                 const spv_position_t & /*position*/,
                 const char *message) { *messages += message; });

  optimizer.RegisterPass(spvtools::CreateInlineExhaustivePass());
  optimizer.RegisterPass(spvtools::CreateLocalAccessChainConvertPass());
  optimizer.RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass());
  optimizer.RegisterPass(spvtools::CreateLocalSingleStoreElimPass());
  optimizer.RegisterPass(spvtools::CreateInsertExtractElimPass());
  optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass());

  optimizer.RegisterPass(spvtools::CreateDeadBranchElimPass());
  optimizer.RegisterPass(spvtools::CreateBlockMergePass());
  optimizer.RegisterPass(spvtools::CreateLocalMultiStoreElimPass());
  optimizer.RegisterPass(spvtools::CreateInsertExtractElimPass());
  optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass());

  optimizer.RegisterPass(spvtools::CreateEliminateDeadFunctionsPass());
  optimizer.RegisterPass(spvtools::CreateCFGCleanupPass());
  optimizer.RegisterPass(spvtools::CreateDeadVariableEliminationPass());
  optimizer.RegisterPass(spvtools::CreateEliminateDeadConstantPass());

  optimizer.RegisterPass(spvtools::CreateCompactIdsPass());

  return optimizer.Run(module->data(), module->size(), module);
}

/// Translates atomic HLSL opcodes into the equivalent SPIR-V opcode.
spv::Op translateAtomicHlslOpcodeToSpirvOpcode(hlsl::IntrinsicOp opcode) {
  using namespace hlsl;
  using namespace spv;

  switch (opcode) {
  case IntrinsicOp::IOP_InterlockedAdd:
  case IntrinsicOp::MOP_InterlockedAdd:
    return Op::OpAtomicIAdd;
  case IntrinsicOp::IOP_InterlockedAnd:
  case IntrinsicOp::MOP_InterlockedAnd:
    return Op::OpAtomicAnd;
  case IntrinsicOp::IOP_InterlockedOr:
  case IntrinsicOp::MOP_InterlockedOr:
    return Op::OpAtomicOr;
  case IntrinsicOp::IOP_InterlockedXor:
  case IntrinsicOp::MOP_InterlockedXor:
    return Op::OpAtomicXor;
  case IntrinsicOp::IOP_InterlockedUMax:
  case IntrinsicOp::MOP_InterlockedUMax:
    return Op::OpAtomicUMax;
  case IntrinsicOp::IOP_InterlockedUMin:
  case IntrinsicOp::MOP_InterlockedUMin:
    return Op::OpAtomicUMin;
  case IntrinsicOp::IOP_InterlockedMax:
  case IntrinsicOp::MOP_InterlockedMax:
    return Op::OpAtomicSMax;
  case IntrinsicOp::IOP_InterlockedMin:
  case IntrinsicOp::MOP_InterlockedMin:
    return Op::OpAtomicSMin;
  case IntrinsicOp::IOP_InterlockedExchange:
  case IntrinsicOp::MOP_InterlockedExchange:
    return Op::OpAtomicExchange;
  }

  assert(false && "unimplemented hlsl intrinsic opcode");
  return Op::Max;
}

} // namespace

SPIRVEmitter::SPIRVEmitter(CompilerInstance &ci,
                           const EmitSPIRVOptions &options)
    : theCompilerInstance(ci), astContext(ci.getASTContext()),
      diags(ci.getDiagnostics()), spirvOptions(options),
      entryFunctionName(ci.getCodeGenOpts().HLSLEntryFunction),
      shaderModel(*hlsl::ShaderModel::GetByName(
          ci.getCodeGenOpts().HLSLProfile.c_str())),
      theContext(), theBuilder(&theContext),
      declIdMapper(shaderModel, astContext, theBuilder, diags, spirvOptions),
      typeTranslator(astContext, theBuilder, diags), entryFunctionId(0),
      curFunction(nullptr), curThis(0), needsLegalization(false) {
  if (shaderModel.GetKind() == hlsl::ShaderModel::Kind::Invalid)
    emitError("unknown shader module: %0") << shaderModel.GetName();
}

void SPIRVEmitter::HandleTranslationUnit(ASTContext &context) {
  // Stop translating if there are errors in previous compilation stages.
  if (context.getDiagnostics().hasErrorOccurred())
    return;

  TranslationUnitDecl *tu = context.getTranslationUnitDecl();

  // The entry function is the seed of the queue.
  for (auto *decl : tu->decls()) {
    if (auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
      if (funcDecl->getName() == entryFunctionName) {
        workQueue.insert(funcDecl);
      }
      if (context.IsPatchConstantFunctionDecl(funcDecl)) {
        patchConstFunc = funcDecl;
      }
    } else if (auto *varDecl = dyn_cast<VarDecl>(decl)) {
      if (isa<HLSLBufferDecl>(varDecl->getDeclContext())) {
        // This is a VarDecl of a ConstantBuffer/TextureBuffer type.
        (void)declIdMapper.createCTBuffer(varDecl);
      } else {
        doVarDecl(varDecl);
      }
    } else if (auto *bufferDecl = dyn_cast<HLSLBufferDecl>(decl)) {
      // This is a cbuffer/tbuffer decl.
      (void)declIdMapper.createCTBuffer(bufferDecl);
    }
  }

  // Translate all functions reachable from the entry function.
  // The queue can grow in the meanwhile; so need to keep evaluating
  // workQueue.size().
  for (uint32_t i = 0; i < workQueue.size(); ++i) {
    doDecl(workQueue[i]);
  }

  if (context.getDiagnostics().hasErrorOccurred())
    return;

  AddRequiredCapabilitiesForShaderModel();

  // Addressing and memory model are required in a valid SPIR-V module.
  theBuilder.setAddressingModel(spv::AddressingModel::Logical);
  theBuilder.setMemoryModel(spv::MemoryModel::GLSL450);

  theBuilder.addEntryPoint(getSpirvShaderStage(shaderModel), entryFunctionId,
                           entryFunctionName, declIdMapper.collectStageVars());

  AddExecutionModeForEntryPoint(entryFunctionId);

  // Add Location decorations to stage input/output variables.
  if (!declIdMapper.decorateStageIOLocations())
    return;

  // Add descriptor set and binding decorations to resource variables.
  if (!declIdMapper.decorateResourceBindings())
    return;

  // Output the constructed module.
  std::vector<uint32_t> m = theBuilder.takeModule();

  const auto optLevel = theCompilerInstance.getCodeGenOpts().OptimizationLevel;
  if (needsLegalization || optLevel > 0) {
    if (needsLegalization && optLevel == 0)
      emitWarning("-O0 ignored since SPIR-V legalization required");

    std::string messages;
    if (!spirvToolsOptimize(&m, &messages)) {
      emitFatalError("failed to legalize/optimize SPIR-V: %0") << messages;
      return;
    }
  }

  theCompilerInstance.getOutStream()->write(
      reinterpret_cast<const char *>(m.data()), m.size() * 4);
}

void SPIRVEmitter::doDecl(const Decl *decl) {
  if (const auto *varDecl = dyn_cast<VarDecl>(decl)) {
    doVarDecl(varDecl);
  } else if (const auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
    doFunctionDecl(funcDecl);
  } else if (dyn_cast<HLSLBufferDecl>(decl)) {
    llvm_unreachable("HLSLBufferDecl should not be handled here");
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
  } else if (const auto *discardStmt = dyn_cast<DiscardStmt>(stmt)) {
    doDiscardStmt(discardStmt);
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

SpirvEvalInfo SPIRVEmitter::doExpr(const Expr *expr) {
  if (const auto *delRefExpr = dyn_cast<DeclRefExpr>(expr)) {
    return declIdMapper.getDeclResultId(delRefExpr->getFoundDecl());
  }

  if (const auto *parenExpr = dyn_cast<ParenExpr>(expr)) {
    // Just need to return what's inside the parentheses.
    return doExpr(parenExpr->getSubExpr());
  }

  if (const auto *memberExpr = dyn_cast<MemberExpr>(expr)) {
    return doMemberExpr(memberExpr);
  }

  if (const auto *castExpr = dyn_cast<CastExpr>(expr)) {
    return doCastExpr(castExpr);
  }

  if (const auto *initListExpr = dyn_cast<InitListExpr>(expr)) {
    return doInitListExpr(initListExpr);
  }

  if (const auto *boolLiteral = dyn_cast<CXXBoolLiteralExpr>(expr)) {
    const bool value = boolLiteral->getValue();
    return SpirvEvalInfo::withConst(theBuilder.getConstantBool(value));
  }

  if (const auto *intLiteral = dyn_cast<IntegerLiteral>(expr)) {
    return SpirvEvalInfo::withConst(
        translateAPInt(intLiteral->getValue(), expr->getType()));
  }

  if (const auto *floatLiteral = dyn_cast<FloatingLiteral>(expr)) {
    return SpirvEvalInfo::withConst(
        translateAPFloat(floatLiteral->getValue(), expr->getType()));
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

  if (const auto *subscriptExpr = dyn_cast<ArraySubscriptExpr>(expr)) {
    return doArraySubscriptExpr(subscriptExpr);
  }

  if (const auto *condExpr = dyn_cast<ConditionalOperator>(expr)) {
    return doConditionalOperator(condExpr);
  }

  if (isa<CXXThisExpr>(expr)) {
    assert(curThis);
    return curThis;
  }

  emitError("Expr '%0' is not supported yet.") << expr->getStmtClassName();
  return 0;
}

SpirvEvalInfo SPIRVEmitter::loadIfGLValue(const Expr *expr) {
  auto info = doExpr(expr);
  if (expr->isGLValue())
    info.resultId = theBuilder.createLoad(
        typeTranslator.translateType(expr->getType(), info.layoutRule),
        info.resultId);

  return info;
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
  // We are about to start translation for a new function. Clear the break stack
  // and the continue stack.
  breakStack = std::stack<uint32_t>();
  continueStack = std::stack<uint32_t>();

  curFunction = decl;

  std::string funcName = decl->getName();

  uint32_t funcId = 0;

  if (funcName == entryFunctionName) {
    // The entry function surely does not have pre-assigned <result-id> for
    // it like other functions that got added to the work queue following
    // function calls.
    funcId = theContext.takeNextId();
    funcName = "src." + funcName;

    // Create wrapper for the entry function
    if (!emitEntryFunctionWrapper(decl, funcId))
      return;
  } else {
    // Non-entry functions are added to the work queue following function
    // calls. We have already assigned <result-id>s for it when translating
    // its call site. Query it here.
    funcId = declIdMapper.getDeclResultId(decl).resultId;
  }

  if (!needsLegalization &&
      TypeTranslator::isOpaqueStructType(decl->getReturnType()))
    needsLegalization = true;

  const uint32_t retType = typeTranslator.translateType(decl->getReturnType());

  // Construct the function signature.
  llvm::SmallVector<uint32_t, 4> paramTypes;

  bool isNonStaticMemberFn = false;
  if (const auto *memberFn = dyn_cast<CXXMethodDecl>(decl)) {
    isNonStaticMemberFn = !memberFn->isStatic();

    if (isNonStaticMemberFn) {
      // For non-static member function, the first parameter should be the
      // object on which we are invoking this method.
      const uint32_t valueType = typeTranslator.translateType(
          memberFn->getThisType(astContext)->getPointeeType());
      const uint32_t ptrType =
          theBuilder.getPointerType(valueType, spv::StorageClass::Function);
      paramTypes.push_back(ptrType);
    }

    // Prefix the function name with the struct name
    if (const auto *st = dyn_cast<CXXRecordDecl>(memberFn->getDeclContext()))
      funcName = st->getName().str() + "." + funcName;
  }

  for (const auto *param : decl->params()) {
    const uint32_t valueType = typeTranslator.translateType(param->getType());
    const uint32_t ptrType =
        theBuilder.getPointerType(valueType, spv::StorageClass::Function);
    paramTypes.push_back(ptrType);

    if (!needsLegalization &&
        TypeTranslator::isOpaqueStructType(param->getType()))
      needsLegalization = true;
  }

  const uint32_t funcType = theBuilder.getFunctionType(retType, paramTypes);
  theBuilder.beginFunction(funcType, retType, funcName, funcId);

  if (isNonStaticMemberFn) {
    // Remember the parameter for the this object so later we can handle
    // CXXThisExpr correctly.
    curThis = theBuilder.addFnParam(paramTypes[0], "param.this");
  }

  // Create all parameters.
  for (uint32_t i = 0; i < decl->getNumParams(); ++i) {
    const ParmVarDecl *paramDecl = decl->getParamDecl(i);
    (void)declIdMapper.createFnParam(paramTypes[i + isNonStaticMemberFn],
                                     paramDecl);
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

void SPIRVEmitter::doVarDecl(const VarDecl *decl) {
  uint32_t varId = 0;

  // The contents in externally visible variables can be updated via the
  // pipeline. They should be handled differently from file and function scope
  // variables.
  // File scope variables (static "global" and "local" variables) belongs to
  // the Private storage class, while function scope variables (normal "local"
  // variables) belongs to the Function storage class.
  if (!decl->isExternallyVisible() || decl->isStaticDataMember()) {
    // Note: cannot move varType outside of this scope because it generates
    // SPIR-V types without decorations, while external visible variable should
    // have SPIR-V type with decorations.
    const uint32_t varType = typeTranslator.translateType(decl->getType());

    // We already know the variable is not externally visible here. If it does
    // not have local storage, it should be file scope variable.
    const bool isFileScopeVar = !decl->hasLocalStorage();

    // Handle initializer. SPIR-V requires that "initializer must be an <id>
    // from a constant instruction or a global (module scope) OpVariable
    // instruction."
    llvm::Optional<uint32_t> constInit;
    if (decl->hasInit()) {
      if (const uint32_t id = tryToEvaluateAsConst(decl->getInit()))
        constInit = llvm::Optional<uint32_t>(id);
    } else if (isFileScopeVar) {
      // For static variables, if no initializers are provided, we should
      // initialize them to zero values.
      constInit = llvm::Optional<uint32_t>(theBuilder.getConstantNull(varType));
    }

    if (isFileScopeVar)
      varId = declIdMapper.createFileVar(varType, decl, constInit);
    else
      varId = declIdMapper.createFnVar(varType, decl, constInit);

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
        storeValue(varId, loadIfGLValue(decl->getInit()), decl->getType());
      }
    }
  } else {
    varId = declIdMapper.createExternVar(decl);
  }

  if (TypeTranslator::isRelaxedPrecisionType(decl->getType())) {
    theBuilder.decorate(varId, spv::Decoration::RelaxedPrecision);
  }

  if (!needsLegalization && TypeTranslator::isOpaqueStructType(decl->getType()))
    needsLegalization = true;
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

void SPIRVEmitter::doDiscardStmt(const DiscardStmt *discardStmt) {
  assert(!theBuilder.isCurrentBasicBlockTerminated());
  theBuilder.createKill();
  // Some statements that alter the control flow (break, continue, return, and
  // discard), require creation of a new basic block to hold any statement that
  // may follow them.
  const uint32_t newBB = theBuilder.createBasicBlock();
  theBuilder.setInsertPoint(newBB);
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

  // Some statements that alter the control flow (break, continue, return, and
  // discard), require creation of a new basic block to hold any statement that
  // may follow them. For example: StmtB and StmtC below are put inside a new
  // basic block which is unreachable.
  //
  // while (true) {
  //   StmtA;
  //   continue;
  //   StmtB;
  //   StmtC;
  // }
  const uint32_t newBB = theBuilder.createBasicBlock();
  theBuilder.setInsertPoint(newBB);
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

  { // Try to see if we can const-eval the condition
    bool condition = false;
    if (ifStmt->getCond()->EvaluateAsBooleanCondition(condition, astContext)) {
      if (condition) {
        doStmt(ifStmt->getThen());
      } else if (ifStmt->getElse()) {
        doStmt(ifStmt->getElse());
      }
      return;
    }
  }

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
  if (const auto *retVal = stmt->getRetValue()) {
    const auto retInfo = doExpr(retVal);
    const auto retType = retVal->getType();
    if (retInfo.storageClass != spv::StorageClass::Function &&
        retType->isStructureType()) {
      // We are returning some value from a non-Function storage class. Need to
      // create a temporary variable to "convert" the value to Function storage
      // class and then return.
      const uint32_t valType = typeTranslator.translateType(retType);
      const uint32_t tempVar = theBuilder.addFnVar(valType, "temp.var.ret");
      storeValue(tempVar, retInfo, retType);

      theBuilder.createReturnValue(theBuilder.createLoad(valType, tempVar));
    } else {
      theBuilder.createReturnValue(retInfo);
    }
  } else {
    theBuilder.createReturn();
  }

  // Some statements that alter the control flow (break, continue, return, and
  // discard), require creation of a new basic block to hold any statement that
  // may follow them. In this case, the newly created basic block will contain
  // any statement that may come after an early return.
  const uint32_t newBB = theBuilder.createBasicBlock();
  theBuilder.setInsertPoint(newBB);
}

void SPIRVEmitter::doBreakStmt(const BreakStmt *breakStmt) {
  assert(!theBuilder.isCurrentBasicBlockTerminated());
  uint32_t breakTargetBB = breakStack.top();
  theBuilder.addSuccessor(breakTargetBB);
  theBuilder.createBranch(breakTargetBB);

  // Some statements that alter the control flow (break, continue, return, and
  // discard), require creation of a new basic block to hold any statement that
  // may follow them. For example: StmtB and StmtC below are put inside a new
  // basic block which is unreachable.
  //
  // while (true) {
  //   StmtA;
  //   break;
  //   StmtB;
  //   StmtC;
  // }
  const uint32_t newBB = theBuilder.createBasicBlock();
  theBuilder.setInsertPoint(newBB);
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

SpirvEvalInfo
SPIRVEmitter::doArraySubscriptExpr(const ArraySubscriptExpr *expr) {
  llvm::SmallVector<uint32_t, 4> indices;
  const auto *base = collectArrayStructIndices(expr, &indices);
  auto info = doExpr(base);

  if (!indices.empty()) {
    const uint32_t ptrType = theBuilder.getPointerType(
        typeTranslator.translateType(expr->getType(), info.layoutRule),
        info.storageClass);
    info.resultId = theBuilder.createAccessChain(ptrType, info, indices);
  }

  return info;
}

SpirvEvalInfo SPIRVEmitter::doBinaryOperator(const BinaryOperator *expr) {
  const auto opcode = expr->getOpcode();

  // Handle assignment first since we need to evaluate rhs before lhs.
  // For other binary operations, we need to evaluate lhs before rhs.
  if (opcode == BO_Assign) {
    return processAssignment(expr->getLHS(), loadIfGLValue(expr->getRHS()),
                             false);
  }

  // Try to optimize floatMxN * float and floatN * float case
  if (opcode == BO_Mul) {
    if (const SpirvEvalInfo result = tryToGenFloatMatrixScale(expr))
      return result;
    if (const SpirvEvalInfo result = tryToGenFloatVectorScale(expr))
      return result;
  }

  const uint32_t resultType = typeTranslator.translateType(expr->getType());
  return processBinaryOp(expr->getLHS(), expr->getRHS(), opcode, resultType);
}

SpirvEvalInfo SPIRVEmitter::doCallExpr(const CallExpr *callExpr) {
  if (const auto *operatorCall = dyn_cast<CXXOperatorCallExpr>(callExpr))
    return doCXXOperatorCallExpr(operatorCall);

  if (const auto *memberCall = dyn_cast<CXXMemberCallExpr>(callExpr))
    return doCXXMemberCallExpr(memberCall);

  // Intrinsic functions such as 'dot' or 'mul'
  if (hlsl::IsIntrinsicOp(callExpr->getDirectCallee())) {
    return processIntrinsicCallExpr(callExpr);
  }

  // Normal standalone functions
  return processCall(callExpr);
}

uint32_t SPIRVEmitter::processCall(const CallExpr *callExpr) {
  const FunctionDecl *callee = callExpr->getDirectCallee();

  if (callee) {
    const auto numParams = callee->getNumParams();
    bool isNonStaticMemberCall = false;

    llvm::SmallVector<uint32_t, 4> params; // Temporary variables
    llvm::SmallVector<uint32_t, 4> args;   // Evaluated arguments

    if (const auto *memberCall = dyn_cast<CXXMemberCallExpr>(callExpr)) {
      isNonStaticMemberCall =
          !cast<CXXMethodDecl>(memberCall->getCalleeDecl())->isStatic();
      if (isNonStaticMemberCall) {
        // For non-static member calls, evaluate the object and pass it as the
        // first argument.
        const auto *object = memberCall->getImplicitObjectArgument();
        args.push_back(doExpr(object));
        // We do not need to create a new temporary variable for the this
        // object. Use the evaluated argument.
        params.push_back(args.back());
      }
    }

    // Evaluate parameters
    for (uint32_t i = 0; i < numParams; ++i) {
      const auto *arg = callExpr->getArg(i);
      const auto *param = callee->getParamDecl(i);

      // We need to create variables for holding the values to be used as
      // arguments. The variables themselves are of pointer types.
      const uint32_t varType = typeTranslator.translateType(arg->getType());
      const std::string varName = "param.var." + param->getNameAsString();
      const uint32_t tempVarId = theBuilder.addFnVar(varType, varName);

      params.push_back(tempVarId);
      args.push_back(doExpr(arg));

      if (param->getAttr<HLSLOutAttr>() || param->getAttr<HLSLInOutAttr>()) {
        // The current parameter is marked as out/inout. The argument then is
        // essentially passed in by reference. We need to load the value
        // explicitly here since the AST won't inject LValueToRValue implicit
        // cast for this case.
        const uint32_t value = theBuilder.createLoad(varType, args.back());
        theBuilder.createStore(tempVarId, value);
      } else {
        theBuilder.createStore(tempVarId, args.back());
      }
    }

    // Push the callee into the work queue if it is not there.
    if (!workQueue.count(callee)) {
      workQueue.insert(callee);
    }

    const uint32_t retType = typeTranslator.translateType(callExpr->getType());
    // Get or forward declare the function <result-id>
    const uint32_t funcId = declIdMapper.getOrRegisterFnResultId(callee);

    const uint32_t retVal =
        theBuilder.createFunctionCall(retType, funcId, params);

    // Go through all parameters and write those marked as out/inout
    for (uint32_t i = 0; i < numParams; ++i) {
      const auto *param = callee->getParamDecl(i);
      if (param->getAttr<HLSLOutAttr>() || param->getAttr<HLSLInOutAttr>()) {
        const uint32_t index = i + isNonStaticMemberCall;
        const uint32_t typeId = typeTranslator.translateType(param->getType());
        const uint32_t value = theBuilder.createLoad(typeId, params[index]);
        theBuilder.createStore(args[index], value);
      }
    }

    return retVal;
  }

  emitError("calling non-function unimplemented");
  return 0;
}

SpirvEvalInfo SPIRVEmitter::doCastExpr(const CastExpr *expr) {
  const Expr *subExpr = expr->getSubExpr();
  const QualType toType = expr->getType();

  switch (expr->getCastKind()) {
  case CastKind::CK_LValueToRValue: {
    auto info = doExpr(subExpr);
    if (isVectorShuffle(subExpr) || isa<ExtMatrixElementExpr>(subExpr) ||
        isBufferTextureIndexing(dyn_cast<CXXOperatorCallExpr>(subExpr)) ||
        isTextureMipsSampleIndexing(dyn_cast<CXXOperatorCallExpr>(subExpr))) {
      // By reaching here, it means the vector/matrix/Buffer/RWBuffer/RWTexture
      // element accessing operation is an lvalue. For vector element accessing,
      // if we generated a vector shuffle for it and trying to use it as a
      // rvalue, we cannot do the load here as normal. Need the upper nodes in
      // the AST tree to handle it properly. For matrix element accessing, load
      // should have already happened after creating access chain for each
      // element. For (RW)Buffer/RWTexture element accessing, load should have
      // already happened using OpImageFetch.
      return info;
    }

    // Using lvalue as rvalue means we need to OpLoad the contents from
    // the parameter/variable first.
    info.resultId = theBuilder.createLoad(
        typeTranslator.translateType(expr->getType(), info.layoutRule), info);
    return info;
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

    const auto vecSplat = createVectorSplat(subExpr, colCount);
    if (rowCount == 1)
      return vecSplat;

    const uint32_t matType = typeTranslator.translateType(toType);
    llvm::SmallVector<uint32_t, 4> vectors(size_t(rowCount), vecSplat);

    if (vecSplat.isConst) {
      return SpirvEvalInfo::withConst(
          theBuilder.getConstantComposite(matType, vectors));
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

SpirvEvalInfo
SPIRVEmitter::doCompoundAssignOperator(const CompoundAssignOperator *expr) {
  const auto opcode = expr->getOpcode();

  // Try to optimize floatMxN *= float and floatN *= float case
  if (opcode == BO_MulAssign) {
    if (const SpirvEvalInfo result = tryToGenFloatMatrixScale(expr))
      return result;
    if (const SpirvEvalInfo result = tryToGenFloatVectorScale(expr))
      return result;
  }

  const auto *rhs = expr->getRHS();
  const auto *lhs = expr->getLHS();

  SpirvEvalInfo lhsPtr = 0;
  const uint32_t resultType = typeTranslator.translateType(expr->getType());
  const auto result = processBinaryOp(lhs, rhs, opcode, resultType, &lhsPtr);
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

uint32_t SPIRVEmitter::processByteAddressBufferStructuredBufferGetDimensions(
    const CXXMemberCallExpr *expr) {
  const auto *object = expr->getImplicitObjectArgument();
  const auto objectId = loadIfGLValue(object);
  const auto type = object->getType();
  const bool isByteAddressBuffer = TypeTranslator::isByteAddressBuffer(type) ||
                                   TypeTranslator::isRWByteAddressBuffer(type);
  const bool isStructuredBuffer =
      TypeTranslator::isStructuredBuffer(type) ||
      TypeTranslator::isAppendStructuredBuffer(type) ||
      TypeTranslator::isConsumeStructuredBuffer(type);
  assert(isByteAddressBuffer || isStructuredBuffer);

  // (RW)ByteAddressBuffers/(RW)StructuredBuffers are represented as a structure
  // with only one member that is a runtime array. We need to perform
  // OpArrayLength on member 0.
  const auto uintType = theBuilder.getUint32Type();
  uint32_t length =
      theBuilder.createBinaryOp(spv::Op::OpArrayLength, uintType, objectId, 0);
  // For (RW)ByteAddressBuffers, GetDimensions() must return the array length
  // in bytes, but OpArrayLength returns the number of uints in the runtime
  // array. Therefore we must multiply the results by 4.
  if (isByteAddressBuffer) {
    length = theBuilder.createBinaryOp(spv::Op::OpIMul, uintType, length,
                                       theBuilder.getConstantUint32(4u));
  }
  theBuilder.createStore(doExpr(expr->getArg(0)), length);

  if (isStructuredBuffer) {
    // For (RW)StructuredBuffer, the stride of the runtime array (which is the
    // size of the struct) must also be written to the second argument.
    uint32_t size = 0, stride = 0;
    std::tie(std::ignore, size) = typeTranslator.getAlignmentAndSize(
        type, LayoutRule::GLSLStd430, /*isRowMajor*/ false, &stride);
    const auto sizeId = theBuilder.getConstantUint32(size);
    theBuilder.createStore(doExpr(expr->getArg(1)), sizeId);
  }

  return 0;
}

uint32_t SPIRVEmitter::processRWByteAddressBufferAtomicMethods(
    hlsl::IntrinsicOp opcode, const CXXMemberCallExpr *expr) {
  // The signature of RWByteAddressBuffer atomic methods are largely:
  // void Interlocked*(in UINT dest, in UINT value);
  // void Interlocked*(in UINT dest, in UINT value, out UINT original_value);

  const auto *object = expr->getImplicitObjectArgument();
  // We do not need to load the object since we are using its pointers.
  const auto objectInfo = doExpr(object);

  const auto uintType = theBuilder.getUint32Type();
  const uint32_t zero = theBuilder.getConstantUint32(0);

  const uint32_t offset = doExpr(expr->getArg(0));
  // Right shift by 2 to convert the byte offset to uint32_t offset
  const uint32_t address =
      theBuilder.createBinaryOp(spv::Op::OpShiftRightLogical, uintType, offset,
                                theBuilder.getConstantUint32(2));
  const auto ptrType =
      theBuilder.getPointerType(uintType, objectInfo.storageClass);
  const uint32_t ptr =
      theBuilder.createAccessChain(ptrType, objectInfo, {zero, address});

  const uint32_t scope = theBuilder.getConstantUint32(1); // Device

  const bool isCompareExchange =
      opcode == hlsl::IntrinsicOp::MOP_InterlockedCompareExchange;
  const bool isCompareStore =
      opcode == hlsl::IntrinsicOp::MOP_InterlockedCompareStore;

  if (isCompareExchange || isCompareStore) {
    const uint32_t comparator = doExpr(expr->getArg(1));
    const uint32_t originalVal = theBuilder.createAtomicCompareExchange(
        uintType, ptr, scope, zero, zero, doExpr(expr->getArg(2)), comparator);
    if (isCompareExchange)
      theBuilder.createStore(doExpr(expr->getArg(3)), originalVal);
  } else {
    const uint32_t value = doExpr(expr->getArg(1));
    const uint32_t originalVal = theBuilder.createAtomicOp(
        translateAtomicHlslOpcodeToSpirvOpcode(opcode), uintType, ptr, scope,
        zero, value);
    if (expr->getNumArgs() > 2)
      theBuilder.createStore(doExpr(expr->getArg(2)), originalVal);
  }

  return 0;
}

uint32_t
SPIRVEmitter::processBufferTextureGetDimensions(const CXXMemberCallExpr *expr) {
  theBuilder.requireCapability(spv::Capability::ImageQuery);
  const auto *object = expr->getImplicitObjectArgument();
  const auto objectId = loadIfGLValue(object);
  const auto type = object->getType();
  const auto *recType = type->getAs<RecordType>();
  assert(recType);
  const auto typeName = recType->getDecl()->getName();
  const auto numArgs = expr->getNumArgs();
  const Expr *mipLevel = nullptr, *numLevels = nullptr, *numSamples = nullptr;

  assert(TypeTranslator::isTexture(type) || TypeTranslator::isRWTexture(type) ||
         TypeTranslator::isBuffer(type) || TypeTranslator::isRWBuffer(type));

  // For Texture1D, arguments are either:
  // a) width
  // b) MipLevel, width, NumLevels

  // For Texture1DArray, arguments are either:
  // a) width, elements
  // b) MipLevel, width, elements, NumLevels

  // For Texture2D, arguments are either:
  // a) width, height
  // b) MipLevel, width, height, NumLevels

  // For Texture2DArray, arguments are either:
  // a) width, height, elements
  // b) MipLevel, width, height, elements, NumLevels

  // For Texture3D, arguments are either:
  // a) width, height, depth
  // b) MipLevel, width, height, depth, NumLevels

  // For Texture2DMS, arguments are: width, height, NumSamples

  // For Texture2DMSArray, arguments are: width, height, elements, NumSamples

  if ((typeName == "Texture1D" && numArgs > 1) ||
      (typeName == "Texture2D" && numArgs > 2) ||
      (typeName == "Texture3D" && numArgs > 3) ||
      (typeName == "Texture1DArray" && numArgs > 2) ||
      (typeName == "Texture2DArray" && numArgs > 3)) {
    mipLevel = expr->getArg(0);
    numLevels = expr->getArg(numArgs - 1);
  }
  if (TypeTranslator::isTextureMS(type)) {
    numSamples = expr->getArg(numArgs - 1);
  }

  uint32_t querySize = numArgs;
  // If numLevels arg is present, mipLevel must also be present. These are not
  // queried via ImageQuerySizeLod.
  if (numLevels)
    querySize -= 2;
  // If numLevels arg is present, mipLevel must also be present.
  else if (numSamples)
    querySize -= 1;

  const uint32_t uintId = theBuilder.getUint32Type();
  const uint32_t resultTypeId =
      querySize == 1 ? uintId : theBuilder.getVecType(uintId, querySize);

  // Only Texture types use ImageQuerySizeLod.
  // TextureMS, RWTexture, Buffers, RWBuffers use ImageQuerySize.
  uint32_t lod = 0;
  if (TypeTranslator::isTexture(type) && !numSamples) {
    if (mipLevel) {
      // For Texture types when mipLevel argument is present.
      lod = doExpr(mipLevel);
    } else {
      // For Texture types when mipLevel argument is omitted.
      lod = theBuilder.getConstantInt32(0);
    }
  }

  const uint32_t query =
      lod
          ? theBuilder.createBinaryOp(spv::Op::OpImageQuerySizeLod,
                                      resultTypeId, objectId, lod)
          : theBuilder.createUnaryOp(spv::Op::OpImageQuerySize, resultTypeId,
                                     objectId);

  if (querySize == 1) {
    const uint32_t argIndex = mipLevel ? 1 : 0;
    theBuilder.createStore(doExpr(expr->getArg(argIndex)), query);
  } else {
    for (uint32_t i = 0; i < querySize; ++i) {
      const uint32_t component =
          theBuilder.createCompositeExtract(uintId, query, {i});
      // If the first arg is the mipmap level, we must write the results
      // starting from Arg(i+1), not Arg(i).
      const uint32_t argIndex = mipLevel ? i + 1 : i;
      theBuilder.createStore(doExpr(expr->getArg(argIndex)), component);
    }
  }

  if (numLevels || numSamples) {
    const Expr *numLevelsSamplesArg = numLevels ? numLevels : numSamples;
    const spv::Op opcode =
        numLevels ? spv::Op::OpImageQueryLevels : spv::Op::OpImageQuerySamples;
    const uint32_t resultType =
        typeTranslator.translateType(numLevelsSamplesArg->getType());
    const uint32_t numLevelsSamplesQuery =
        theBuilder.createUnaryOp(opcode, resultType, objectId);
    theBuilder.createStore(doExpr(numLevelsSamplesArg), numLevelsSamplesQuery);
  }

  return 0;
}

uint32_t
SPIRVEmitter::processTextureLevelOfDetail(const CXXMemberCallExpr *expr) {
  // Possible signatures are as follows:
  // Texture1D(Array).CalculateLevelOfDetail(SamplerState S, float x);
  // Texture2D(Array).CalculateLevelOfDetail(SamplerState S, float2 xy);
  // TextureCube(Array).CalculateLevelOfDetail(SamplerState S, float3 xyz);
  // Texture3D.CalculateLevelOfDetail(SamplerState S, float3 xyz);
  // Return type is always a single float (LOD).
  assert(expr->getNumArgs() == 2u);
  theBuilder.requireCapability(spv::Capability::ImageQuery);
  const auto *object = expr->getImplicitObjectArgument();
  const uint32_t objectId = loadIfGLValue(object);
  const uint32_t samplerState = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  const uint32_t sampledImageType = theBuilder.getSampledImageType(
      typeTranslator.translateType(object->getType()));
  const uint32_t sampledImage = theBuilder.createBinaryOp(
      spv::Op::OpSampledImage, sampledImageType, objectId, samplerState);

  // The result type of OpImageQueryLod must be a float2.
  const uint32_t queryResultType =
      theBuilder.getVecType(theBuilder.getFloat32Type(), 2u);
  const uint32_t query = theBuilder.createBinaryOp(
      spv::Op::OpImageQueryLod, queryResultType, sampledImage, coordinate);
  // The first component of the float2 contains the mipmap array layer.
  return theBuilder.createCompositeExtract(theBuilder.getFloat32Type(), query,
                                           {0});
}

uint32_t SPIRVEmitter::processTextureGatherRGBACmpRGBA(
    const CXXMemberCallExpr *expr, const bool isCmp, const uint32_t component) {
  // Parameters for .Gather{Red|Green|Blue|Alpha}() are one of the following
  // two sets:
  // * SamplerState s, float2 location, int2 offset
  // * SamplerState s, float2 location, int2 offset0, int2 offset1,
  //   int offset2, int2 offset3
  //
  // An additional out uint status parameter can appear in both of the above,
  // which we does not support yet.
  //
  // Parameters for .GatherCmp{Red|Green|Blue|Alpha}() are one of the following
  // two sets:
  // * SamplerState s, float2 location, int2 offset
  // * SamplerState s, float2 location, int2 offset0, int2 offset1,
  //   int offset2, int2 offset3
  //
  // An additional out uint status parameter can appear in both of the above,
  // which we does not support yet.
  //
  // Return type is always a 4-component vector.
  const FunctionDecl *callee = expr->getDirectCallee();
  const auto numArgs = expr->getNumArgs();

  if (numArgs != 3 + isCmp && numArgs != 6 + isCmp) {
    emitError("unsupported '%0' method call with status parameter",
              expr->getExprLoc())
        << callee->getName() << expr->getSourceRange();
    return 0;
  }

  const auto *imageExpr = expr->getImplicitObjectArgument();

  const uint32_t image = loadIfGLValue(imageExpr);
  const uint32_t sampler = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  const uint32_t compareVal = isCmp ? doExpr(expr->getArg(2)) : 0;

  uint32_t constOffset = 0, varOffset = 0, constOffsets = 0;
  if (numArgs == 3 + isCmp) {
    // One offset parameter
    handleOptionalOffsetInMethodCall(expr, 2 + isCmp, &constOffset, &varOffset);
  } else {
    // Four offset parameters
    const auto offset0 = tryToEvaluateAsConst(expr->getArg(2 + isCmp));
    const auto offset1 = tryToEvaluateAsConst(expr->getArg(3 + isCmp));
    const auto offset2 = tryToEvaluateAsConst(expr->getArg(4 + isCmp));
    const auto offset3 = tryToEvaluateAsConst(expr->getArg(5 + isCmp));

    // Make sure we can generate the ConstOffsets image operands in SPIR-V.
    if (!offset0 || !offset1 || !offset2 || !offset3) {
      emitError("all offset parameters to '%0' method call must be constants",
                expr->getExprLoc())
          << callee->getName() << expr->getSourceRange();
      return 0;
    }
    const uint32_t v2i32 = theBuilder.getVecType(theBuilder.getInt32Type(), 2);
    const uint32_t offsetType =
        theBuilder.getArrayType(v2i32, theBuilder.getConstantUint32(4));
    constOffsets = theBuilder.getConstantComposite(
        offsetType, {offset0, offset1, offset2, offset3});
  }

  const auto retType = typeTranslator.translateType(callee->getReturnType());
  const auto imageType = typeTranslator.translateType(imageExpr->getType());

  return theBuilder.createImageGather(
      retType, imageType, image, sampler, coordinate,
      theBuilder.getConstantInt32(component), compareVal, constOffset,
      varOffset, constOffsets, /*sampleNumber*/ 0);
}

uint32_t SPIRVEmitter::processTextureGatherCmp(const CXXMemberCallExpr *expr) {
  // Signature:
  //
  // float4 GatherCmp(
  //   in SamplerComparisonState s,
  //   in float2 location,
  //   in float compare_value
  //   [,in int2 offset]
  // );
  const FunctionDecl *callee = expr->getDirectCallee();
  const auto numArgs = expr->getNumArgs();

  if (expr->getNumArgs() > 4) {
    emitError("unsupported '%0' method call with status parameter",
              expr->getExprLoc())
        << callee->getName() << expr->getSourceRange();
    return 0;
  }

  const auto *imageExpr = expr->getImplicitObjectArgument();

  const uint32_t image = loadIfGLValue(imageExpr);
  const uint32_t sampler = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  const uint32_t comparator = doExpr(expr->getArg(2));
  uint32_t constOffset = 0, varOffset = 0;
  handleOptionalOffsetInMethodCall(expr, 3, &constOffset, &varOffset);

  const auto retType = typeTranslator.translateType(callee->getReturnType());
  const auto imageType = typeTranslator.translateType(imageExpr->getType());

  return theBuilder.createImageGather(
      retType, imageType, image, sampler, coordinate,
      /*component*/ 0, comparator, constOffset, varOffset, /*constOffsets*/ 0,
      /*sampleNumber*/ 0);
}

uint32_t SPIRVEmitter::processBufferTextureLoad(const Expr *object,
                                                const uint32_t locationId,
                                                uint32_t constOffset,
                                                uint32_t varOffset,
                                                uint32_t lod) {
  // Loading for Buffer and RWBuffer translates to an OpImageFetch.
  // The result type of an OpImageFetch must be a vec4 of float or int.
  const auto type = object->getType();
  assert(TypeTranslator::isBuffer(type) || TypeTranslator::isRWBuffer(type) ||
         TypeTranslator::isTexture(type) || TypeTranslator::isRWTexture(type));
  const bool doFetch =
      TypeTranslator::isBuffer(type) || TypeTranslator::isTexture(type);

  const uint32_t objectId = loadIfGLValue(object);

  // For Texture2DMS and Texture2DMSArray, Sample must be used rather than Lod.
  uint32_t sampleNumber = 0;
  if (TypeTranslator::isTextureMS(type)) {
    sampleNumber = lod;
    lod = 0;
  }

  const auto sampledType = hlsl::GetHLSLResourceResultType(type);
  QualType elemType = sampledType;
  uint32_t elemCount = 1;
  uint32_t elemTypeId = 0;
  (void)TypeTranslator::isVectorType(sampledType, &elemType, &elemCount);
  if (elemType->isFloatingType()) {
    elemTypeId = theBuilder.getFloat32Type();
  } else if (elemType->isSignedIntegerType()) {
    elemTypeId = theBuilder.getInt32Type();
  } else if (elemType->isUnsignedIntegerType()) {
    elemTypeId = theBuilder.getUint32Type();
  } else {
    emitError("Unimplemented Buffer/Texture type");
    return 0;
  }
  const uint32_t resultTypeId =
      elemCount == 1 ? elemTypeId
                     : theBuilder.getVecType(elemTypeId, elemCount);

  // OpImageFetch can only fetch a vector of 4 elements. OpImageRead can load a
  // vector of any size.
  const uint32_t fetchTypeId = theBuilder.getVecType(elemTypeId, 4u);
  const uint32_t texel = theBuilder.createImageFetchOrRead(
      doFetch, doFetch ? fetchTypeId : resultTypeId, objectId, locationId, lod,
      constOffset, varOffset, /*constOffsets*/ 0, sampleNumber);

  // OpImageRead can load a vector of any size. So we can return the result of
  // the instruction directly.
  if (!doFetch) {
    return texel;
  }

  // OpImageFetch can only fetch vec4. If the result type is a vec1, vec2, or
  // vec3, some extra processing (extraction) is required.
  switch (elemCount) {
  case 1:
    return theBuilder.createCompositeExtract(elemTypeId, texel, {0});
  case 2:
    return theBuilder.createVectorShuffle(resultTypeId, texel, texel, {0, 1});
  case 3:
    return theBuilder.createVectorShuffle(resultTypeId, texel, texel,
                                          {0, 1, 2});
  case 4:
    return texel;
  }
  llvm_unreachable("Element count of a vector must be 1, 2, 3, or 4.");
}

uint32_t SPIRVEmitter::processByteAddressBufferLoadStore(
    const CXXMemberCallExpr *expr, uint32_t numWords, bool doStore) {
  uint32_t resultId = 0;
  const auto object = expr->getImplicitObjectArgument();
  const auto type = object->getType();
  const auto objectInfo = doExpr(object);
  assert(numWords >= 1 && numWords <= 4);
  if (doStore) {
    assert(typeTranslator.isRWByteAddressBuffer(type));
    assert(expr->getNumArgs() == 2);
  } else {
    assert(typeTranslator.isRWByteAddressBuffer(type) ||
           typeTranslator.isByteAddressBuffer(type));
    if (expr->getNumArgs() == 2) {

      emitError("Load(in Address, out Status) has not been implemented for "
                "(RW)ByteAddressBuffer yet.");
      return 0;
    }
  }
  const Expr *addressExpr = expr->getArg(0);
  const uint32_t byteAddress = doExpr(addressExpr);
  const uint32_t addressTypeId =
      typeTranslator.translateType(addressExpr->getType());

  // Do a OpShiftRightLogical by 2 (divide by 4 to get aligned memory
  // access). The AST always casts the address to unsinged integer, so shift
  // by unsinged integer 2.
  const uint32_t constUint2 = theBuilder.getConstantUint32(2);
  const uint32_t address = theBuilder.createBinaryOp(
      spv::Op::OpShiftRightLogical, addressTypeId, byteAddress, constUint2);

  // Perform access chain into the RWByteAddressBuffer.
  // First index must be zero (member 0 of the struct is a
  // runtimeArray). The second index passed to OpAccessChain should be
  // the address.
  const uint32_t uintTypeId = theBuilder.getUint32Type();
  const uint32_t ptrType =
      theBuilder.getPointerType(uintTypeId, objectInfo.storageClass);
  const uint32_t constUint0 = theBuilder.getConstantUint32(0);

  if (doStore) {
    const uint32_t valuesId = doExpr(expr->getArg(1));
    uint32_t curStoreAddress = address;
    for (uint32_t wordCounter = 0; wordCounter < numWords; ++wordCounter) {
      // Extract a 32-bit word from the input.
      const uint32_t curValue = numWords == 1
                                    ? valuesId
                                    : theBuilder.createCompositeExtract(
                                          uintTypeId, valuesId, {wordCounter});

      // Update the output address if necessary.
      if (wordCounter > 0) {
        const uint32_t offset = theBuilder.getConstantUint32(wordCounter);
        curStoreAddress = theBuilder.createBinaryOp(
            spv::Op::OpIAdd, addressTypeId, address, offset);
      }

      // Store the word to the right address at the output.
      const uint32_t storePtr = theBuilder.createAccessChain(
          ptrType, objectInfo, {constUint0, curStoreAddress});
      theBuilder.createStore(storePtr, curValue);
    }
  } else {
    uint32_t loadPtr = theBuilder.createAccessChain(ptrType, objectInfo,
                                                    {constUint0, address});
    resultId = theBuilder.createLoad(uintTypeId, loadPtr);
    if (numWords > 1) {
      // Load word 2, 3, and 4 where necessary. Use OpCompositeConstruct to
      // return a vector result.
      llvm::SmallVector<uint32_t, 4> values;
      values.push_back(resultId);
      for (uint32_t wordCounter = 2; wordCounter <= numWords; ++wordCounter) {
        const uint32_t offset = theBuilder.getConstantUint32(wordCounter - 1);
        const uint32_t newAddress = theBuilder.createBinaryOp(
            spv::Op::OpIAdd, addressTypeId, address, offset);
        loadPtr = theBuilder.createAccessChain(ptrType, objectInfo,
                                               {constUint0, newAddress});
        values.push_back(theBuilder.createLoad(uintTypeId, loadPtr));
      }
      const uint32_t resultType =
          theBuilder.getVecType(addressTypeId, numWords);
      resultId = theBuilder.createCompositeConstruct(resultType, values);
    }
  }
  return resultId;
}

SpirvEvalInfo
SPIRVEmitter::processStructuredBufferLoad(const CXXMemberCallExpr *expr) {
  if (expr->getNumArgs() == 2) {
    emitError("Load(int, int) unimplemented for (RW)StructuredBuffer");
    return 0;
  }

  const auto *buffer = expr->getImplicitObjectArgument();
  auto info = doExpr(buffer);

  const QualType structType =
      hlsl::GetHLSLResourceResultType(buffer->getType());
  const uint32_t ptrType = theBuilder.getPointerType(
      typeTranslator.translateType(structType, info.layoutRule),
      info.storageClass);

  const uint32_t zero = theBuilder.getConstantInt32(0);
  const uint32_t index = doExpr(expr->getArg(0));

  info.resultId = theBuilder.createAccessChain(ptrType, info, {zero, index});
  return info;
}

uint32_t SPIRVEmitter::incDecRWACSBufferCounter(const CXXMemberCallExpr *expr,
                                                bool isInc) {
  const uint32_t i32Type = theBuilder.getInt32Type();
  const uint32_t one = theBuilder.getConstantUint32(1);  // As scope: Device
  const uint32_t zero = theBuilder.getConstantUint32(0); // As memory sema: None
  const uint32_t sOne = theBuilder.getConstantInt32(1);

  const auto *object =
      expr->getImplicitObjectArgument()->IgnoreParenNoopCasts(astContext);
  const auto *buffer = cast<DeclRefExpr>(object)->getDecl();

  const uint32_t counterVar = declIdMapper.getOrCreateCounterId(buffer);
  const uint32_t counterPtrType = theBuilder.getPointerType(
      theBuilder.getInt32Type(), spv::StorageClass::Uniform);
  const uint32_t counterPtr =
      theBuilder.createAccessChain(counterPtrType, counterVar, {zero});

  uint32_t index = 0;
  if (isInc) {
    index = theBuilder.createAtomicOp(spv::Op::OpAtomicIAdd, i32Type,
                                      counterPtr, one, zero, sOne);
  } else {
    // Note that OpAtomicISub returns the value before the subtraction;
    // so we need to do substraction again with OpAtomicISub's return value.
    const auto prev = theBuilder.createAtomicOp(spv::Op::OpAtomicISub, i32Type,
                                                counterPtr, one, zero, sOne);
    index = theBuilder.createBinaryOp(spv::Op::OpISub, i32Type, prev, sOne);
  }

  return index;
}

SpirvEvalInfo
SPIRVEmitter::processACSBufferAppendConsume(const CXXMemberCallExpr *expr) {
  const bool isAppend = expr->getNumArgs() == 1;

  const uint32_t zero = theBuilder.getConstantUint32(0);

  const auto *object =
      expr->getImplicitObjectArgument()->IgnoreParenNoopCasts(astContext);
  const auto *buffer = cast<DeclRefExpr>(object)->getDecl();

  uint32_t index = incDecRWACSBufferCounter(expr, isAppend);

  auto bufferInfo = declIdMapper.getDeclResultId(buffer);

  const auto bufferElemTy = hlsl::GetHLSLResourceResultType(object->getType());
  const uint32_t bufferElemType =
      typeTranslator.translateType(bufferElemTy, bufferInfo.layoutRule);
  // Get the pointer inside the {Append|Consume}StructuredBuffer
  const uint32_t bufferElemPtrType =
      theBuilder.getPointerType(bufferElemType, bufferInfo.storageClass);
  const uint32_t bufferElemPtr = theBuilder.createAccessChain(
      bufferElemPtrType, bufferInfo.resultId, {zero, index});

  if (isAppend) {
    // Write out the value
    bufferInfo.resultId = bufferElemPtr;
    storeValue(bufferInfo, doExpr(expr->getArg(0)), bufferElemTy);

    return 0;
  } else {
    // Somehow if the element type is not a structure type, the return value
    // of .Consume() is not labelled as xvalue. That will cause OpLoad
    // instruction missing. Load directly here.
    if (bufferElemTy->isStructureType())
      bufferInfo.resultId = bufferElemPtr;
    else
      bufferInfo.resultId =
          theBuilder.createLoad(bufferElemType, bufferElemPtr);
    return bufferInfo;
  }
}

SpirvEvalInfo SPIRVEmitter::doCXXMemberCallExpr(const CXXMemberCallExpr *expr) {
  const FunctionDecl *callee = expr->getDirectCallee();

  llvm::StringRef group;
  uint32_t opcode = static_cast<uint32_t>(hlsl::IntrinsicOp::Num_Intrinsics);

  if (hlsl::GetIntrinsicOp(callee, opcode, group)) {
    return processIntrinsicMemberCall(expr,
                                      static_cast<hlsl::IntrinsicOp>(opcode));
  }

  return processCall(expr);
}
void SPIRVEmitter::handleOptionalOffsetInMethodCall(
    const CXXMemberCallExpr *expr, uint32_t index, uint32_t *constOffset,
    uint32_t *varOffset) {
  *constOffset = *varOffset = 0; // Initialize both first

  if (expr->getNumArgs() == index + 1) { // Has offset argument
    if (*constOffset = tryToEvaluateAsConst(expr->getArg(index)))
      return; // Constant offset
    else
      *varOffset = doExpr(expr->getArg(index));
  }
};

SpirvEvalInfo
SPIRVEmitter::processIntrinsicMemberCall(const CXXMemberCallExpr *expr,
                                         hlsl::IntrinsicOp opcode) {
  using namespace hlsl;

  switch (opcode) {
  case IntrinsicOp::MOP_Sample:
    return processTextureSampleGather(expr, /*isSample=*/true);
  case IntrinsicOp::MOP_Gather:
    return processTextureSampleGather(expr, /*isSample=*/false);
  case IntrinsicOp::MOP_SampleBias:
    return processTextureSampleBiasLevel(expr, /*isBias=*/true);
  case IntrinsicOp::MOP_SampleLevel:
    return processTextureSampleBiasLevel(expr, /*isBias=*/false);
  case IntrinsicOp::MOP_SampleGrad:
    return processTextureSampleGrad(expr);
  case IntrinsicOp::MOP_SampleCmp:
    return processTextureSampleCmpCmpLevelZero(expr, /*isCmp=*/true);
  case IntrinsicOp::MOP_SampleCmpLevelZero:
    return processTextureSampleCmpCmpLevelZero(expr, /*isCmp=*/false);
  case IntrinsicOp::MOP_GatherRed:
    return processTextureGatherRGBACmpRGBA(expr, /*isCmp=*/false, 0);
  case IntrinsicOp::MOP_GatherGreen:
    return processTextureGatherRGBACmpRGBA(expr, /*isCmp=*/false, 1);
  case IntrinsicOp::MOP_GatherBlue:
    return processTextureGatherRGBACmpRGBA(expr, /*isCmp=*/false, 2);
  case IntrinsicOp::MOP_GatherAlpha:
    return processTextureGatherRGBACmpRGBA(expr, /*isCmp=*/false, 3);
  case IntrinsicOp::MOP_GatherCmp:
    return processTextureGatherCmp(expr);
  case IntrinsicOp::MOP_GatherCmpRed:
    return processTextureGatherRGBACmpRGBA(expr, /*isCmp=*/true, 0);
  case IntrinsicOp::MOP_Load:
    return processBufferTextureLoad(expr);
  case IntrinsicOp::MOP_Load2:
    return processByteAddressBufferLoadStore(expr, 2, /*doStore*/ false);
  case IntrinsicOp::MOP_Load3:
    return processByteAddressBufferLoadStore(expr, 3, /*doStore*/ false);
  case IntrinsicOp::MOP_Load4:
    return processByteAddressBufferLoadStore(expr, 4, /*doStore*/ false);
  case IntrinsicOp::MOP_Store:
    return processByteAddressBufferLoadStore(expr, 1, /*doStore*/ true);
  case IntrinsicOp::MOP_Store2:
    return processByteAddressBufferLoadStore(expr, 2, /*doStore*/ true);
  case IntrinsicOp::MOP_Store3:
    return processByteAddressBufferLoadStore(expr, 3, /*doStore*/ true);
  case IntrinsicOp::MOP_Store4:
    return processByteAddressBufferLoadStore(expr, 4, /*doStore*/ true);
  case IntrinsicOp::MOP_GetDimensions:
    return processGetDimensions(expr);
  case IntrinsicOp::MOP_CalculateLevelOfDetail:
    return processTextureLevelOfDetail(expr);
  case IntrinsicOp::MOP_IncrementCounter:
    return theBuilder.createUnaryOp(
        spv::Op::OpBitcast, theBuilder.getUint32Type(),
        incDecRWACSBufferCounter(expr, /*isInc*/ true));
  case IntrinsicOp::MOP_DecrementCounter:
    return theBuilder.createUnaryOp(
        spv::Op::OpBitcast, theBuilder.getUint32Type(),
        incDecRWACSBufferCounter(expr, /*isInc*/ false));
  case IntrinsicOp::MOP_Append:
  case IntrinsicOp::MOP_Consume:
    return processACSBufferAppendConsume(expr);
  case IntrinsicOp::MOP_InterlockedAdd:
  case IntrinsicOp::MOP_InterlockedAnd:
  case IntrinsicOp::MOP_InterlockedOr:
  case IntrinsicOp::MOP_InterlockedXor:
  case IntrinsicOp::MOP_InterlockedUMax:
  case IntrinsicOp::MOP_InterlockedUMin:
  case IntrinsicOp::MOP_InterlockedMax:
  case IntrinsicOp::MOP_InterlockedMin:
  case IntrinsicOp::MOP_InterlockedExchange:
  case IntrinsicOp::MOP_InterlockedCompareExchange:
  case IntrinsicOp::MOP_InterlockedCompareStore:
    return processRWByteAddressBufferAtomicMethods(opcode, expr);
  }

  emitError("HLSL intrinsic member call unimplemented: %0")
      << expr->getDirectCallee()->getName();
  return 0;
}

uint32_t SPIRVEmitter::processTextureSampleGather(const CXXMemberCallExpr *expr,
                                                  const bool isSample) {
  // Signatures:
  // DXGI_FORMAT Object.Sample(sampler_state S,
  //                           float Location
  //                           [, int Offset]);
  //
  // <Template Type>4 Object.Gather(sampler_state S,
  //                                float2|3|4 Location
  //                                [, int2 Offset]);

  const auto *imageExpr = expr->getImplicitObjectArgument();

  const uint32_t imageType = typeTranslator.translateType(imageExpr->getType());

  const uint32_t image = loadIfGLValue(imageExpr);
  const uint32_t sampler = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  // .Sample()/.Gather() has a third optional paramter for offset.
  uint32_t constOffset = 0, varOffset = 0;
  handleOptionalOffsetInMethodCall(expr, 2, &constOffset, &varOffset);

  const auto retType =
      typeTranslator.translateType(expr->getDirectCallee()->getReturnType());

  if (isSample) {
    return theBuilder.createImageSample(
        retType, imageType, image, sampler, coordinate, /*compareVal*/ 0,
        /*bias*/ 0, /*lod*/ 0, std::make_pair(0, 0), constOffset, varOffset,
        /*constOffsets*/ 0, /*sampleNumber*/ 0);
  } else {
    return theBuilder.createImageGather(
        retType, imageType, image, sampler, coordinate,
        // .Gather() doc says we return four components of red data.
        theBuilder.getConstantInt32(0), /*compareVal*/ 0, constOffset,
        varOffset, /*constOffsets*/ 0, /*sampleNumber*/ 0);
  }
}

uint32_t
SPIRVEmitter::processTextureSampleBiasLevel(const CXXMemberCallExpr *expr,
                                            const bool isBias) {
  // Signatures:
  // DXGI_FORMAT Object.SampleBias(sampler_state S,
  //                               float Location,
  //                               float Bias
  //                               [, int Offset]);
  //
  // DXGI_FORMAT Object.SampleLevel(sampler_state S,
  //                                float Location,
  //                                float LOD
  //                                [, int Offset]);
  const auto *imageExpr = expr->getImplicitObjectArgument();

  const uint32_t imageType = typeTranslator.translateType(imageExpr->getType());

  const uint32_t image = loadIfGLValue(imageExpr);
  const uint32_t sampler = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  uint32_t lod = 0;
  uint32_t bias = 0;
  if (isBias) {
    bias = doExpr(expr->getArg(2));
  } else {
    lod = doExpr(expr->getArg(2));
  }
  // .Bias()/.SampleLevel() has a fourth optional paramter for offset.
  uint32_t constOffset = 0, varOffset = 0;
  handleOptionalOffsetInMethodCall(expr, 3, &constOffset, &varOffset);

  const auto retType =
      typeTranslator.translateType(expr->getDirectCallee()->getReturnType());

  return theBuilder.createImageSample(
      retType, imageType, image, sampler, coordinate, /*compareVal*/ 0, bias,
      lod, std::make_pair(0, 0), constOffset, varOffset, /*constOffsets*/ 0,
      /*sampleNumber*/ 0);
}

uint32_t SPIRVEmitter::processTextureSampleGrad(const CXXMemberCallExpr *expr) {
  // Signature:
  // DXGI_FORMAT Object.SampleGrad(sampler_state S,
  //                               float Location,
  //                               float DDX,
  //                               float DDY
  //                               [, int Offset]);

  const auto *imageExpr = expr->getImplicitObjectArgument();

  const uint32_t imageType = typeTranslator.translateType(imageExpr->getType());

  const uint32_t image = loadIfGLValue(imageExpr);
  const uint32_t sampler = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  const uint32_t ddx = doExpr(expr->getArg(2));
  const uint32_t ddy = doExpr(expr->getArg(3));
  // .SampleGrad() has a fifth optional paramter for offset.
  uint32_t constOffset = 0, varOffset = 0;
  handleOptionalOffsetInMethodCall(expr, 4, &constOffset, &varOffset);

  const auto retType =
      typeTranslator.translateType(expr->getDirectCallee()->getReturnType());

  return theBuilder.createImageSample(
      retType, imageType, image, sampler, coordinate, /*compareVal*/ 0,
      /*bias*/ 0, /*lod*/ 0, std::make_pair(ddx, ddy), constOffset, varOffset,
      /*constOffsets*/ 0,
      /*sampleNumber*/ 0);
}

uint32_t
SPIRVEmitter::processTextureSampleCmpCmpLevelZero(const CXXMemberCallExpr *expr,
                                                  const bool isCmp) {
  // .SampleCmp() Signature:
  //
  // float Object.SampleCmp(
  //   SamplerComparisonState S,
  //   float Location,
  //   float CompareValue,
  //   [int Offset]
  // );
  //
  // .SampleCmpLevelZero() is identical to .SampleCmp() on mipmap level 0 only.
  const auto *imageExpr = expr->getImplicitObjectArgument();

  const uint32_t image = loadIfGLValue(imageExpr);
  const uint32_t sampler = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  const uint32_t compareVal = doExpr(expr->getArg(2));
  // .SampleCmp() has a fourth optional paramter for offset.
  uint32_t constOffset = 0, varOffset = 0;
  handleOptionalOffsetInMethodCall(expr, 3, &constOffset, &varOffset);
  const uint32_t lod = isCmp ? 0 : theBuilder.getConstantFloat32(0);

  const auto retType =
      typeTranslator.translateType(expr->getDirectCallee()->getReturnType());
  const auto imageType = typeTranslator.translateType(imageExpr->getType());

  return theBuilder.createImageSample(
      retType, imageType, image, sampler, coordinate, compareVal, /*bias*/ 0,
      lod, std::make_pair(0, 0), constOffset, varOffset,
      /*constOffsets*/ 0, /*sampleNumber*/ 0);
}

SpirvEvalInfo
SPIRVEmitter::processBufferTextureLoad(const CXXMemberCallExpr *expr) {
  // Signature:
  // ret Object.Load(int Location
  //                 [, int SampleIndex,]
  //                 [, int Offset]);

  const auto *object = expr->getImplicitObjectArgument();
  const auto *location = expr->getArg(0);
  const auto objectType = object->getType();

  if (typeTranslator.isRWByteAddressBuffer(objectType) ||
      typeTranslator.isByteAddressBuffer(objectType))
    return processByteAddressBufferLoadStore(expr, 1, /*doStore*/ false);

  if (TypeTranslator::isStructuredBuffer(objectType))
    return processStructuredBufferLoad(expr);

  if (TypeTranslator::isBuffer(objectType) ||
      TypeTranslator::isRWBuffer(objectType) ||
      TypeTranslator::isRWTexture(objectType))
    return processBufferTextureLoad(object, doExpr(location));

  if (TypeTranslator::isTexture(objectType)) {
    // .Load() has a second optional paramter for offset.
    const auto locationId = doExpr(location);
    uint32_t constOffset = 0, varOffset = 0;
    uint32_t coordinate = locationId, lod = 0;

    if (TypeTranslator::isTextureMS(objectType)) {
      // SampleIndex is only available when the Object is of Texture2DMS or
      // Texture2DMSArray types. Under those cases, Offset will be the third
      // parameter (index 2).
      lod = doExpr(expr->getArg(1));
      handleOptionalOffsetInMethodCall(expr, 2, &constOffset, &varOffset);
    } else {
      // For Texture Load() functions, the location parameter is a vector
      // that consists of both the coordinate and the mipmap level (via the
      // last vector element). We need to split it here since the
      // OpImageFetch SPIR-V instruction encodes them as separate arguments.
      splitVecLastElement(location->getType(), locationId, &coordinate, &lod);
      // For textures other than Texture2DMS(Array), offset should be the
      // second parameter (index 1).
      handleOptionalOffsetInMethodCall(expr, 1, &constOffset, &varOffset);
    }

    return processBufferTextureLoad(object, coordinate, constOffset, varOffset,
                                    lod);
  }
  emitError("Load() is not implemented for the given object type.");
  return 0;
}

uint32_t SPIRVEmitter::processGetDimensions(const CXXMemberCallExpr *expr) {
  const auto objectType = expr->getImplicitObjectArgument()->getType();
  if (TypeTranslator::isTexture(objectType) ||
      TypeTranslator::isRWTexture(objectType) ||
      TypeTranslator::isBuffer(objectType) ||
      TypeTranslator::isRWBuffer(objectType)) {
    return processBufferTextureGetDimensions(expr);
  } else if (TypeTranslator::isByteAddressBuffer(objectType) ||
             TypeTranslator::isRWByteAddressBuffer(objectType) ||
             TypeTranslator::isStructuredBuffer(objectType) ||
             TypeTranslator::isAppendStructuredBuffer(objectType) ||
             TypeTranslator::isConsumeStructuredBuffer(objectType)) {
    return processByteAddressBufferStructuredBufferGetDimensions(expr);
  } else {
    emitError("GetDimensions not implmented for the given type yet.");
    return 0;
  }
}

SpirvEvalInfo
SPIRVEmitter::doCXXOperatorCallExpr(const CXXOperatorCallExpr *expr) {
  { // Handle Buffer/RWBuffer/Texture/RWTexture indexing
    const Expr *baseExpr = nullptr;
    const Expr *indexExpr = nullptr;
    const Expr *lodExpr = nullptr;

    // For Textures, regular indexing (operator[]) uses slice 0.
    if (isBufferTextureIndexing(expr, &baseExpr, &indexExpr)) {
      const uint32_t lod = TypeTranslator::isTexture(baseExpr->getType())
                               ? theBuilder.getConstantUint32(0)
                               : 0;
      return processBufferTextureLoad(baseExpr, doExpr(indexExpr),
                                      /*constOffset*/ 0, /*varOffset*/ 0, lod);
    }
    // .mips[][] or .sample[][] must use the correct slice.
    if (isTextureMipsSampleIndexing(expr, &baseExpr, &indexExpr, &lodExpr)) {
      const uint32_t lod = doExpr(lodExpr);
      return processBufferTextureLoad(baseExpr, doExpr(indexExpr),
                                      /*constOffset*/ 0, /*varOffset*/ 0, lod);
    }
  }

  llvm::SmallVector<uint32_t, 4> indices;
  const Expr *baseExpr = collectArrayStructIndices(expr, &indices);

  auto base = doExpr(baseExpr);
  if (indices.empty())
    return base; // For indexing into size-1 vectors and 1xN matrices
  // If we are indexing into a rvalue, to use OpAccessChain, we first need
  // to create a local variable to hold the rvalue.
  //
  // TODO: We can optimize the codegen by emitting OpCompositeExtract if
  // all indices are contant integers.
  if (!baseExpr->isGLValue()) {
    const uint32_t baseType = typeTranslator.translateType(baseExpr->getType());
    const uint32_t tempVar = theBuilder.addFnVar(baseType, "temp.var");
    theBuilder.createStore(tempVar, base);
    base = tempVar;
  }

  const uint32_t ptrType = theBuilder.getPointerType(
      typeTranslator.translateType(expr->getType(), base.layoutRule),
      base.storageClass);
  base.resultId = theBuilder.createAccessChain(ptrType, base, indices);

  return base;
}

SpirvEvalInfo
SPIRVEmitter::doExtMatrixElementExpr(const ExtMatrixElementExpr *expr) {
  const Expr *baseExpr = expr->getBase();
  const auto baseInfo = doExpr(baseExpr);
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

      const uint32_t ptrType =
          theBuilder.getPointerType(elemType, baseInfo.storageClass);
      if (!indices.empty()) {
        // Load the element via access chain
        elem = theBuilder.createAccessChain(ptrType, baseInfo, indices);
      } else {
        // The matrix is of size 1x1. No need to use access chain, base should
        // be the source pointer.
        elem = baseInfo;
      }
      elem = theBuilder.createLoad(elemType, elem);
    } else { // e.g., (mat1 + mat2)._m11
      elem = theBuilder.createCompositeExtract(elemType, baseInfo, indices);
    }
    elements.push_back(elem);
  }

  if (elements.size() == 1)
    return elements.front();

  const uint32_t vecType = theBuilder.getVecType(elemType, elements.size());
  return theBuilder.createCompositeConstruct(vecType, elements);
}

SpirvEvalInfo
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
      const auto baseInfo = doExpr(baseExpr);
      const uint32_t ptrType =
          theBuilder.getPointerType(type, baseInfo.storageClass);
      const uint32_t index = theBuilder.getConstantInt32(accessor.Swz0);
      // We need a lvalue here. Do not try to load.
      return theBuilder.createAccessChain(ptrType, baseInfo, {index});
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

SpirvEvalInfo SPIRVEmitter::doInitListExpr(const InitListExpr *expr) {
  if (const uint32_t id = tryToEvaluateAsConst(expr))
    return id;

  return InitListHandler(*this).process(expr);
}

SpirvEvalInfo SPIRVEmitter::doMemberExpr(const MemberExpr *expr) {
  llvm::SmallVector<uint32_t, 4> indices;

  const Expr *base = collectArrayStructIndices(expr, &indices);
  auto info = doExpr(base);

  if (!indices.empty()) {
    const uint32_t ptrType = theBuilder.getPointerType(
        typeTranslator.translateType(expr->getType(), info.layoutRule),
        info.storageClass);
    info.resultId = theBuilder.createAccessChain(ptrType, info, indices);
  }

  return info;
}

SpirvEvalInfo SPIRVEmitter::doUnaryOperator(const UnaryOperator *expr) {
  const auto opcode = expr->getOpcode();
  const auto *subExpr = expr->getSubExpr();
  const auto subType = subExpr->getType();
  auto subValue = doExpr(subExpr);
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

SpirvEvalInfo SPIRVEmitter::processAssignment(const Expr *lhs,
                                              const SpirvEvalInfo &rhs,
                                              const bool isCompoundAssignment,
                                              SpirvEvalInfo lhsPtr) {
  // Assigning to vector swizzling should be handled differently.
  if (const SpirvEvalInfo result = tryToAssignToVectorElements(lhs, rhs))
    return result;

  // Assigning to matrix swizzling should be handled differently.
  if (const SpirvEvalInfo result = tryToAssignToMatrixElements(lhs, rhs))
    return result;

  // Assigning to a RWBuffer/RWTexture should be handled differently.
  if (const SpirvEvalInfo result = tryToAssignToRWBufferRWTexture(lhs, rhs))
    return result;

  // Normal assignment procedure

  if (!lhsPtr.resultId)
    lhsPtr = doExpr(lhs);

  storeValue(lhsPtr, rhs, lhs->getType());

  // Plain assignment returns a rvalue, while compound assignment returns
  // lvalue.
  return isCompoundAssignment ? lhsPtr : rhs;
}

void SPIRVEmitter::storeValue(const SpirvEvalInfo &lhsPtr,
                              const SpirvEvalInfo &rhsVal,
                              const QualType valType) {
  // If lhs and rhs has the same memory layout, we should be safe to load
  // from rhs and directly store into lhs and avoid decomposing rhs.
  // TODO: is this optimization always correct?
  if (lhsPtr.layoutRule == rhsVal.layoutRule ||
      typeTranslator.isScalarType(valType) ||
      typeTranslator.isVectorType(valType) ||
      typeTranslator.isMxNMatrix(valType)) {
    theBuilder.createStore(lhsPtr, rhsVal);
  } else if (const auto *recordType = valType->getAs<RecordType>()) {
    uint32_t index = 0;
    for (const auto *decl : recordType->getDecl()->decls()) {
      // Ignore implicit generated struct declarations/constructors/destructors.
      if (decl->isImplicit())
        continue;

      const auto *field = cast<FieldDecl>(decl);
      assert(field);

      const auto subRhsValType =
          typeTranslator.translateType(field->getType(), rhsVal.layoutRule);
      const auto subRhsVal =
          theBuilder.createCompositeExtract(subRhsValType, rhsVal, {index});
      const auto subLhsPtrType = theBuilder.getPointerType(
          typeTranslator.translateType(field->getType(), lhsPtr.layoutRule),
          lhsPtr.storageClass);
      const auto subLhsPtr = theBuilder.createAccessChain(
          subLhsPtrType, lhsPtr, {theBuilder.getConstantUint32(index)});

      storeValue(lhsPtr.substResultId(subLhsPtr),
                 rhsVal.substResultId(subRhsVal), field->getType());
      ++index;
    }
  } else if (const auto *arrayType =
                 astContext.getAsConstantArrayType(valType)) {
    const auto elemType = arrayType->getElementType();
    // TODO: handle extra large array size?
    const auto size =
        static_cast<uint32_t>(arrayType->getSize().getZExtValue());

    for (uint32_t i = 0; i < size; ++i) {
      const auto subRhsValType =
          typeTranslator.translateType(elemType, rhsVal.layoutRule);
      const auto subRhsVal =
          theBuilder.createCompositeExtract(subRhsValType, rhsVal, {i});
      const auto subLhsPtrType = theBuilder.getPointerType(
          typeTranslator.translateType(elemType, lhsPtr.layoutRule),
          lhsPtr.storageClass);
      const auto subLhsPtr = theBuilder.createAccessChain(
          subLhsPtrType, lhsPtr, {theBuilder.getConstantUint32(i)});

      storeValue(lhsPtr.substResultId(subLhsPtr),
                 rhsVal.substResultId(subRhsVal), elemType);
    }
  } else {
    emitError("storing value of type %0 unimplemented") << valType;
  }
}

SpirvEvalInfo SPIRVEmitter::processBinaryOp(const Expr *lhs, const Expr *rhs,
                                            const BinaryOperatorKind opcode,
                                            const uint32_t resultType,
                                            SpirvEvalInfo *lhsInfo,
                                            const spv::Op mandateGenOpcode) {
  // If the operands are of matrix type, we need to dispatch the operation
  // onto each element vector iff the operands are not degenerated matrices
  // and we don't have a matrix specific SPIR-V instruction for the operation.
  if (!isSpirvMatrixOp(mandateGenOpcode) &&
      TypeTranslator::isSpirvAcceptableMatrixType(lhs->getType())) {
    return processMatrixBinaryOp(lhs, rhs, opcode);
  }

  // Comma operator works differently from other binary operations as there is
  // no SPIR-V instruction for it. For each comma, we must evaluate lhs and rhs
  // respectively, and return the results of rhs.
  if (opcode == BO_Comma) {
    (void)doExpr(lhs);
    return doExpr(rhs);
  }

  const spv::Op spvOp = (mandateGenOpcode == spv::Op::Max)
                            ? translateOp(opcode, lhs->getType())
                            : mandateGenOpcode;

  SpirvEvalInfo rhsVal = 0, lhsPtr = 0, lhsVal = 0;
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

  if (lhsInfo)
    *lhsInfo = lhsPtr;

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
  case BO_ShrAssign: {
    const auto result =
        theBuilder.createBinaryOp(spvOp, resultType, lhsVal, rhsVal);
    return lhsVal.isRelaxedPrecision || rhsVal.isRelaxedPrecision
               ? SpirvEvalInfo::withRelaxedPrecision(result)
               : result;
  }
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
  const uint32_t initDoneVar =
      theBuilder.addModuleVar(boolType, spv::StorageClass::Private, varName,
                              theBuilder.getConstantBool(false));

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

bool SPIRVEmitter::isTextureMipsSampleIndexing(const CXXOperatorCallExpr *expr,
                                               const Expr **base,
                                               const Expr **location,
                                               const Expr **lod) {
  if (!expr)
    return false;

  // <object>.mips[][] consists of an outer operator[] and an inner operator[]
  const CXXOperatorCallExpr *outerExpr = expr;
  if (outerExpr->getOperator() != OverloadedOperatorKind::OO_Subscript)
    return false;

  const Expr *arg0 = outerExpr->getArg(0)->IgnoreParenNoopCasts(astContext);
  const CXXOperatorCallExpr *innerExpr = dyn_cast<CXXOperatorCallExpr>(arg0);

  // Must have an inner operator[]
  if (!innerExpr ||
      innerExpr->getOperator() != OverloadedOperatorKind::OO_Subscript) {
    return false;
  }

  const Expr *innerArg0 =
      innerExpr->getArg(0)->IgnoreParenNoopCasts(astContext);
  const MemberExpr *memberExpr = dyn_cast<MemberExpr>(innerArg0);
  if (!memberExpr)
    return false;

  // Must be accessing the member named "mips" or "sample"
  const auto &memberName =
      memberExpr->getMemberNameInfo().getName().getAsString();
  if (memberName != "mips" && memberName != "sample")
    return false;

  const Expr *object = memberExpr->getBase();
  const auto objectType = object->getType();
  if (!TypeTranslator::isTexture(objectType))
    return false;

  if (base)
    *base = object;
  if (lod)
    *lod = innerExpr->getArg(1);
  if (location)
    *location = outerExpr->getArg(1);
  return true;
}

bool SPIRVEmitter::isBufferTextureIndexing(const CXXOperatorCallExpr *indexExpr,
                                           const Expr **base,
                                           const Expr **index) {
  if (!indexExpr)
    return false;

  // Must be operator[]
  if (indexExpr->getOperator() != OverloadedOperatorKind::OO_Subscript)
    return false;
  const Expr *object = indexExpr->getArg(0);
  const auto objectType = object->getType();
  if (TypeTranslator::isBuffer(objectType) ||
      TypeTranslator::isRWBuffer(objectType) ||
      TypeTranslator::isTexture(objectType) ||
      TypeTranslator::isRWTexture(objectType)) {
    if (base)
      *base = object;
    if (index)
      *index = indexExpr->getArg(1);
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

SpirvEvalInfo SPIRVEmitter::createVectorSplat(const Expr *scalarExpr,
                                              uint32_t size) {
  bool isConstVal = false;
  uint32_t scalarVal = 0;

  // Try to evaluate the element as constant first. If successful, then we
  // can generate constant instructions for this vector splat.
  if (scalarVal = tryToEvaluateAsConst(scalarExpr)) {
    isConstVal = true;
  } else {
    scalarVal = doExpr(scalarExpr);
  }

  // Just return the scalar value for vector splat with size 1
  if (size == 1)
    return isConstVal ? SpirvEvalInfo::withConst(scalarVal) : scalarVal;

  const uint32_t vecType = theBuilder.getVecType(
      typeTranslator.translateType(scalarExpr->getType()), size);
  llvm::SmallVector<uint32_t, 4> elements(size_t(size), scalarVal);

  if (isConstVal) {
    // TODO: we are saying the constant has Function storage class here.
    // Should find a more meaningful one.
    return SpirvEvalInfo::withConst(
        theBuilder.getConstantComposite(vecType, elements));
  } else {
    return theBuilder.createCompositeConstruct(vecType, elements);
  }
}

void SPIRVEmitter::splitVecLastElement(QualType vecType, uint32_t vec,
                                       uint32_t *residual,
                                       uint32_t *lastElement) {
  assert(hlsl::IsHLSLVecType(vecType));

  const uint32_t count = hlsl::GetHLSLVecSize(vecType);
  assert(count > 1);
  const uint32_t elemTypeId =
      typeTranslator.translateType(hlsl::GetHLSLVecElementType(vecType));

  if (count == 2) {
    *residual = theBuilder.createCompositeExtract(elemTypeId, vec, 0);
  } else {
    llvm::SmallVector<uint32_t, 4> indices;
    for (uint32_t i = 0; i < count - 1; ++i)
      indices.push_back(i);

    const uint32_t typeId = theBuilder.getVecType(elemTypeId, count - 1);
    *residual = theBuilder.createVectorShuffle(typeId, vec, vec, indices);
  }

  *lastElement =
      theBuilder.createCompositeExtract(elemTypeId, vec, {count - 1});
}

SpirvEvalInfo
SPIRVEmitter::tryToGenFloatVectorScale(const BinaryOperator *expr) {
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
          SpirvEvalInfo lhsPtr = 0;
          const auto result =
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

SpirvEvalInfo
SPIRVEmitter::tryToGenFloatMatrixScale(const BinaryOperator *expr) {
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
          SpirvEvalInfo lhsPtr = 0;
          const auto result =
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

SpirvEvalInfo
SPIRVEmitter::tryToAssignToVectorElements(const Expr *lhs,
                                          const SpirvEvalInfo &rhs) {
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

SpirvEvalInfo
SPIRVEmitter::tryToAssignToRWBufferRWTexture(const Expr *lhs,
                                             const SpirvEvalInfo &rhs) {
  const Expr *baseExpr = nullptr;
  const Expr *indexExpr = nullptr;
  const auto lhsExpr = dyn_cast<CXXOperatorCallExpr>(lhs);
  if (isBufferTextureIndexing(lhsExpr, &baseExpr, &indexExpr)) {
    const uint32_t locId = doExpr(indexExpr);
    const uint32_t imageId = theBuilder.createLoad(
        typeTranslator.translateType(baseExpr->getType()), doExpr(baseExpr));
    theBuilder.createImageWrite(imageId, locId, rhs);
    return rhs;
  }
  return 0;
}

SpirvEvalInfo
SPIRVEmitter::tryToAssignToMatrixElements(const Expr *lhs,
                                          const SpirvEvalInfo &rhs) {
  const auto *lhsExpr = dyn_cast<ExtMatrixElementExpr>(lhs);
  if (!lhsExpr)
    return 0;

  const Expr *baseMat = lhsExpr->getBase();
  const auto &base = doExpr(baseMat);
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

    const uint32_t ptrType =
        theBuilder.getPointerType(elemTypeId, base.storageClass);

    // If the lhs is actually a matrix of size 1x1, we don't need the access
    // chain. base is already the dest pointer.
    uint32_t lhsElemPtr = base;
    if (!indices.empty()) {
      // Load the element via access chain
      lhsElemPtr = theBuilder.createAccessChain(ptrType, lhsElemPtr, indices);
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

SpirvEvalInfo
SPIRVEmitter::processMatrixBinaryOp(const Expr *lhs, const Expr *rhs,
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

const Expr *SPIRVEmitter::collectArrayStructIndices(
    const Expr *expr, llvm::SmallVectorImpl<uint32_t> *indices) {
  if (const auto *indexing = dyn_cast<MemberExpr>(expr)) {
    // First check whether this is referring to a static member. If it is, we
    // create a DeclRefExpr for it.
    if (auto *varDecl = dyn_cast<VarDecl>(indexing->getMemberDecl()))
      if (varDecl->isStaticDataMember())
        return DeclRefExpr::Create(
            astContext, NestedNameSpecifierLoc(), SourceLocation(), varDecl,
            /*RefersToEnclosingVariableOrCapture=*/false, SourceLocation(),
            varDecl->getType(), VK_LValue);

    const Expr *base = collectArrayStructIndices(
        indexing->getBase()->IgnoreParenNoopCasts(astContext), indices);

    // Append the index of the current level
    const auto *fieldDecl = cast<FieldDecl>(indexing->getMemberDecl());
    assert(fieldDecl);
    indices->push_back(theBuilder.getConstantInt32(fieldDecl->getFieldIndex()));

    return base;
  }

  if (const auto *indexing = dyn_cast<ArraySubscriptExpr>(expr)) {
    // The base of an ArraySubscriptExpr has a wrapping LValueToRValue implicit
    // cast. We need to ingore it to avoid creating OpLoad.
    const Expr *thisBase = indexing->getBase()->IgnoreParenLValueCasts();
    const Expr *base = collectArrayStructIndices(thisBase, indices);
    indices->push_back(doExpr(indexing->getIdx()));
    return base;
  }

  if (const auto *indexing = dyn_cast<CXXOperatorCallExpr>(expr))
    if (indexing->getOperator() == OverloadedOperatorKind::OO_Subscript) {
      const Expr *thisBase =
          indexing->getArg(0)->IgnoreParenNoopCasts(astContext);
      const auto thisBaseType = thisBase->getType();
      const Expr *base = collectArrayStructIndices(thisBase, indices);

      // If the base is a StructureType, we need to push an addtional index 0
      // here. This is because we created an additional OpTypeRuntimeArray
      // in the structure.
      if (TypeTranslator::isStructuredBuffer(thisBaseType))
        indices->push_back(theBuilder.getConstantInt32(0));

      if ((hlsl::IsHLSLVecType(thisBaseType) &&
           (hlsl::GetHLSLVecSize(thisBaseType) == 1)) ||
          typeTranslator.is1x1Matrix(thisBaseType) ||
          typeTranslator.is1xNMatrix(thisBaseType)) {
        // If this is a size-1 vector or 1xN matrix, ignore the index.
      } else {
        indices->push_back(doExpr(indexing->getArg(1)));
      }
      return base;
    }

  {
    const Expr *index = nullptr;
    // TODO: the following is duplicating the logic in doCXXMemberCallExpr.
    if (const auto *object = isStructuredBufferLoad(expr, &index)) {
      // For object.Load(index), there should be no more indexing into the
      // object.
      indices->push_back(theBuilder.getConstantInt32(0));
      indices->push_back(doExpr(index));
      return object;
    }
  }

  // This the deepest we can go. No more array or struct indexing.
  return expr;
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

  const bool isFloatType = isFloatOrVecMatOfFloatType(callExpr->getType());
  const bool isSintType = isSintOrVecMatOfSintType(callExpr->getType());

  // Figure out which intrinsic function to translate.
  llvm::StringRef group;
  uint32_t opcode = static_cast<uint32_t>(hlsl::IntrinsicOp::Num_Intrinsics);
  hlsl::GetIntrinsicOp(callee, opcode, group);

  GLSLstd450 glslOpcode = GLSLstd450Bad;

#define INTRINSIC_SPIRV_OP_WITH_CAP_CASE(intrinsicOp, spirvOp, doEachVec, cap) \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    theBuilder.requireCapability(cap);                                         \
    return processIntrinsicUsingSpirvInst(callExpr, spv::Op::Op##spirvOp,      \
                                          doEachVec);                          \
  } break

#define INTRINSIC_SPIRV_OP_CASE(intrinsicOp, spirvOp, doEachVec)               \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    return processIntrinsicUsingSpirvInst(callExpr, spv::Op::Op##spirvOp,      \
                                          doEachVec);                          \
  } break

#define INTRINSIC_OP_CASE(intrinsicOp, glslOp, doEachVec)                      \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = GLSLstd450::GLSLstd450##glslOp;                               \
    return processIntrinsicUsingGLSLInst(callExpr, glslOpcode, doEachVec);     \
  } break

#define INTRINSIC_OP_CASE_INT_FLOAT(intrinsicOp, glslIntOp, glslFloatOp,       \
                                    doEachVec)                                 \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = isFloatType ? GLSLstd450::GLSLstd450##glslFloatOp             \
                             : GLSLstd450::GLSLstd450##glslIntOp;              \
    return processIntrinsicUsingGLSLInst(callExpr, glslOpcode, doEachVec);     \
  } break

#define INTRINSIC_OP_CASE_SINT_UINT(intrinsicOp, glslSintOp, glslUintOp,       \
                                    doEachVec)                                 \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = isSintType ? GLSLstd450::GLSLstd450##glslSintOp               \
                            : GLSLstd450::GLSLstd450##glslUintOp;              \
    return processIntrinsicUsingGLSLInst(callExpr, glslOpcode, doEachVec);     \
  } break

#define INTRINSIC_OP_CASE_SINT_UINT_FLOAT(intrinsicOp, glslSintOp, glslUintOp, \
                                          glslFloatOp, doEachVec)              \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = isFloatType                                                   \
                     ? GLSLstd450::GLSLstd450##glslFloatOp                     \
                     : isSintType ? GLSLstd450::GLSLstd450##glslSintOp         \
                                  : GLSLstd450::GLSLstd450##glslUintOp;        \
    return processIntrinsicUsingGLSLInst(callExpr, glslOpcode, doEachVec);     \
  } break

  switch (const auto hlslOpcode = static_cast<hlsl::IntrinsicOp>(opcode)) {
  case hlsl::IntrinsicOp::IOP_InterlockedAdd:
  case hlsl::IntrinsicOp::IOP_InterlockedAnd:
  case hlsl::IntrinsicOp::IOP_InterlockedMax:
  case hlsl::IntrinsicOp::IOP_InterlockedUMax:
  case hlsl::IntrinsicOp::IOP_InterlockedMin:
  case hlsl::IntrinsicOp::IOP_InterlockedUMin:
  case hlsl::IntrinsicOp::IOP_InterlockedOr:
  case hlsl::IntrinsicOp::IOP_InterlockedXor:
  case hlsl::IntrinsicOp::IOP_InterlockedExchange:
  case hlsl::IntrinsicOp::IOP_InterlockedCompareStore:
  case hlsl::IntrinsicOp::IOP_InterlockedCompareExchange:
    return processIntrinsicInterlockedMethod(callExpr, hlslOpcode);
  case hlsl::IntrinsicOp::IOP_dot:
    return processIntrinsicDot(callExpr);
  case hlsl::IntrinsicOp::IOP_mul:
    return processIntrinsicMul(callExpr);
  case hlsl::IntrinsicOp::IOP_all:
    return processIntrinsicAllOrAny(callExpr, spv::Op::OpAll);
  case hlsl::IntrinsicOp::IOP_any:
    return processIntrinsicAllOrAny(callExpr, spv::Op::OpAny);
  case hlsl::IntrinsicOp::IOP_asfloat:
  case hlsl::IntrinsicOp::IOP_asint:
  case hlsl::IntrinsicOp::IOP_asuint:
    return processIntrinsicAsType(callExpr);
  case hlsl::IntrinsicOp::IOP_clip: {
    return processIntrinsicClip(callExpr);
  }
  case hlsl::IntrinsicOp::IOP_clamp:
  case hlsl::IntrinsicOp::IOP_uclamp:
    return processIntrinsicClamp(callExpr);
  case hlsl::IntrinsicOp::IOP_frexp:
    return processIntrinsicFrexp(callExpr);
  case hlsl::IntrinsicOp::IOP_modf:
    return processIntrinsicModf(callExpr);
  case hlsl::IntrinsicOp::IOP_sign: {
    if (isFloatOrVecMatOfFloatType(callExpr->getArg(0)->getType()))
      return processIntrinsicFloatSign(callExpr);
    else
      return processIntrinsicUsingGLSLInst(callExpr,
                                           GLSLstd450::GLSLstd450SSign,
                                           /*actPerRowForMatrices*/ true);
  }
  case hlsl::IntrinsicOp::IOP_isfinite: {
    return processIntrinsicIsFinite(callExpr);
  }
  case hlsl::IntrinsicOp::IOP_sincos: {
    return processIntrinsicSinCos(callExpr);
  }
  case hlsl::IntrinsicOp::IOP_rcp: {
    return processIntrinsicRcp(callExpr);
  }
  case hlsl::IntrinsicOp::IOP_saturate: {
    return processIntrinsicSaturate(callExpr);
  }
  case hlsl::IntrinsicOp::IOP_log10: {
    return processIntrinsicLog10(callExpr);
  }
    INTRINSIC_SPIRV_OP_CASE(transpose, Transpose, false);
    INTRINSIC_SPIRV_OP_CASE(ddx, DPdx, true);
    INTRINSIC_SPIRV_OP_WITH_CAP_CASE(ddx_coarse, DPdxCoarse, false,
                                     spv::Capability::DerivativeControl);
    INTRINSIC_SPIRV_OP_WITH_CAP_CASE(ddx_fine, DPdxFine, false,
                                     spv::Capability::DerivativeControl);
    INTRINSIC_SPIRV_OP_CASE(ddy, DPdy, true);
    INTRINSIC_SPIRV_OP_WITH_CAP_CASE(ddy_coarse, DPdyCoarse, false,
                                     spv::Capability::DerivativeControl);
    INTRINSIC_SPIRV_OP_WITH_CAP_CASE(ddy_fine, DPdyFine, false,
                                     spv::Capability::DerivativeControl);
    INTRINSIC_SPIRV_OP_CASE(countbits, BitCount, false);
    INTRINSIC_SPIRV_OP_CASE(isinf, IsInf, true);
    INTRINSIC_SPIRV_OP_CASE(isnan, IsNan, true);
    INTRINSIC_SPIRV_OP_CASE(fmod, FMod, true);
    INTRINSIC_SPIRV_OP_CASE(fwidth, Fwidth, true);
    INTRINSIC_SPIRV_OP_CASE(reversebits, BitReverse, false);
    INTRINSIC_OP_CASE(round, Round, true);
    INTRINSIC_OP_CASE_INT_FLOAT(abs, SAbs, FAbs, true);
    INTRINSIC_OP_CASE(acos, Acos, true);
    INTRINSIC_OP_CASE(asin, Asin, true);
    INTRINSIC_OP_CASE(atan, Atan, true);
    INTRINSIC_OP_CASE(atan2, Atan2, true);
    INTRINSIC_OP_CASE(ceil, Ceil, true);
    INTRINSIC_OP_CASE(cos, Cos, true);
    INTRINSIC_OP_CASE(cosh, Cosh, true);
    INTRINSIC_OP_CASE(cross, Cross, false);
    INTRINSIC_OP_CASE(degrees, Degrees, true);
    INTRINSIC_OP_CASE(distance, Distance, false);
    INTRINSIC_OP_CASE(determinant, Determinant, false);
    INTRINSIC_OP_CASE(exp, Exp, true);
    INTRINSIC_OP_CASE(exp2, Exp2, true);
    INTRINSIC_OP_CASE_SINT_UINT(firstbithigh, FindSMsb, FindUMsb, false);
    INTRINSIC_OP_CASE_SINT_UINT(ufirstbithigh, FindSMsb, FindUMsb, false);
    INTRINSIC_OP_CASE(faceforward, FaceForward, false);
    INTRINSIC_OP_CASE(firstbitlow, FindILsb, false);
    INTRINSIC_OP_CASE(floor, Floor, true);
    INTRINSIC_OP_CASE(fma, Fma, true);
    INTRINSIC_OP_CASE(frac, Fract, true);
    INTRINSIC_OP_CASE(length, Length, false);
    INTRINSIC_OP_CASE(ldexp, Ldexp, true);
    INTRINSIC_OP_CASE(lerp, FMix, true);
    INTRINSIC_OP_CASE(log, Log, true);
    INTRINSIC_OP_CASE(log2, Log2, true);
    INTRINSIC_OP_CASE(mad, Fma, true);
    INTRINSIC_OP_CASE_SINT_UINT_FLOAT(max, SMax, UMax, FMax, true);
    INTRINSIC_OP_CASE(umax, UMax, true);
    INTRINSIC_OP_CASE_SINT_UINT_FLOAT(min, SMin, UMin, FMin, true);
    INTRINSIC_OP_CASE(umin, UMin, true);
    INTRINSIC_OP_CASE(normalize, Normalize, false);
    INTRINSIC_OP_CASE(pow, Pow, true);
    INTRINSIC_OP_CASE(radians, Radians, true);
    INTRINSIC_OP_CASE(reflect, Reflect, false);
    INTRINSIC_OP_CASE(refract, Refract, false);
    INTRINSIC_OP_CASE(rsqrt, InverseSqrt, true);
    INTRINSIC_OP_CASE(smoothstep, SmoothStep, true);
    INTRINSIC_OP_CASE(step, Step, true);
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

  return 0;
}

uint32_t
SPIRVEmitter::processIntrinsicInterlockedMethod(const CallExpr *expr,
                                                hlsl::IntrinsicOp opcode) {
  // The signature of intrinsic atomic methods are:
  // void Interlocked*(in R dest, in T value);
  // void Interlocked*(in R dest, in T value, out T original_value);

  // Note: ALL Interlocked*() methods are forced to have an unsigned integer
  // 'value'. Meaning, T is forced to be 'unsigned int'. If the provided
  // parameter is not an unsigned integer, the frontend inserts an
  // 'ImplicitCastExpr' to convert it to unsigned integer. OpAtomicIAdd (and
  // other SPIR-V OpAtomic* instructions) require that the pointee in 'dest' to
  // be of the same type as T. This will result in an invalid SPIR-V if 'dest'
  // is a signed integer typed resource such as RWTexture1D<int>. For example,
  // the following OpAtomicIAdd is invalid because the pointee type defined in
  // %1 is a signed integer, while the value passed to atomic add (%3) is an
  // unsigned integer.
  //
  //  %_ptr_Image_int = OpTypePointer Image %int
  //  %1 = OpImageTexelPointer %_ptr_Image_int %RWTexture1D_int %index %uint_0
  //  %2 = OpLoad %int %value
  //  %3 = OpBitcast %uint %2   <-------- Inserted by the frontend
  //  %4 = OpAtomicIAdd %int %1 %uint_1 %uint_0 %3
  //
  // In such cases, we bypass the forced IntegralCast.
  // Moreover, the frontend does not add a cast AST node to cast uint to int
  // where necessary. To ensure SPIR-V validity, we add that where necessary.

  const uint32_t zero = theBuilder.getConstantUint32(0);
  const uint32_t scope = theBuilder.getConstantUint32(1); // Device
  const auto *dest = expr->getArg(0);
  const auto baseType = dest->getType();
  const uint32_t baseTypeId = typeTranslator.translateType(baseType);

  const auto doArg = [baseType, this](const CallExpr *callExpr,
                                      uint32_t argIndex) {
    const Expr *valueExpr = callExpr->getArg(argIndex);
    if (const auto *castExpr = dyn_cast<ImplicitCastExpr>(valueExpr))
      if (castExpr->getCastKind() == CK_IntegralCast &&
          castExpr->getSubExpr()->getType() == baseType)
        valueExpr = castExpr->getSubExpr();

    uint32_t argId = doExpr(valueExpr);
    if (valueExpr->getType() != baseType)
      argId = castToInt(argId, valueExpr->getType(), baseType);
    return argId;
  };

  const auto writeToOutputArg = [&baseType, this](
      uint32_t toWrite, const CallExpr *callExpr, uint32_t outputArgIndex) {
    const auto outputArg = callExpr->getArg(outputArgIndex);
    const auto outputArgType = outputArg->getType();
    if (baseType != outputArgType)
      toWrite = castToInt(toWrite, baseType, outputArgType);
    theBuilder.createStore(doExpr(outputArg), toWrite);
  };

  // If the argument is indexing into a texture/buffer, we need to create an
  // OpImageTexelPointer instruction.
  uint32_t ptr = 0;
  if (const auto *callExpr = dyn_cast<CXXOperatorCallExpr>(dest)) {
    const Expr *base = nullptr;
    const Expr *index = nullptr;
    if (isBufferTextureIndexing(callExpr, &base, &index)) {
      const auto ptrType =
          theBuilder.getPointerType(baseTypeId, spv::StorageClass::Image);
      const auto baseId = doExpr(base);
      const auto coordId = doExpr(index);
      ptr = theBuilder.createImageTexelPointer(ptrType, baseId, coordId, zero);
    }
  } else {
    ptr = doExpr(dest);
  }

  const bool isCompareExchange =
      opcode == hlsl::IntrinsicOp::IOP_InterlockedCompareExchange;
  const bool isCompareStore =
      opcode == hlsl::IntrinsicOp::IOP_InterlockedCompareStore;

  if (isCompareExchange || isCompareStore) {
    const uint32_t comparator = doArg(expr, 1);
    const uint32_t valueId = doArg(expr, 2);
    const uint32_t originalVal = theBuilder.createAtomicCompareExchange(
        baseTypeId, ptr, scope, zero, zero, valueId, comparator);
    if (isCompareExchange)
      writeToOutputArg(originalVal, expr, 3);
  } else {
    const uint32_t valueId = doArg(expr, 1);
    // Since these atomic operations write through the provided pointer, the
    // signed vs. unsigned opcode must be decided based on the pointee type
    // of the first argument. However, the frontend decides the opcode based on
    // the second argument (value). Therefore, the HLSL opcode provided by the
    // frontend may be wrong. Therefore we need the following code to make sure
    // we are using the correct SPIR-V opcode.
    spv::Op atomicOp = translateAtomicHlslOpcodeToSpirvOpcode(opcode);
    if (atomicOp == spv::Op::OpAtomicUMax && baseType->isSignedIntegerType())
      atomicOp = spv::Op::OpAtomicSMax;
    if (atomicOp == spv::Op::OpAtomicSMax && baseType->isUnsignedIntegerType())
      atomicOp = spv::Op::OpAtomicUMax;
    if (atomicOp == spv::Op::OpAtomicUMin && baseType->isSignedIntegerType())
      atomicOp = spv::Op::OpAtomicSMin;
    if (atomicOp == spv::Op::OpAtomicSMin && baseType->isUnsignedIntegerType())
      atomicOp = spv::Op::OpAtomicUMin;
    const uint32_t originalVal = theBuilder.createAtomicOp(
        atomicOp, baseTypeId, ptr, scope, zero, valueId);
    if (expr->getNumArgs() > 2)
      writeToOutputArg(originalVal, expr, 2);
  }

  return 0;
}

uint32_t SPIRVEmitter::processIntrinsicModf(const CallExpr *callExpr) {
  // Signature is: ret modf(x, ip)
  // [in]    x: the input floating-point value.
  // [out]  ip: the integer portion of x.
  // [out] ret: the fractional portion of x.
  // All of the above must be a scalar, vector, or matrix with the same
  // component types. Component types can be float or int.

  // The ModfStruct SPIR-V instruction returns a struct. The first member is the
  // fractional part and the second member is the integer portion.
  // ModfStruct {
  //   <scalar or vector of float> frac;
  //   <scalar or vector of float> ip;
  // }

  // Note if the input number (x) is not a float (i.e. 'x' is an int), it is
  // automatically converted to float before modf is invoked. Sadly, the 'ip'
  // argument is not treated the same way. Therefore, in such cases we'll have
  // to manually convert the float result into int.

  const uint32_t glslInstSetId = theBuilder.getGLSLExtInstSet();
  const Expr *arg = callExpr->getArg(0);
  const Expr *ipArg = callExpr->getArg(1);
  const auto argType = arg->getType();
  const auto ipType = ipArg->getType();
  const auto returnType = callExpr->getType();
  const auto returnTypeId = typeTranslator.translateType(returnType);
  const auto ipTypeId = typeTranslator.translateType(ipType);
  const uint32_t argId = doExpr(arg);
  const uint32_t ipId = doExpr(ipArg);

  // TODO: We currently do not support non-float matrices.
  QualType ipElemType = {};
  if (TypeTranslator::isMxNMatrix(ipType, &ipElemType) &&
      !ipElemType->isFloatingType()) {
    emitError("Non-FP matrices are currently not supported.");
    return 0;
  }

  // For scalar and vector argument types.
  {
    if (TypeTranslator::isScalarType(argType) ||
        TypeTranslator::isVectorType(argType)) {
      const auto argTypeId = typeTranslator.translateType(argType);
      // The struct members *must* have the same type.
      const auto modfStructTypeId = theBuilder.getStructType(
          {argTypeId, argTypeId}, "ModfStructType", {"frac", "ip"});
      const auto modf =
          theBuilder.createExtInst(modfStructTypeId, glslInstSetId,
                                   GLSLstd450::GLSLstd450ModfStruct, {argId});
      auto ip = theBuilder.createCompositeExtract(argTypeId, modf, {1});
      // This will do nothing if the input number (x) and the ip are both of the
      // same type. Otherwise, it will convert the ip into int as necessary.
      ip = castToInt(ip, argType, ipType);
      theBuilder.createStore(ipId, ip);
      return theBuilder.createCompositeExtract(argTypeId, modf, {0});
    }
  }

  // For matrix argument types.
  {
    uint32_t rowCount = 0, colCount = 0;
    QualType elemType = {};
    if (TypeTranslator::isMxNMatrix(argType, &elemType, &rowCount, &colCount)) {
      const auto elemTypeId = typeTranslator.translateType(elemType);
      const auto colTypeId = theBuilder.getVecType(elemTypeId, colCount);
      const auto modfStructTypeId = theBuilder.getStructType(
          {colTypeId, colTypeId}, "ModfStructType", {"frac", "ip"});
      llvm::SmallVector<uint32_t, 4> fracs;
      llvm::SmallVector<uint32_t, 4> ips;
      for (uint32_t i = 0; i < rowCount; ++i) {
        const auto curRow =
            theBuilder.createCompositeExtract(colTypeId, argId, {i});
        const auto modf = theBuilder.createExtInst(
            modfStructTypeId, glslInstSetId, GLSLstd450::GLSLstd450ModfStruct,
            {curRow});
        auto ip = theBuilder.createCompositeExtract(colTypeId, modf, {1});
        ips.push_back(ip);
        fracs.push_back(
            theBuilder.createCompositeExtract(colTypeId, modf, {0}));
      }
      theBuilder.createStore(
          ipId, theBuilder.createCompositeConstruct(returnTypeId, ips));
      return theBuilder.createCompositeConstruct(returnTypeId, fracs);
    }
  }

  emitError("Unknown argument type passed to Modf function.");
  return 0;
}

uint32_t SPIRVEmitter::processIntrinsicFrexp(const CallExpr *callExpr) {
  // Signature is: ret frexp(x, exp)
  // [in]   x: the input floating-point value.
  // [out]  exp: the calculated exponent.
  // [out]  ret: the calculated mantissa.
  // All of the above must be a scalar, vector, or matrix of *float* type.

  // The FrexpStruct SPIR-V instruction returns a struct. The first
  // member is the significand (mantissa) and must be of the same type as the
  // input parameter, and the second member is the exponent and must always be a
  // scalar or vector of 32-bit *integer* type.
  // FrexpStruct {
  //   <scalar or vector of int/float> mantissa;
  //   <scalar or vector of integers>  exponent;
  // }

  const uint32_t glslInstSetId = theBuilder.getGLSLExtInstSet();
  const Expr *arg = callExpr->getArg(0);
  const auto argType = arg->getType();
  const auto intId = theBuilder.getInt32Type();
  const auto returnTypeId = typeTranslator.translateType(callExpr->getType());
  const uint32_t argId = doExpr(arg);
  const uint32_t expId = doExpr(callExpr->getArg(1));

  // For scalar and vector argument types.
  {
    uint32_t elemCount = 1;
    if (TypeTranslator::isScalarType(argType) ||
        TypeTranslator::isVectorType(argType, nullptr, &elemCount)) {
      const auto argTypeId = typeTranslator.translateType(argType);
      const auto expTypeId =
          elemCount == 1 ? intId : theBuilder.getVecType(intId, elemCount);
      const auto frexpStructTypeId = theBuilder.getStructType(
          {argTypeId, expTypeId}, "FrexpStructType", {"mantissa", "exponent"});
      const auto frexp =
          theBuilder.createExtInst(frexpStructTypeId, glslInstSetId,
                                   GLSLstd450::GLSLstd450FrexpStruct, {argId});
      const auto exponentInt =
          theBuilder.createCompositeExtract(expTypeId, frexp, {1});

      // Since the SPIR-V instruction returns an int, and the intrinsic HLSL
      // expects a float, an conversion must take place before writing the
      // results.
      const auto exponentFloat = theBuilder.createUnaryOp(
          spv::Op::OpConvertSToF, returnTypeId, exponentInt);
      theBuilder.createStore(expId, exponentFloat);
      return theBuilder.createCompositeExtract(argTypeId, frexp, {0});
    }
  }

  // For matrix argument types.
  {
    uint32_t rowCount = 0, colCount = 0;
    if (TypeTranslator::isMxNMatrix(argType, nullptr, &rowCount, &colCount)) {
      const auto floatId = theBuilder.getFloat32Type();
      const auto expTypeId = theBuilder.getVecType(intId, colCount);
      const auto colTypeId = theBuilder.getVecType(floatId, colCount);
      const auto frexpStructTypeId = theBuilder.getStructType(
          {colTypeId, expTypeId}, "FrexpStructType", {"mantissa", "exponent"});
      llvm::SmallVector<uint32_t, 4> exponents;
      llvm::SmallVector<uint32_t, 4> mantissas;
      for (uint32_t i = 0; i < rowCount; ++i) {
        const auto curRow =
            theBuilder.createCompositeExtract(colTypeId, argId, {i});
        const auto frexp = theBuilder.createExtInst(
            frexpStructTypeId, glslInstSetId, GLSLstd450::GLSLstd450FrexpStruct,
            {curRow});
        const auto exponentInt =
            theBuilder.createCompositeExtract(expTypeId, frexp, {1});

        // Since the SPIR-V instruction returns an int, and the intrinsic HLSL
        // expects a float, an conversion must take place before writing the
        // results.
        const auto exponentFloat = theBuilder.createUnaryOp(
            spv::Op::OpConvertSToF, colTypeId, exponentInt);
        exponents.push_back(exponentFloat);
        mantissas.push_back(
            theBuilder.createCompositeExtract(colTypeId, frexp, {0}));
      }
      const auto exponentsResultId =
          theBuilder.createCompositeConstruct(returnTypeId, exponents);
      theBuilder.createStore(expId, exponentsResultId);
      return theBuilder.createCompositeConstruct(returnTypeId, mantissas);
    }
  }

  emitError("Unknown argument type passed to Frexp function.");
  return 0;
}

uint32_t SPIRVEmitter::processIntrinsicClip(const CallExpr *callExpr) {
  // Discards the current pixel if the specified value is less than zero.
  // TODO: If the argument can be const folded and evaluated, we could
  // potentially avoid creating a branch. This would be a bit challenging for
  // matrix/vector arguments.

  assert(callExpr->getNumArgs() == 1u);
  const Expr *arg = callExpr->getArg(0);
  const auto argType = arg->getType();
  const auto boolType = theBuilder.getBoolType();
  uint32_t condition = 0;

  // Could not determine the argument as a constant. We need to branch based on
  // the argument. If the argument is a vector/matrix, clipping is done if *any*
  // element of the vector/matrix is less than zero.
  const uint32_t argId = doExpr(arg);

  QualType elemType = {};
  uint32_t elemCount = 0, rowCount = 0, colCount = 0;
  if (TypeTranslator::isScalarType(argType)) {
    const auto zero = getValueZero(argType);
    condition = theBuilder.createBinaryOp(spv::Op::OpFOrdLessThan, boolType,
                                          argId, zero);
  } else if (TypeTranslator::isVectorType(argType, nullptr, &elemCount)) {
    const auto zero = getValueZero(argType);
    const auto boolVecType = theBuilder.getVecType(boolType, elemCount);
    const auto cmp = theBuilder.createBinaryOp(spv::Op::OpFOrdLessThan,
                                               boolVecType, argId, zero);
    condition = theBuilder.createUnaryOp(spv::Op::OpAny, boolType, cmp);
  } else if (TypeTranslator::isMxNMatrix(argType, &elemType, &rowCount,
                                         &colCount)) {
    const uint32_t elemTypeId = typeTranslator.translateType(elemType);
    const uint32_t floatVecType = theBuilder.getVecType(elemTypeId, colCount);
    const uint32_t elemZeroId = getValueZero(elemType);
    llvm::SmallVector<uint32_t, 4> elements(size_t(colCount), elemZeroId);
    const auto zero = theBuilder.getConstantComposite(floatVecType, elements);
    llvm::SmallVector<uint32_t, 4> cmpResults;
    for (uint32_t i = 0; i < rowCount; ++i) {
      const uint32_t lhsVec =
          theBuilder.createCompositeExtract(floatVecType, argId, {i});
      const auto boolColType = theBuilder.getVecType(boolType, colCount);
      const auto cmp = theBuilder.createBinaryOp(spv::Op::OpFOrdLessThan,
                                                 boolColType, lhsVec, zero);
      const auto any = theBuilder.createUnaryOp(spv::Op::OpAny, boolType, cmp);
      cmpResults.push_back(any);
    }
    const auto boolRowType = theBuilder.getVecType(boolType, rowCount);
    const auto results =
        theBuilder.createCompositeConstruct(boolRowType, cmpResults);
    condition = theBuilder.createUnaryOp(spv::Op::OpAny, boolType, results);
  } else {
    emitError("Invalid type passed to clip function.");
    return 0;
  }

  // Then we need to emit the instruction for the conditional branch.
  const uint32_t thenBB = theBuilder.createBasicBlock("if.true");
  const uint32_t mergeBB = theBuilder.createBasicBlock("if.merge");
  // Create the branch instruction. This will end the current basic block.
  theBuilder.createConditionalBranch(condition, thenBB, mergeBB, mergeBB);
  theBuilder.addSuccessor(thenBB);
  theBuilder.addSuccessor(mergeBB);
  theBuilder.setMergeTarget(mergeBB);
  // Handle the then branch
  theBuilder.setInsertPoint(thenBB);
  theBuilder.createKill();
  theBuilder.addSuccessor(mergeBB);
  // From now on, we'll emit instructions into the merge block.
  theBuilder.setInsertPoint(mergeBB);
  return 0;
}

uint32_t SPIRVEmitter::processIntrinsicClamp(const CallExpr *callExpr) {
  // According the HLSL reference: clamp(X, Min, Max) takes 3 arguments. Each
  // one may be int, uint, or float.
  const uint32_t glslInstSetId = theBuilder.getGLSLExtInstSet();
  const QualType returnType = callExpr->getType();
  const uint32_t returnTypeId = typeTranslator.translateType(returnType);
  GLSLstd450 glslOpcode = GLSLstd450::GLSLstd450UClamp;
  if (isFloatOrVecMatOfFloatType(returnType))
    glslOpcode = GLSLstd450::GLSLstd450FClamp;
  else if (isSintOrVecMatOfSintType(returnType))
    glslOpcode = GLSLstd450::GLSLstd450SClamp;

  // Get the function parameters. Expect 3 parameters.
  assert(callExpr->getNumArgs() == 3u);
  const Expr *argX = callExpr->getArg(0);
  const Expr *argMin = callExpr->getArg(1);
  const Expr *argMax = callExpr->getArg(2);
  const uint32_t argXId = doExpr(argX);
  const uint32_t argMinId = doExpr(argMin);
  const uint32_t argMaxId = doExpr(argMax);

  // FClamp, UClamp, and SClamp do not operate on matrices, so we should perform
  // the operation on each vector of the matrix.
  if (TypeTranslator::isSpirvAcceptableMatrixType(argX->getType())) {
    const auto actOnEachVec = [this, glslInstSetId, glslOpcode, argMinId,
                               argMaxId](uint32_t index, uint32_t vecType,
                                         uint32_t curRowId) {
      const auto minRowId =
          theBuilder.createCompositeExtract(vecType, argMinId, {index});
      const auto maxRowId =
          theBuilder.createCompositeExtract(vecType, argMaxId, {index});
      return theBuilder.createExtInst(vecType, glslInstSetId, glslOpcode,
                                      {curRowId, minRowId, maxRowId});
    };
    return processEachVectorInMatrix(argX, argXId, actOnEachVec);
  }

  return theBuilder.createExtInst(returnTypeId, glslInstSetId, glslOpcode,
                                  {argXId, argMinId, argMaxId});
}

uint32_t SPIRVEmitter::processIntrinsicMul(const CallExpr *callExpr) {
  const QualType returnType = callExpr->getType();
  const uint32_t returnTypeId =
      typeTranslator.translateType(callExpr->getType());

  // Get the function parameters. Expect 2 parameters.
  assert(callExpr->getNumArgs() == 2u);
  const Expr *arg0 = callExpr->getArg(0);
  const Expr *arg1 = callExpr->getArg(1);
  const QualType arg0Type = arg0->getType();
  const QualType arg1Type = arg1->getType();

  // The HLSL mul() function takes 2 arguments. Each argument may be a scalar,
  // vector, or matrix. The frontend ensures that the two arguments have the
  // same component type. The only allowed component types are int and float.

  // mul(scalar, vector)
  {
    uint32_t elemCount = 0;
    if (TypeTranslator::isScalarType(arg0Type) &&
        TypeTranslator::isVectorType(arg1Type, nullptr, &elemCount)) {

      const uint32_t arg1Id = doExpr(arg1);

      // We can use OpVectorTimesScalar if arguments are floats.
      if (arg0Type->isFloatingType())
        return theBuilder.createBinaryOp(spv::Op::OpVectorTimesScalar,
                                         returnTypeId, arg1Id, doExpr(arg0));

      // Use OpIMul for integers
      return theBuilder.createBinaryOp(spv::Op::OpIMul, returnTypeId,
                                       createVectorSplat(arg0, elemCount),
                                       arg1Id);
    }
  }

  // mul(vector, scalar)
  {
    uint32_t elemCount = 0;
    if (TypeTranslator::isVectorType(arg0Type, nullptr, &elemCount) &&
        TypeTranslator::isScalarType(arg1Type)) {

      const uint32_t arg0Id = doExpr(arg0);

      // We can use OpVectorTimesScalar if arguments are floats.
      if (arg1Type->isFloatingType())
        return theBuilder.createBinaryOp(spv::Op::OpVectorTimesScalar,
                                         returnTypeId, arg0Id, doExpr(arg1));

      // Use OpIMul for integers
      return theBuilder.createBinaryOp(spv::Op::OpIMul, returnTypeId, arg0Id,
                                       createVectorSplat(arg1, elemCount));
    }
  }

  // mul(vector, vector)
  if (TypeTranslator::isVectorType(arg0Type) &&
      TypeTranslator::isVectorType(arg1Type))
    return processIntrinsicDot(callExpr);

  // All the following cases require handling arg0 and arg1 expressions first.
  const uint32_t arg0Id = doExpr(arg0);
  const uint32_t arg1Id = doExpr(arg1);

  // mul(scalar, scalar)
  if (TypeTranslator::isScalarType(arg0Type) &&
      TypeTranslator::isScalarType(arg1Type))
    return theBuilder.createBinaryOp(translateOp(BO_Mul, arg0Type),
                                     returnTypeId, arg0Id, arg1Id);

  // mul(scalar, matrix)
  if (TypeTranslator::isScalarType(arg0Type) &&
      TypeTranslator::isMxNMatrix(arg1Type)) {
    // We currently only support float matrices. So we can use
    // OpMatrixTimesScalar
    if (arg0Type->isFloatingType())
      return theBuilder.createBinaryOp(spv::Op::OpMatrixTimesScalar,
                                       returnTypeId, arg1Id, arg0Id);
  }

  // mul(matrix, scalar)
  if (TypeTranslator::isScalarType(arg1Type) &&
      TypeTranslator::isMxNMatrix(arg0Type)) {
    // We currently only support float matrices. So we can use
    // OpMatrixTimesScalar
    if (arg1Type->isFloatingType())
      return theBuilder.createBinaryOp(spv::Op::OpMatrixTimesScalar,
                                       returnTypeId, arg0Id, arg1Id);
  }

  // mul(vector, matrix)
  {
    QualType elemType = {};
    uint32_t elemCount = 0, numRows = 0;
    if (TypeTranslator::isVectorType(arg0Type, &elemType, &elemCount) &&
        TypeTranslator::isMxNMatrix(arg1Type, nullptr, &numRows, nullptr) &&
        elemType->isFloatingType()) {
      assert(elemCount == numRows);
      return theBuilder.createBinaryOp(spv::Op::OpMatrixTimesVector,
                                       returnTypeId, arg1Id, arg0Id);
    }
  }

  // mul(matrix, vector)
  {
    QualType elemType = {};
    uint32_t elemCount = 0, numCols = 0;
    if (TypeTranslator::isMxNMatrix(arg0Type, nullptr, nullptr, &numCols) &&
        TypeTranslator::isVectorType(arg1Type, &elemType, &elemCount) &&
        elemType->isFloatingType()) {
      assert(elemCount == numCols);
      return theBuilder.createBinaryOp(spv::Op::OpVectorTimesMatrix,
                                       returnTypeId, arg1Id, arg0Id);
    }
  }

  // mul(matrix, matrix)
  {
    QualType elemType = {};
    uint32_t arg0Cols = 0, arg1Rows = 0;
    if (TypeTranslator::isMxNMatrix(arg0Type, &elemType, nullptr, &arg0Cols) &&
        TypeTranslator::isMxNMatrix(arg1Type, nullptr, &arg1Rows, nullptr) &&
        elemType->isFloatingType()) {
      assert(arg0Cols == arg1Rows);
      return theBuilder.createBinaryOp(spv::Op::OpMatrixTimesMatrix,
                                       returnTypeId, arg1Id, arg0Id);
    }
  }

  emitError("Unsupported arguments passed to mul() function.");
  return 0;
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

uint32_t SPIRVEmitter::processIntrinsicRcp(const CallExpr *callExpr) {
  // 'rcp' takes only 1 argument that is a scalar, vector, or matrix of type
  // float or double.
  assert(callExpr->getNumArgs() == 1u);
  const QualType returnType = callExpr->getType();
  const uint32_t returnTypeId = typeTranslator.translateType(returnType);
  const Expr *arg = callExpr->getArg(0);
  const uint32_t argId = doExpr(arg);
  const QualType argType = arg->getType();

  // For cases with matrix argument.
  QualType elemType = {};
  uint32_t numRows = 0, numCols = 0;
  if (TypeTranslator::isMxNMatrix(argType, &elemType, &numRows, &numCols)) {
    const uint32_t vecOne = getVecValueOne(elemType, numCols);
    const auto actOnEachVec = [this, vecOne](
        uint32_t /*index*/, uint32_t vecType, uint32_t curRowId) {
      return theBuilder.createBinaryOp(spv::Op::OpFDiv, vecType, vecOne,
                                       curRowId);
    };
    return processEachVectorInMatrix(arg, argId, actOnEachVec);
  }

  // For cases with scalar or vector arguments.
  return theBuilder.createBinaryOp(spv::Op::OpFDiv, returnTypeId,
                                   getValueOne(argType), argId);
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

uint32_t SPIRVEmitter::processIntrinsicIsFinite(const CallExpr *callExpr) {
  // Since OpIsFinite needs the Kernel capability, translation is instead done
  // using OpIsNan and OpIsInf:
  // isFinite = !(isNan || isInf)
  const auto arg = doExpr(callExpr->getArg(0));
  const auto returnType = typeTranslator.translateType(callExpr->getType());
  const auto isNan =
      theBuilder.createUnaryOp(spv::Op::OpIsNan, returnType, arg);
  const auto isInf =
      theBuilder.createUnaryOp(spv::Op::OpIsInf, returnType, arg);
  const auto isNanOrInf =
      theBuilder.createBinaryOp(spv::Op::OpLogicalOr, returnType, isNan, isInf);
  return theBuilder.createUnaryOp(spv::Op::OpLogicalNot, returnType,
                                  isNanOrInf);
}

uint32_t SPIRVEmitter::processIntrinsicSinCos(const CallExpr *callExpr) {
  // Since there is no sincos equivalent in SPIR-V, we need to perform Sin
  // once and Cos once. We can reuse existing Sine/Cosine handling functions.
  CallExpr *sincosExpr =
      new (astContext) CallExpr(astContext, Stmt::StmtClass::NoStmtClass, {});
  sincosExpr->setType(callExpr->getArg(0)->getType());
  sincosExpr->setNumArgs(astContext, 1);
  sincosExpr->setArg(0, const_cast<Expr *>(callExpr->getArg(0)));

  // Perform Sin and store results in argument 1.
  const uint32_t sin =
      processIntrinsicUsingGLSLInst(sincosExpr, GLSLstd450::GLSLstd450Sin,
                                    /*actPerRowForMatrices*/ true);
  theBuilder.createStore(doExpr(callExpr->getArg(1)), sin);

  // Perform Cos and store results in argument 2.
  const uint32_t cos =
      processIntrinsicUsingGLSLInst(sincosExpr, GLSLstd450::GLSLstd450Cos,
                                    /*actPerRowForMatrices*/ true);
  theBuilder.createStore(doExpr(callExpr->getArg(2)), cos);
  return 0;
}

uint32_t SPIRVEmitter::processIntrinsicSaturate(const CallExpr *callExpr) {
  const auto *arg = callExpr->getArg(0);
  const auto argId = doExpr(arg);
  const auto argType = arg->getType();
  const uint32_t returnType = typeTranslator.translateType(callExpr->getType());
  const uint32_t glslInstSetId = theBuilder.getGLSLExtInstSet();

  if (argType->isFloatingType()) {
    const uint32_t floatZero = getValueZero(argType);
    const uint32_t floatOne = getValueOne(argType);
    return theBuilder.createExtInst(returnType, glslInstSetId,
                                    GLSLstd450::GLSLstd450FClamp,
                                    {argId, floatZero, floatOne});
  }

  QualType elemType = {};
  uint32_t vecSize = 0;
  if (TypeTranslator::isVectorType(argType, &elemType, &vecSize)) {
    const uint32_t vecZero = getVecValueZero(elemType, vecSize);
    const uint32_t vecOne = getVecValueOne(elemType, vecSize);
    return theBuilder.createExtInst(returnType, glslInstSetId,
                                    GLSLstd450::GLSLstd450FClamp,
                                    {argId, vecZero, vecOne});
  }

  uint32_t numRows = 0, numCols = 0;
  if (TypeTranslator::isMxNMatrix(argType, &elemType, &numRows, &numCols)) {
    const uint32_t vecZero = getVecValueZero(elemType, numCols);
    const uint32_t vecOne = getVecValueOne(elemType, numCols);
    const auto actOnEachVec = [this, vecZero, vecOne, glslInstSetId](
        uint32_t /*index*/, uint32_t vecType, uint32_t curRowId) {
      return theBuilder.createExtInst(vecType, glslInstSetId,
                                      GLSLstd450::GLSLstd450FClamp,
                                      {curRowId, vecZero, vecOne});
    };
    return processEachVectorInMatrix(arg, argId, actOnEachVec);
  }

  emitError("Invalid argument type passed to saturate().");
  return 0;
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

uint32_t SPIRVEmitter::processIntrinsicUsingSpirvInst(
    const CallExpr *callExpr, spv::Op opcode, bool actPerRowForMatrices) {
  const uint32_t returnType = typeTranslator.translateType(callExpr->getType());
  if (callExpr->getNumArgs() == 1u) {
    const Expr *arg = callExpr->getArg(0);
    const uint32_t argId = doExpr(arg);

    // If the instruction does not operate on matrices, we can perform the
    // instruction on each vector of the matrix.
    if (actPerRowForMatrices &&
        TypeTranslator::isSpirvAcceptableMatrixType(arg->getType())) {
      const auto actOnEachVec = [this, opcode](
          uint32_t /*index*/, uint32_t vecType, uint32_t curRowId) {
        return theBuilder.createUnaryOp(opcode, vecType, {curRowId});
      };
      return processEachVectorInMatrix(arg, argId, actOnEachVec);
    }
    return theBuilder.createUnaryOp(opcode, returnType, {argId});
  } else if (callExpr->getNumArgs() == 2u) {
    const Expr *arg0 = callExpr->getArg(0);
    const uint32_t arg0Id = doExpr(arg0);
    const uint32_t arg1Id = doExpr(callExpr->getArg(1));
    // If the instruction does not operate on matrices, we can perform the
    // instruction on each vector of the matrix.
    if (actPerRowForMatrices &&
        TypeTranslator::isSpirvAcceptableMatrixType(arg0->getType())) {
      const auto actOnEachVec = [this, opcode, arg1Id](
          uint32_t index, uint32_t vecType, uint32_t arg0RowId) {
        const uint32_t arg1RowId =
            theBuilder.createCompositeExtract(vecType, arg1Id, {index});
        return theBuilder.createBinaryOp(opcode, vecType, arg0RowId, arg1RowId);
      };
      return processEachVectorInMatrix(arg0, arg0Id, actOnEachVec);
    }
    return theBuilder.createBinaryOp(opcode, returnType, arg0Id, arg1Id);
  }

  emitError("Unsupported intrinsic function %0.")
      << cast<DeclRefExpr>(callExpr->getCallee())->getNameInfo().getAsString();
  return 0;
}

uint32_t SPIRVEmitter::processIntrinsicUsingGLSLInst(
    const CallExpr *callExpr, GLSLstd450 opcode, bool actPerRowForMatrices) {
  // Import the GLSL.std.450 extended instruction set.
  const uint32_t glslInstSetId = theBuilder.getGLSLExtInstSet();
  const uint32_t returnType = typeTranslator.translateType(callExpr->getType());
  if (callExpr->getNumArgs() == 1u) {
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
  } else if (callExpr->getNumArgs() == 2u) {
    const Expr *arg0 = callExpr->getArg(0);
    const uint32_t arg0Id = doExpr(arg0);
    const uint32_t arg1Id = doExpr(callExpr->getArg(1));
    // If the instruction does not operate on matrices, we can perform the
    // instruction on each vector of the matrix.
    if (actPerRowForMatrices &&
        TypeTranslator::isSpirvAcceptableMatrixType(arg0->getType())) {
      const auto actOnEachVec = [this, glslInstSetId, opcode, arg1Id](
          uint32_t index, uint32_t vecType, uint32_t arg0RowId) {
        const uint32_t arg1RowId =
            theBuilder.createCompositeExtract(vecType, arg1Id, {index});
        return theBuilder.createExtInst(vecType, glslInstSetId, opcode,
                                        {arg0RowId, arg1RowId});
      };
      return processEachVectorInMatrix(arg0, arg0Id, actOnEachVec);
    }
    return theBuilder.createExtInst(returnType, glslInstSetId, opcode,
                                    {arg0Id, arg1Id});
  } else if (callExpr->getNumArgs() == 3u) {
    const Expr *arg0 = callExpr->getArg(0);
    const uint32_t arg0Id = doExpr(arg0);
    const uint32_t arg1Id = doExpr(callExpr->getArg(1));
    const uint32_t arg2Id = doExpr(callExpr->getArg(2));
    // If the instruction does not operate on matrices, we can perform the
    // instruction on each vector of the matrix.
    if (actPerRowForMatrices &&
        TypeTranslator::isSpirvAcceptableMatrixType(arg0->getType())) {
      const auto actOnEachVec = [this, glslInstSetId, opcode, arg0Id, arg1Id,
                                 arg2Id](uint32_t index, uint32_t vecType,
                                         uint32_t arg0RowId) {
        const uint32_t arg1RowId =
            theBuilder.createCompositeExtract(vecType, arg1Id, {index});
        const uint32_t arg2RowId =
            theBuilder.createCompositeExtract(vecType, arg2Id, {index});
        return theBuilder.createExtInst(vecType, glslInstSetId, opcode,
                                        {arg0RowId, arg1RowId, arg2RowId});
      };
      return processEachVectorInMatrix(arg0, arg0Id, actOnEachVec);
    }
    return theBuilder.createExtInst(returnType, glslInstSetId, opcode,
                                    {arg0Id, arg1Id, arg2Id});
  }

  emitError("Unsupported intrinsic function %0.")
      << cast<DeclRefExpr>(callExpr->getCallee())->getNameInfo().getAsString();
  return 0;
}

uint32_t SPIRVEmitter::processIntrinsicLog10(const CallExpr *callExpr) {
  // Since there is no log10 instruction in SPIR-V, we can use:
  // log10(x) = log2(x) * ( 1 / log2(10) )
  // 1 / log2(10) = 0.30103
  const auto scale = theBuilder.getConstantFloat32(0.30103f);
  const auto log2 =
      processIntrinsicUsingGLSLInst(callExpr, GLSLstd450::GLSLstd450Log2, true);
  const auto returnType = callExpr->getType();
  const auto returnTypeId = typeTranslator.translateType(returnType);
  spv::Op scaleOp = TypeTranslator::isScalarType(returnType)
                        ? spv::Op::OpFMul
                        : TypeTranslator::isVectorType(returnType)
                              ? spv::Op::OpVectorTimesScalar
                              : spv::Op::OpMatrixTimesScalar;
  return theBuilder.createBinaryOp(scaleOp, returnTypeId, log2, scale);
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
      // TODO: Support other types such as short, half, etc.

      if (scalarType->isSignedIntegerType()) {
        return theBuilder.getConstantInt32(1);
      }

      if (scalarType->isUnsignedIntegerType()) {
        return theBuilder.getConstantUint32(1);
      }

      if (const auto *builtinType = scalarType->getAs<BuiltinType>()) {
        // TODO: Add support for other types that are not covered yet.
        switch (builtinType->getKind()) {
        case BuiltinType::Double:
          return theBuilder.getConstantFloat64(1.0);
        case BuiltinType::Float:
          return theBuilder.getConstantFloat32(1.0);
        }
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
  if (targetType->isSignedIntegerType()) {
    // Try to see if this integer can be represented in 32-bit
    if (intValue.isSignedIntN(32))
      return theBuilder.getConstantInt32(
          static_cast<int32_t>(intValue.getSExtValue()));
  } else {
    // Try to see if this integer can be represented in 32-bit
    if (intValue.isIntN(32))
      return theBuilder.getConstantUint32(
          static_cast<uint32_t>(intValue.getZExtValue()));
  }

  emitError("APInt for target bitwidth '%0' is not supported yet.")
      << astContext.getIntWidth(targetType);

  return 0;
}

uint32_t SPIRVEmitter::translateAPFloat(const llvm::APFloat &floatValue,
                                        QualType targetType) {
  const auto &semantics = astContext.getFloatTypeSemantics(targetType);
  const auto bitwidth = llvm::APFloat::getSizeInBits(semantics);

  switch (bitwidth) {
  case 32:
    return theBuilder.getConstantFloat32(floatValue.convertToFloat());
  case 64:
    return theBuilder.getConstantFloat64(floatValue.convertToDouble());
  default:
    break;
  }

  emitError("APFloat for target bitwidth '%0' is not supported yet.")
      << bitwidth;
  return 0;
}

uint32_t SPIRVEmitter::tryToEvaluateAsConst(const Expr *expr) {
  Expr::EvalResult evalResult;
  if (expr->EvaluateAsRValue(evalResult, astContext) &&
      !evalResult.HasSideEffects) {
    return translateAPValue(evalResult.Val, expr->getType());
  }

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
  } else if (shaderModel.IsGS()) {
    theBuilder.requireCapability(spv::Capability::Geometry);
  } else {
    theBuilder.requireCapability(spv::Capability::Shader);
  }
}

void SPIRVEmitter::AddExecutionModeForEntryPoint(uint32_t entryPointId) {
  if (shaderModel.IsPS()) {
    theBuilder.addExecutionMode(entryPointId,
                                spv::ExecutionMode::OriginUpperLeft, {});
  }
}

bool SPIRVEmitter::processGeometryShaderAttributes(const FunctionDecl *decl) {
  bool success = true;
  assert(shaderModel.IsGS());
  if (auto *vcAttr = decl->getAttr<HLSLMaxVertexCountAttr>()) {
    theBuilder.addExecutionMode(entryFunctionId,
                                spv::ExecutionMode::OutputVertices,
                                {static_cast<uint32_t>(vcAttr->getCount())});
  }

  // Only one primitive type is permitted for the geometry shader.
  bool outPoint = false, outLine = false, outTriangle = false, inPoint = false,
       inLine = false, inTriangle = false, inLineAdj = false,
       inTriangleAdj = false;
  for (const auto *param : decl->params()) {
    // Add an execution mode based on the output stream type. Do not an
    // execution mode more than once.
    if (param->hasAttr<HLSLInOutAttr>()) {
      const auto paramType = param->getType();
      if (hlsl::IsHLSLTriangleStreamType(paramType) && !outTriangle) {
        theBuilder.addExecutionMode(
            entryFunctionId, spv::ExecutionMode::OutputTriangleStrip, {});
        outTriangle = true;
      } else if (hlsl::IsHLSLLineStreamType(paramType) && !outLine) {
        theBuilder.addExecutionMode(entryFunctionId,
                                    spv::ExecutionMode::OutputLineStrip, {});
        outLine = true;
      } else if (hlsl::IsHLSLPointStreamType(paramType) && !outPoint) {
        theBuilder.addExecutionMode(entryFunctionId,
                                    spv::ExecutionMode::OutputPoints, {});
        outPoint = true;
      }
      // An output stream parameter will not have the input primitive type
      // attributes, so we can continue to the next parameter.
      continue;
    }

    // Add an execution mode based on the input primitive type. Do not add an
    // execution mode more than once.
    if (param->hasAttr<HLSLTriangleAttr>() && !inTriangle) {
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::Triangles, {});
      inTriangle = true;
    } else if (param->hasAttr<HLSLTriangleAdjAttr>() && !inTriangleAdj) {
      theBuilder.addExecutionMode(
          entryFunctionId, spv::ExecutionMode::InputTrianglesAdjacency, {});
      inTriangleAdj = true;
    } else if (param->hasAttr<HLSLPointAttr>() && !inPoint) {
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::InputPoints, {});
      inPoint = true;
    } else if (param->hasAttr<HLSLLineAdjAttr>() && !inLineAdj) {
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::InputLinesAdjacency, {});
      inLineAdj = true;
    } else if (param->hasAttr<HLSLLineAttr>() && !inLine) {
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::InputLines, {});
      inLine = true;
    }
  }
  if (inPoint + inLine + inLineAdj + inTriangle + inTriangleAdj > 1) {
    emitError("only one input primitive type can be specified in the geometry "
              "shader");
    success = false;
  }
  if (outPoint + outTriangle + outLine > 1) {
    emitError("only one output primitive type can be specified in the geometry "
              "shader");
    success = false;
  }

  return success;
}

bool SPIRVEmitter::processTessellationShaderAttributes(
    const FunctionDecl *decl, uint32_t *numOutputControlPoints) {
  assert(shaderModel.IsHS() || shaderModel.IsDS());
  using namespace spv;

  if (auto *domain = decl->getAttr<HLSLDomainAttr>()) {
    const auto domainType = domain->getDomainType().lower();
    const ExecutionMode hsExecMode =
        llvm::StringSwitch<ExecutionMode>(domainType)
            .Case("tri", ExecutionMode::Triangles)
            .Case("quad", ExecutionMode::Quads)
            .Case("isoline", ExecutionMode::Isolines)
            .Default(ExecutionMode::Max);
    if (hsExecMode == ExecutionMode::Max) {
      emitError("unknown domain type specified for entry function",
                decl->getLocation());
      return false;
    }
    theBuilder.addExecutionMode(entryFunctionId, hsExecMode, {});
  }

  // Early return for domain shaders as domain shaders only takes the 'domain'
  // attribute.
  if (shaderModel.IsDS())
    return true;

  if (auto *partitioning = decl->getAttr<HLSLPartitioningAttr>()) {
    // TODO: Could not find an equivalent of "pow2" partitioning scheme in
    // SPIR-V.
    const auto scheme = partitioning->getScheme().lower();
    const ExecutionMode hsExecMode =
        llvm::StringSwitch<ExecutionMode>(scheme)
            .Case("fractional_even", ExecutionMode::SpacingFractionalEven)
            .Case("fractional_odd", ExecutionMode::SpacingFractionalOdd)
            .Case("integer", ExecutionMode::SpacingEqual)
            .Default(ExecutionMode::Max);
    if (hsExecMode == ExecutionMode::Max) {
      emitError("unknown partitioning scheme in hull shader",
                decl->getLocation());
      return false;
    }
    theBuilder.addExecutionMode(entryFunctionId, hsExecMode, {});
  }
  if (auto *outputTopology = decl->getAttr<HLSLOutputTopologyAttr>()) {
    const auto topology = outputTopology->getTopology().lower();
    const ExecutionMode hsExecMode =
        llvm::StringSwitch<ExecutionMode>(topology)
            .Case("point", ExecutionMode::PointMode)
            .Case("triangle_cw", ExecutionMode::VertexOrderCw)
            .Case("triangle_ccw", ExecutionMode::VertexOrderCcw)
            .Default(ExecutionMode::Max);
    // TODO: There is no SPIR-V equivalent for "line" topology. Is it the
    // default?
    if (topology != "line") {
      if (hsExecMode != spv::ExecutionMode::Max) {
        theBuilder.addExecutionMode(entryFunctionId, hsExecMode, {});
      } else {
        emitError("unknown output topology in hull shader",
                  decl->getLocation());
        return false;
      }
    }
  }
  if (auto *controlPoints = decl->getAttr<HLSLOutputControlPointsAttr>()) {
    *numOutputControlPoints = controlPoints->getCount();
    theBuilder.addExecutionMode(entryFunctionId,
                                spv::ExecutionMode::OutputVertices,
                                {*numOutputControlPoints});
  }

  return true;
}

bool SPIRVEmitter::emitEntryFunctionWrapper(const FunctionDecl *decl,
                                            const uint32_t entryFuncId) {
  // These are going to be used for Hull shaders only.
  uint32_t numOutputControlPoints = 0;
  uint32_t outputControlPointIdVal = 0;
  uint32_t primitiveIdVar = 0;
  uint32_t hullMainInputPatchParam = 0;

  // Construct the wrapper function signature.
  const uint32_t voidType = theBuilder.getVoidType();
  const uint32_t funcType = theBuilder.getFunctionType(voidType, {});

  // The wrapper entry function surely does not have pre-assigned <result-id>
  // for it like other functions that got added to the work queue following
  // function calls. And the wrapper is the entry function.
  entryFunctionId =
      theBuilder.beginFunction(funcType, voidType, decl->getName());
  declIdMapper.setEntryFunctionId(entryFunctionId);

  // Handle translation of numthreads attribute for compute shaders.
  if (shaderModel.IsCS()) {
    // Number of threads attributes are stored as integers. We cast them to
    // uint32_t to pass to OpExecutionMode SPIR-V instruction.
    if (auto *numThreadsAttr = decl->getAttr<HLSLNumThreadsAttr>()) {
      theBuilder.addExecutionMode(
          entryFunctionId, spv::ExecutionMode::LocalSize,
          {static_cast<uint32_t>(numThreadsAttr->getX()),
           static_cast<uint32_t>(numThreadsAttr->getY()),
           static_cast<uint32_t>(numThreadsAttr->getZ())});
    } else {
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::LocalSize, {1, 1, 1});
    }
  } else if (shaderModel.IsHS() || shaderModel.IsDS()) {
    if (!processTessellationShaderAttributes(decl, &numOutputControlPoints))
      return false;
  } else if (shaderModel.IsGS()) {
    if (!processGeometryShaderAttributes(decl))
      return false;
  }

  // The entry basic block.
  const uint32_t entryLabel = theBuilder.createBasicBlock();
  theBuilder.setInsertPoint(entryLabel);

  // Initialize all global variables at the beginning of the wrapper
  for (const VarDecl *varDecl : toInitGloalVars)
    theBuilder.createStore(declIdMapper.getDeclResultId(varDecl),
                           doExpr(varDecl->getInit()));

  // Create temporary variables for holding function call arguments
  llvm::SmallVector<uint32_t, 4> params;
  for (const auto *param : decl->params()) {
    const uint32_t typeId = typeTranslator.translateType(param->getType());
    std::string tempVarName = "param.var." + param->getNameAsString();
    const uint32_t tempVar = theBuilder.addFnVar(typeId, tempVarName);

    params.push_back(tempVar);

    // Create the stage input variable for parameter not marked as pure out and
    // initialize the corresponding temporary variable
    // Also do not create input variables for output stream objects of geometry
    // shaders (e.g. TriangleStream) which are required to be marked as 'inout'.
    bool isGSOutputStream = shaderModel.IsGS() &&
                            param->hasAttr<HLSLInOutAttr>() &&
                            hlsl::IsHLSLStreamOutputType(param->getType());
    if (!param->hasAttr<HLSLOutAttr>() && !isGSOutputStream) {
      uint32_t loadedValue = 0;
      if (shaderModel.IsHS() &&
          TypeTranslator::isInputPatch(param->getType())) {
        const uint32_t hullInputPatchId =
            declIdMapper.createStageVarWithoutSemantics(
                /*isInput*/ true, typeId, "hullEntryPointInput",
                decl->getAttr<VKLocationAttr>());
        loadedValue = theBuilder.createLoad(typeId, hullInputPatchId);
        hullMainInputPatchParam = tempVar;
      } else if (shaderModel.IsDS() &&
                 TypeTranslator::isOutputPatch(param->getType())) {
        // OutputPatch is the output of the hull shader and an input to the
        // domain shader.
        const uint32_t hullOutputPatchId =
            declIdMapper.createStageVarWithoutSemantics(
                /*isInput*/ true, typeId, "hullShaderOutput",
                decl->getAttr<VKLocationAttr>());
        loadedValue = theBuilder.createLoad(typeId, hullOutputPatchId);
      } else if (!declIdMapper.createStageInputVar(param, &loadedValue,
                                                   /*isPC*/ false)) {
        return false;
      }

      // SV_DomainLocation refers to a float2 (u,v), whereas TessCoord is a
      // float3 (u,v,w). To ensure SPIR-V validity, a float3 stage variable is
      // created, and we must extract a float2 from it before passing it to the
      // main function.
      if (hasSemantic(param, hlsl::DXIL::SemanticKind::DomainLocation) &&
          hlsl::GetHLSLVecSize(param->getType()) == 2) {
        loadedValue = theBuilder.createVectorShuffle(typeId, loadedValue,
                                                     loadedValue, {0, 1});
      }

      theBuilder.createStore(tempVar, loadedValue);

      if (hasSemantic(param, hlsl::DXIL::SemanticKind::OutputControlPointID))
        outputControlPointIdVal = loadedValue;
      if (hasSemantic(param, hlsl::DXIL::SemanticKind::PrimitiveID))
        primitiveIdVar = tempVar;
    }
  }

  // Call the original entry function
  const uint32_t retType = typeTranslator.translateType(decl->getReturnType());
  const uint32_t retVal =
      theBuilder.createFunctionCall(retType, entryFuncId, params);

  // Create and write stage output variables for return value. Special case for
  // Hull shaders since they operate differently in 2 ways:
  // 1- Their return value is in fact an array and each invocation should write
  // to the proper offset in the array.
  // 2- The patch constant function must be called *once* after all invocations
  // of the main entry point function is done.
  if (shaderModel.IsHS()) {
    if (!processHullEntryPointOutputAndPatchConstFunc(
            decl, retType, retVal, numOutputControlPoints,
            outputControlPointIdVal, primitiveIdVar, hullMainInputPatchParam))
      return false;
  } else {
    if (!declIdMapper.createStageOutputVar(decl, retVal, /*isPC*/ false))
      return false;
  }

  // Create and write stage output variables for parameters marked as
  // out/inout
  for (uint32_t i = 0; i < decl->getNumParams(); ++i) {
    const auto *param = decl->getParamDecl(i);
    if (param->getAttr<HLSLOutAttr>() || param->getAttr<HLSLInOutAttr>()) {
      // Load the value from the parameter after function call
      const uint32_t typeId = typeTranslator.translateType(param->getType());
      const uint32_t loadedParam = theBuilder.createLoad(typeId, params[i]);

      if (!declIdMapper.createStageOutputVar(param, loadedParam,
                                             /*isPC*/ false))
        return false;
    }
  }

  theBuilder.createReturn();
  theBuilder.endFunction();

  // For Hull shaders, there is no explicit call to the PCF in the HLSL source.
  // We should invoke a translation of the PCF manually.
  if (shaderModel.IsHS())
    doDecl(patchConstFunc);

  return true;
}

bool SPIRVEmitter::processHullEntryPointOutputAndPatchConstFunc(
    const FunctionDecl *hullMainFuncDecl, uint32_t retType, uint32_t retVal,
    uint32_t numOutputControlPoints, uint32_t outputControlPointId,
    uint32_t primitiveId, uint32_t hullMainInputPatch) {

  // This method may only be called for Hull shaders.
  assert(shaderModel.IsHS());
  uint32_t hullMainOutputPatch = 0;

  // For Hull shaders, the real output is an array of size
  // numOutputControlPoints. The results of the main should be written to the
  // correct offset in the array (based on InvocationID).
  if (!numOutputControlPoints) {
    emitError("number of output control points cannot be zero",
              hullMainFuncDecl->getLocation());
    return false;
  }
  // TODO: We should be able to handle cases where the SV_OutputControlPointID
  // is not provided.
  if (!outputControlPointId) {
    emitError(
        "SV_OutputControlPointID semantic must be provided in the hull shader",
        hullMainFuncDecl->getLocation());
    return false;
  }
  if (!patchConstFunc) {
    emitError("patch constant function not defined in hull shader",
              hullMainFuncDecl->getLocation());
    return false;
  }

  // Let's call the return value of the Hull entry point function
  // "hllEntryPointOutput". The type of hullEntryPointOutput should be an
  // array of size numOutputControlPoints.
  const uint32_t hullEntryPointOutputType = theBuilder.getArrayType(
      retType, theBuilder.getConstantUint32(numOutputControlPoints));
  const auto loc = hullMainFuncDecl->getAttr<VKLocationAttr>();
  const auto hullOutputVar = declIdMapper.createStageVarWithoutSemantics(
      /*isInput*/ false, hullEntryPointOutputType, "hullEntryPointOutput", loc);
  if (!hullOutputVar)
    return false;

  // Write the results into the correct Output array offset.
  const auto location = theBuilder.createAccessChain(
      theBuilder.getPointerType(retType, spv::StorageClass::Output),
      hullOutputVar, {outputControlPointId});
  theBuilder.createStore(location, retVal);

  // If the patch constant function (PCF) takes the result of the Hull main
  // entry point, create a temporary function-scope variable and write the
  // results to it, so it can be passed to the PCF.
  if (patchConstFuncTakesHullOutputPatch(patchConstFunc)) {
    hullMainOutputPatch = theBuilder.addFnVar(hullEntryPointOutputType,
                                              "temp.var.hullEntryPointOutput");
    const auto tempLocation = theBuilder.createAccessChain(
        theBuilder.getPointerType(retType, spv::StorageClass::Function),
        hullMainOutputPatch, {outputControlPointId});
    theBuilder.createStore(tempLocation, retVal);
  }

  // Now create a barrier before calling the Patch Constant Function (PCF).
  // Flags are:
  // Execution Barrier scope = Workgroup (2)
  // Memory Barrier scope = Device (1)
  // Memory Semantics Barrier scope = None (0)
  theBuilder.createControlBarrier(theBuilder.getConstantUint32(2),
                                  theBuilder.getConstantUint32(1),
                                  theBuilder.getConstantUint32(0));

  // The PCF should be called only once. Therefore, we check the invocationID,
  // and we only allow ID 0 to call the PCF.
  const uint32_t condition = theBuilder.createBinaryOp(
      spv::Op::OpIEqual, theBuilder.getBoolType(), outputControlPointId,
      theBuilder.getConstantUint32(0));
  const uint32_t thenBB = theBuilder.createBasicBlock("if.true");
  const uint32_t mergeBB = theBuilder.createBasicBlock("if.merge");
  theBuilder.createConditionalBranch(condition, thenBB, mergeBB, mergeBB);
  theBuilder.addSuccessor(thenBB);
  theBuilder.addSuccessor(mergeBB);
  theBuilder.setMergeTarget(mergeBB);

  theBuilder.setInsertPoint(thenBB);
  // Call the PCF. Since the function is not explicitly called, we must first
  // register an ID for it.
  const uint32_t pcfId = declIdMapper.getOrRegisterFnResultId(patchConstFunc);
  const uint32_t pcfRetType =
      typeTranslator.translateType(patchConstFunc->getReturnType());
  std::vector<uint32_t> pcfParams;
  for (const auto *param : patchConstFunc->parameters()) {
    // Note: According to the HLSL reference, the PCF takes an InputPatch of
    // ControlPoints as well as the PatchID (PrimitiveID). This does not
    // necessarily mean that they are present. There is also no requirement
    // for the order of parameters passed to PCF.
    if (TypeTranslator::isInputPatch(param->getType()))
      pcfParams.push_back(hullMainInputPatch);
    if (TypeTranslator::isOutputPatch(param->getType()))
      pcfParams.push_back(hullMainOutputPatch);
    if (hasSemantic(param, hlsl::DXIL::SemanticKind::PrimitiveID)) {
      if (!primitiveId) {
        const uint32_t typeId = typeTranslator.translateType(param->getType());
        std::string tempVarName = "param.var." + param->getNameAsString();
        const uint32_t tempVar = theBuilder.addFnVar(typeId, tempVarName);
        uint32_t loadedValue = 0;
        declIdMapper.createStageInputVar(param, &loadedValue, /*isPC*/ true);
        theBuilder.createStore(tempVar, loadedValue);
        primitiveId = tempVar;
      }
      pcfParams.push_back(primitiveId);
    }
  }
  const uint32_t pcfResultId =
      theBuilder.createFunctionCall(pcfRetType, pcfId, {pcfParams});
  if (!declIdMapper.createStageOutputVar(patchConstFunc, pcfResultId,
                                         /*isPC*/ true))
    return false;

  theBuilder.createBranch(mergeBB);
  theBuilder.addSuccessor(mergeBB);
  theBuilder.setInsertPoint(mergeBB);
  return true;
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
