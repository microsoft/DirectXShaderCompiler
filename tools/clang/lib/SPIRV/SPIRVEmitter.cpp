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
    if (hlsl::IsHLSLOutputPatchType(param->getType()))
      return true;
  return false;
}

// TODO: Maybe we should move these type probing functions to TypeTranslator.

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

inline bool isSpirvMatrixOp(spv::Op opcode) {
  return opcode == spv::Op::OpMatrixTimesMatrix ||
         opcode == spv::Op::OpMatrixTimesVector ||
         opcode == spv::Op::OpMatrixTimesScalar;
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

/// Returns true if the given VarDecl will be translated into a SPIR-V variable
/// not in the Private or Function storage class.
inline bool isExternalVar(const VarDecl *var) {
  // Class static variables should be put in the Private storage class.
  // groupshared variables are allowed to be declared as "static". But we still
  // need to put them in the Workgroup storage class. That is, when seeing
  // "static groupshared", ignore "static".
  return var->hasExternalFormalLinkage()
             ? !var->isStaticDataMember()
             : (var->getAttr<HLSLGroupSharedAttr>() != nullptr);
}

/// Returns the referenced variable's DeclContext if the given expr is
/// a DeclRefExpr referencing a ConstantBuffer/TextureBuffer. Otherwise,
/// returns nullptr.
const DeclContext *isConstantTextureBufferDeclRef(const Expr *expr) {
  if (const auto *declRefExpr = dyn_cast<DeclRefExpr>(expr->IgnoreParenCasts()))
    if (const auto *varDecl = dyn_cast<VarDecl>(declRefExpr->getFoundDecl()))
      if (TypeTranslator::isConstantTextureBuffer(varDecl))
        return varDecl->getType()->getAs<RecordType>()->getDecl();

  return nullptr;
}

/// Returns true if
/// * the given expr is an DeclRefExpr referencing a kind of structured or byte
///   buffer and it is non-alias one, or
/// * the given expr is an CallExpr returning a kind of structured or byte
///   buffer.
/// * the given expr is an ArraySubscriptExpr referencing a kind of structured
///   or byte buffer.
///
/// Note: legalization specific code
bool isReferencingNonAliasStructuredOrByteBuffer(const Expr *expr) {
  expr = expr->IgnoreParenCasts();
  if (const auto *declRefExpr = dyn_cast<DeclRefExpr>(expr)) {
    if (const auto *varDecl = dyn_cast<VarDecl>(declRefExpr->getFoundDecl()))
      if (TypeTranslator::isAKindOfStructuredOrByteBuffer(varDecl->getType()))
        return isExternalVar(varDecl);
  } else if (const auto *callExpr = dyn_cast<CallExpr>(expr)) {
    if (TypeTranslator::isAKindOfStructuredOrByteBuffer(callExpr->getType()))
      return true;
  } else if (const auto *arrSubExpr = dyn_cast<ArraySubscriptExpr>(expr)) {
    return isReferencingNonAliasStructuredOrByteBuffer(arrSubExpr->getBase());
  }
  return false;
}

bool spirvToolsLegalize(spv_target_env env, std::vector<uint32_t> *module,
                        std::string *messages) {
  spvtools::Optimizer optimizer(env);

  optimizer.SetMessageConsumer(
      [messages](spv_message_level_t /*level*/, const char * /*source*/,
                 const spv_position_t & /*position*/,
                 const char *message) { *messages += message; });

  optimizer.RegisterLegalizationPasses();

  optimizer.RegisterPass(spvtools::CreateReplaceInvalidOpcodePass());

  optimizer.RegisterPass(spvtools::CreateCompactIdsPass());

  return optimizer.Run(module->data(), module->size(), module);
}

bool spirvToolsOptimize(spv_target_env env, std::vector<uint32_t> *module,
                        std::string *messages) {
  spvtools::Optimizer optimizer(env);

  optimizer.SetMessageConsumer(
      [messages](spv_message_level_t /*level*/, const char * /*source*/,
                 const spv_position_t & /*position*/,
                 const char *message) { *messages += message; });

  optimizer.RegisterPerformancePasses();

  optimizer.RegisterPass(spvtools::CreateCompactIdsPass());

  return optimizer.Run(module->data(), module->size(), module);
}

bool spirvToolsValidate(spv_target_env env, std::vector<uint32_t> *module,
                        std::string *messages, bool relaxLogicalPointer) {
  spvtools::SpirvTools tools(env);

  tools.SetMessageConsumer(
      [messages](spv_message_level_t /*level*/, const char * /*source*/,
                 const spv_position_t & /*position*/,
                 const char *message) { *messages += message; });

  spvtools::ValidatorOptions options;
  options.SetRelaxLogicalPointer(relaxLogicalPointer);

  return tools.Validate(module->data(), module->size(), options);
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
  default:
    // Only atomic opcodes are relevant.
    break;
  }

  assert(false && "unimplemented hlsl intrinsic opcode");
  return Op::Max;
}

// Returns true if the given opcode is an accepted binary opcode in
// OpSpecConstantOp.
bool isAcceptedSpecConstantBinaryOp(spv::Op op) {
  switch (op) {
  case spv::Op::OpIAdd:
  case spv::Op::OpISub:
  case spv::Op::OpIMul:
  case spv::Op::OpUDiv:
  case spv::Op::OpSDiv:
  case spv::Op::OpUMod:
  case spv::Op::OpSRem:
  case spv::Op::OpSMod:
  case spv::Op::OpShiftRightLogical:
  case spv::Op::OpShiftRightArithmetic:
  case spv::Op::OpShiftLeftLogical:
  case spv::Op::OpBitwiseOr:
  case spv::Op::OpBitwiseXor:
  case spv::Op::OpBitwiseAnd:
  case spv::Op::OpVectorShuffle:
  case spv::Op::OpCompositeExtract:
  case spv::Op::OpCompositeInsert:
  case spv::Op::OpLogicalOr:
  case spv::Op::OpLogicalAnd:
  case spv::Op::OpLogicalNot:
  case spv::Op::OpLogicalEqual:
  case spv::Op::OpLogicalNotEqual:
  case spv::Op::OpIEqual:
  case spv::Op::OpINotEqual:
  case spv::Op::OpULessThan:
  case spv::Op::OpSLessThan:
  case spv::Op::OpUGreaterThan:
  case spv::Op::OpSGreaterThan:
  case spv::Op::OpULessThanEqual:
  case spv::Op::OpSLessThanEqual:
  case spv::Op::OpUGreaterThanEqual:
  case spv::Op::OpSGreaterThanEqual:
    return true;
  default:
    // Accepted binary opcodes return true. Anything else is false.
    return false;
  }
  return false;
}

/// Returns true if the given expression is an accepted initializer for a spec
/// constant.
bool isAcceptedSpecConstantInit(const Expr *init) {
  // Allow numeric casts
  init = init->IgnoreParenCasts();

  if (isa<CXXBoolLiteralExpr>(init) || isa<IntegerLiteral>(init) ||
      isa<FloatingLiteral>(init))
    return true;

  // Allow the minus operator which is used to specify negative values
  if (const auto *unaryOp = dyn_cast<UnaryOperator>(init))
    return unaryOp->getOpcode() == UO_Minus &&
           isAcceptedSpecConstantInit(unaryOp->getSubExpr());

  return false;
}

/// Returns true if the given function parameter can act as shader stage
/// input parameter.
inline bool canActAsInParmVar(const ParmVarDecl *param) {
  // If the parameter has no in/out/inout attribute, it is defaulted to
  // an in parameter.
  return !param->hasAttr<HLSLOutAttr>() &&
         // GS output streams are marked as inout, but it should not be
         // used as in parameter.
         !hlsl::IsHLSLStreamOutputType(param->getType());
}

/// Returns true if the given function parameter can act as shader stage
/// output parameter.
inline bool canActAsOutParmVar(const ParmVarDecl *param) {
  return param->hasAttr<HLSLOutAttr>() || param->hasAttr<HLSLInOutAttr>();
}

/// Returns true if the given expression is of builtin type and can be evaluated
/// to a constant zero. Returns false otherwise.
inline bool evaluatesToConstZero(const Expr *expr, ASTContext &astContext) {
  const auto type = expr->getType();
  if (!type->isBuiltinType())
    return false;

  Expr::EvalResult evalResult;
  if (expr->EvaluateAsRValue(evalResult, astContext) &&
      !evalResult.HasSideEffects) {
    const auto &val = evalResult.Val;
    return ((type->isBooleanType() && !val.getInt().getBoolValue()) ||
            (type->isIntegerType() && !val.getInt().getBoolValue()) ||
            (type->isFloatingType() && val.getFloat().isZero()));
  }
  return false;
}

/// Returns the HLSLBufferDecl if the given VarDecl is inside a cbuffer/tbuffer.
/// Returns nullptr otherwise, including varDecl is a ConstantBuffer or
/// TextureBuffer itself.
inline const HLSLBufferDecl *getCTBufferContext(const VarDecl *varDecl) {
  if (const auto *bufferDecl =
          dyn_cast<HLSLBufferDecl>(varDecl->getDeclContext()))
    // Filter ConstantBuffer/TextureBuffer
    if (!bufferDecl->isConstantBufferView())
      return bufferDecl;
  return nullptr;
}

/// Returns the real definition of the callee of the given CallExpr.
///
/// If we are calling a forward-declared function, callee will be the
/// FunctionDecl for the foward-declared function, not the actual
/// definition. The foward-delcaration and defintion are two completely
/// different AST nodes.
inline const FunctionDecl *getCalleeDefinition(const CallExpr *expr) {
  const auto *callee = expr->getDirectCallee();

  if (callee->isThisDeclarationADefinition())
    return callee;

  // We need to update callee to the actual definition here
  if (!callee->isDefined(callee))
    return nullptr;

  return callee;
}

/// Returns the referenced definition. The given expr is expected to be a
/// DeclRefExpr or CallExpr after ignoring casts. Returns nullptr otherwise.
const DeclaratorDecl *getReferencedDef(const Expr *expr) {
  if (!expr)
    return nullptr;

  expr = expr->IgnoreParenCasts();

  if (const auto *declRefExpr = dyn_cast<DeclRefExpr>(expr)) {
    return dyn_cast_or_null<DeclaratorDecl>(declRefExpr->getDecl());
  }

  if (const auto *callExpr = dyn_cast<CallExpr>(expr)) {
    return getCalleeDefinition(callExpr);
  }

  return nullptr;
}

/// Returns the number of base classes if this type is a derived class/struct.
/// Returns zero otherwise.
inline uint32_t getNumBaseClasses(QualType type) {
  if (const auto *cxxDecl = type->getAsCXXRecordDecl())
    return cxxDecl->getNumBases();
  return 0;
}

/// Gets the index sequence of casting a derived object to a base object by
/// following the cast chain.
void getBaseClassIndices(const CastExpr *expr,
                         llvm::SmallVectorImpl<uint32_t> *indices) {
  assert(expr->getCastKind() == CK_UncheckedDerivedToBase ||
         expr->getCastKind() == CK_HLSLDerivedToBase);

  indices->clear();

  QualType derivedType = expr->getSubExpr()->getType();
  const auto *derivedDecl = derivedType->getAsCXXRecordDecl();

  // Go through the base cast chain: for each of the derived to base cast, find
  // the index of the base in question in the derived's bases.
  for (auto pathIt = expr->path_begin(), pathIe = expr->path_end();
       pathIt != pathIe; ++pathIt) {
    // The type of the base in question
    const auto baseType = (*pathIt)->getType();

    uint32_t index = 0;
    for (auto baseIt = derivedDecl->bases_begin(),
              baseIe = derivedDecl->bases_end();
         baseIt != baseIe; ++baseIt, ++index)
      if (baseIt->getType() == baseType) {
        indices->push_back(index);
        break;
      }

    assert(index < derivedDecl->getNumBases());

    // Continue to proceed the next base in the chain
    derivedType = baseType;
    derivedDecl = derivedType->getAsCXXRecordDecl();
  }
}

spv::Capability getCapabilityForGroupNonUniform(spv::Op opcode) {
  switch (opcode) {
  case spv::Op::OpGroupNonUniformElect:
    return spv::Capability::GroupNonUniform;
  case spv::Op::OpGroupNonUniformAny:
  case spv::Op::OpGroupNonUniformAll:
  case spv::Op::OpGroupNonUniformAllEqual:
    return spv::Capability::GroupNonUniformVote;
  case spv::Op::OpGroupNonUniformBallot:
  case spv::Op::OpGroupNonUniformBallotBitCount:
  case spv::Op::OpGroupNonUniformBroadcast:
  case spv::Op::OpGroupNonUniformBroadcastFirst:
    return spv::Capability::GroupNonUniformBallot;
  case spv::Op::OpGroupNonUniformIAdd:
  case spv::Op::OpGroupNonUniformFAdd:
  case spv::Op::OpGroupNonUniformIMul:
  case spv::Op::OpGroupNonUniformFMul:
  case spv::Op::OpGroupNonUniformSMax:
  case spv::Op::OpGroupNonUniformUMax:
  case spv::Op::OpGroupNonUniformFMax:
  case spv::Op::OpGroupNonUniformSMin:
  case spv::Op::OpGroupNonUniformUMin:
  case spv::Op::OpGroupNonUniformFMin:
  case spv::Op::OpGroupNonUniformBitwiseAnd:
  case spv::Op::OpGroupNonUniformBitwiseOr:
  case spv::Op::OpGroupNonUniformBitwiseXor:
    return spv::Capability::GroupNonUniformArithmetic;
  case spv::Op::OpGroupNonUniformQuadBroadcast:
  case spv::Op::OpGroupNonUniformQuadSwap:
    return spv::Capability::GroupNonUniformQuad;
  default:
    assert(false && "unhandled opcode");
    break;
  }
  assert(false && "unhandled opcode");
  return spv::Capability::Max;
}

std::string getNamespacePrefix(const Decl *decl) {
  std::string nsPrefix = "";
  const DeclContext *dc = decl->getDeclContext();
  while (dc && !dc->isTranslationUnit()) {
    if (const NamespaceDecl *ns = dyn_cast<NamespaceDecl>(dc)) {
      if (!ns->isAnonymousNamespace()) {
        nsPrefix = ns->getName().str() + "::" + nsPrefix;
      }
    }
    dc = dc->getParent();
  }
  return nsPrefix;
}

std::string getFnName(const FunctionDecl *fn) {
  // Prefix the function name with the struct name if necessary
  std::string classOrStructName = "";
  if (const auto *memberFn = dyn_cast<CXXMethodDecl>(fn))
    if (const auto *st = dyn_cast<CXXRecordDecl>(memberFn->getDeclContext()))
      classOrStructName = st->getName().str() + ".";
  return getNamespacePrefix(fn) + classOrStructName + fn->getName().str();
}

/// Returns the capability required to non-uniformly index into the given type.
spv::Capability getNonUniformCapability(QualType type) {
  using spv::Capability;

  if (type->isArrayType()) {
    return getNonUniformCapability(
        type->getAsArrayTypeUnsafe()->getElementType());
  }
  if (TypeTranslator::isTexture(type) || TypeTranslator::isSampler(type)) {
    return Capability::SampledImageArrayNonUniformIndexingEXT;
  }
  if (TypeTranslator::isRWTexture(type)) {
    return Capability::StorageImageArrayNonUniformIndexingEXT;
  }
  if (TypeTranslator::isBuffer(type)) {
    return Capability::UniformTexelBufferArrayNonUniformIndexingEXT;
  }
  if (TypeTranslator::isRWBuffer(type)) {
    return Capability::StorageTexelBufferArrayNonUniformIndexingEXT;
  }
  if (const auto *recordType = type->getAs<RecordType>()) {
    const auto name = recordType->getDecl()->getName();

    if (name == "SubpassInput" || name == "SubpassInputMS") {
      return Capability::InputAttachmentArrayNonUniformIndexingEXT;
    }
  }

  return Capability::Max;
}

} // namespace

SPIRVEmitter::SPIRVEmitter(CompilerInstance &ci, EmitSPIRVOptions &options)
    : theCompilerInstance(ci), astContext(ci.getASTContext()),
      diags(ci.getDiagnostics()), spirvOptions(options),
      entryFunctionName(ci.getCodeGenOpts().HLSLEntryFunction),
      shaderModel(*hlsl::ShaderModel::GetByName(
          ci.getCodeGenOpts().HLSLProfile.c_str())),
      theContext(), featureManager(diags, options),
      theBuilder(&theContext, &featureManager, options.enableReflect),
      typeTranslator(astContext, theBuilder, diags, options),
      declIdMapper(shaderModel, astContext, theBuilder, typeTranslator,
                   featureManager, options),
      entryFunctionId(0), curFunction(nullptr), curThis(0),
      seenPushConstantAt(), isSpecConstantMode(false),
      foundNonUniformResourceIndex(false), needsLegalization(false) {
  if (shaderModel.GetKind() == hlsl::ShaderModel::Kind::Invalid)
    emitError("unknown shader module: %0", {}) << shaderModel.GetName();

  if (options.invertY && !shaderModel.IsVS() && !shaderModel.IsDS() &&
      !shaderModel.IsGS())
    emitError("-fvk-invert-y can only be used in VS/DS/GS", {});

  if (options.useGlLayout && options.useDxLayout)
    emitError("cannot specify both -fvk-use-dx-layout and -fvk-use-gl-layout",
              {});

  options.Initialize();

  // Set shader module version
  theBuilder.setShaderModelVersion(shaderModel.GetMajor(),
                                   shaderModel.GetMinor());

  // Set debug info
  const auto &inputFiles = ci.getFrontendOpts().Inputs;
  if (options.enableDebugInfo && !inputFiles.empty())
    theBuilder.setSourceFileName(theContext.takeNextId(),
                                 inputFiles.front().getFile().str());
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
    } else {
      doDecl(decl);
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

  const spv_target_env targetEnv = featureManager.getTargetEnv();

  AddRequiredCapabilitiesForShaderModel();

  // Addressing and memory model are required in a valid SPIR-V module.
  theBuilder.setAddressingModel(spv::AddressingModel::Logical);
  theBuilder.setMemoryModel(spv::MemoryModel::GLSL450);

  theBuilder.addEntryPoint(getSpirvShaderStage(shaderModel), entryFunctionId,
                           entryFunctionName, declIdMapper.collectStageVars());

  // Add Location decorations to stage input/output variables.
  if (!declIdMapper.decorateStageIOLocations())
    return;

  // Add descriptor set and binding decorations to resource variables.
  if (!declIdMapper.decorateResourceBindings())
    return;

  // Output the constructed module.
  std::vector<uint32_t> m = theBuilder.takeModule();

  if (!spirvOptions.codeGenHighLevel) {
    // Run legalization passes
    if (needsLegalization || declIdMapper.requiresLegalization()) {
      std::string messages;
      if (!spirvToolsLegalize(targetEnv, &m, &messages)) {
        emitFatalError("failed to legalize SPIR-V: %0", {}) << messages;
        emitNote("please file a bug report on "
                 "https://github.com/Microsoft/DirectXShaderCompiler/issues "
                 "with source code if possible",
                 {});
        return;
      } else if (!messages.empty()) {
        emitWarning("SPIR-V legalization: %0", {}) << messages;
      }
    }

    // Run optimization passes
    if (theCompilerInstance.getCodeGenOpts().OptimizationLevel > 0) {
      std::string messages;
      if (!spirvToolsOptimize(targetEnv, &m, &messages)) {
        emitFatalError("failed to optimize SPIR-V: %0", {}) << messages;
        emitNote("please file a bug report on "
                 "https://github.com/Microsoft/DirectXShaderCompiler/issues "
                 "with source code if possible",
                 {});
        return;
      }
    }
  }

  // Validate the generated SPIR-V code
  if (!spirvOptions.disableValidation) {
    std::string messages;
    if (!spirvToolsValidate(targetEnv, &m, &messages,
                            declIdMapper.requiresLegalization())) {
      emitFatalError("generated SPIR-V is invalid: %0", {}) << messages;
      emitNote("please file a bug report on "
               "https://github.com/Microsoft/DirectXShaderCompiler/issues "
               "with source code if possible",
               {});
      return;
    }
  }

  theCompilerInstance.getOutStream()->write(
      reinterpret_cast<const char *>(m.data()), m.size() * 4);
}

void SPIRVEmitter::doDecl(const Decl *decl) {
  if (decl->isImplicit() || isa<EmptyDecl>(decl) || isa<TypedefDecl>(decl))
    return;

  if (const auto *varDecl = dyn_cast<VarDecl>(decl)) {
    // We can have VarDecls inside cbuffer/tbuffer. For those VarDecls, we need
    // to emit their cbuffer/tbuffer as a whole and access each individual one
    // using access chains.
    if (const auto *bufferDecl = getCTBufferContext(varDecl)) {
      doHLSLBufferDecl(bufferDecl);
    } else {
      doVarDecl(varDecl);
    }
  } else if (const auto *namespaceDecl = dyn_cast<NamespaceDecl>(decl)) {
    for (auto *subDecl : namespaceDecl->decls())
      // Note: We only emit functions as they are discovered through the call
      // graph starting from the entry-point. We should not emit unused
      // functions inside namespaces.
      if (!isa<FunctionDecl>(subDecl))
        doDecl(subDecl);
  } else if (const auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
    doFunctionDecl(funcDecl);
  } else if (const auto *bufferDecl = dyn_cast<HLSLBufferDecl>(decl)) {
    doHLSLBufferDecl(bufferDecl);
  } else if (const auto *recordDecl = dyn_cast<RecordDecl>(decl)) {
    doRecordDecl(recordDecl);
  } else {
    emitError("decl type %0 unimplemented", decl->getLocation())
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
    doIfStmt(ifStmt, attrs);
  } else if (const auto *switchStmt = dyn_cast<SwitchStmt>(stmt)) {
    doSwitchStmt(switchStmt, attrs);
  } else if (dyn_cast<CaseStmt>(stmt)) {
    processCaseStmtOrDefaultStmt(stmt);
  } else if (dyn_cast<DefaultStmt>(stmt)) {
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
  } else if (dyn_cast<NullStmt>(stmt)) {
    // For the null statement ";". We don't need to do anything.
  } else if (const auto *expr = dyn_cast<Expr>(stmt)) {
    // All cases for expressions used as statements
    doExpr(expr);
  } else if (const auto *attrStmt = dyn_cast<AttributedStmt>(stmt)) {
    doStmt(attrStmt->getSubStmt(), attrStmt->getAttrs());
  } else {
    emitError("statement class '%0' unimplemented", stmt->getLocStart())
        << stmt->getStmtClassName() << stmt->getSourceRange();
  }
}

SpirvEvalInfo SPIRVEmitter::doExpr(const Expr *expr) {
  SpirvEvalInfo result(/*id*/ 0);

  // Provide a hint to the typeTranslator that if a literal is discovered, its
  // intended usage is as this expression type.
  TypeTranslator::LiteralTypeHint hint(typeTranslator, expr->getType());

  expr = expr->IgnoreParens();

  if (const auto *declRefExpr = dyn_cast<DeclRefExpr>(expr)) {
    result = declIdMapper.getDeclEvalInfo(declRefExpr->getDecl());
  } else if (const auto *memberExpr = dyn_cast<MemberExpr>(expr)) {
    result = doMemberExpr(memberExpr);
  } else if (const auto *castExpr = dyn_cast<CastExpr>(expr)) {
    result = doCastExpr(castExpr);
  } else if (const auto *initListExpr = dyn_cast<InitListExpr>(expr)) {
    result = doInitListExpr(initListExpr);
  } else if (const auto *boolLiteral = dyn_cast<CXXBoolLiteralExpr>(expr)) {
    const auto value =
        theBuilder.getConstantBool(boolLiteral->getValue(), isSpecConstantMode);
    result = SpirvEvalInfo(value).setConstant().setRValue();
  } else if (const auto *intLiteral = dyn_cast<IntegerLiteral>(expr)) {
    const auto value = translateAPInt(intLiteral->getValue(), expr->getType());
    result = SpirvEvalInfo(value).setConstant().setRValue();
  } else if (const auto *floatLiteral = dyn_cast<FloatingLiteral>(expr)) {
    const auto value =
        translateAPFloat(floatLiteral->getValue(), expr->getType());
    result = SpirvEvalInfo(value).setConstant().setRValue();
  } else if (const auto *compoundAssignOp =
                 dyn_cast<CompoundAssignOperator>(expr)) {
    // CompoundAssignOperator is a subclass of BinaryOperator. It should be
    // checked before BinaryOperator.
    result = doCompoundAssignOperator(compoundAssignOp);
  } else if (const auto *binOp = dyn_cast<BinaryOperator>(expr)) {
    result = doBinaryOperator(binOp);
  } else if (const auto *unaryOp = dyn_cast<UnaryOperator>(expr)) {
    result = doUnaryOperator(unaryOp);
  } else if (const auto *vecElemExpr = dyn_cast<HLSLVectorElementExpr>(expr)) {
    result = doHLSLVectorElementExpr(vecElemExpr);
  } else if (const auto *matElemExpr = dyn_cast<ExtMatrixElementExpr>(expr)) {
    result = doExtMatrixElementExpr(matElemExpr);
  } else if (const auto *funcCall = dyn_cast<CallExpr>(expr)) {
    result = doCallExpr(funcCall);
  } else if (const auto *subscriptExpr = dyn_cast<ArraySubscriptExpr>(expr)) {
    result = doArraySubscriptExpr(subscriptExpr);
  } else if (const auto *condExpr = dyn_cast<ConditionalOperator>(expr)) {
    result = doConditionalOperator(condExpr);
  } else if (const auto *defaultArgExpr = dyn_cast<CXXDefaultArgExpr>(expr)) {
    result = doExpr(defaultArgExpr->getParam()->getDefaultArg());
  } else if (isa<CXXThisExpr>(expr)) {
    assert(curThis);
    result = curThis;
  } else {
    emitError("expression class '%0' unimplemented", expr->getExprLoc())
        << expr->getStmtClassName() << expr->getSourceRange();
  }

  return result;
}

SpirvEvalInfo SPIRVEmitter::loadIfGLValue(const Expr *expr) {
  // We are trying to load the value here, which is what an LValueToRValue
  // implicit cast is intended to do. We can ignore the cast if exists.
  expr = expr->IgnoreParenLValueCasts();

  return loadIfGLValue(expr, doExpr(expr));
}

SpirvEvalInfo SPIRVEmitter::loadIfGLValue(const Expr *expr,
                                          SpirvEvalInfo info) {
  // Do nothing if this is already rvalue
  if (info.isRValue())
    return info;

  // Check whether we are trying to load an array of opaque objects as a whole.
  // If true, we are likely to copy it as a whole. To assist per-element
  // copying, avoid the load here and return the pointer directly.
  // TODO: consider moving this hack into SPIRV-Tools as a transformation.
  if (TypeTranslator::isOpaqueArrayType(expr->getType()))
    return info;

  // Check whether we are trying to load an externally visible structured/byte
  // buffer as a whole. If true, it means we are creating alias for it. Avoid
  // the load and write the pointer directly to the alias variable then.
  //
  // Also for the case of alias function returns. If we are trying to load an
  // alias function return as a whole, it means we are assigning it to another
  // alias variable. Avoid the load and write the pointer directly.
  //
  // Note: legalization specific code
  if (isReferencingNonAliasStructuredOrByteBuffer(expr)) {
    return info.setRValue();
  }

  if (loadIfAliasVarRef(expr, info)) {
    // We are loading an alias variable as a whole here. This is likely for
    // wholesale assignments or function returns. Need to load the pointer.
    //
    // Note: legalization specific code
    return info;
  }

  uint32_t valType = 0;
  // TODO: Ouch. Very hacky. We need special path to get the value type if
  // we are loading a whole ConstantBuffer/TextureBuffer since the normal
  // type translation path won't work.
  if (const auto *declContext = isConstantTextureBufferDeclRef(expr)) {
    valType = declIdMapper.getCTBufferPushConstantTypeId(declContext);
  } else {
    valType =
        typeTranslator.translateType(expr->getType(), info.getLayoutRule());
  }

  uint32_t loadedId = theBuilder.createLoad(valType, info);

  // Decorate with NonUniformEXT if loading from a pointer with that property.
  // We are likely loading an element from the resource array here.
  if (info.isNonUniform()) {
    theBuilder.decorateNonUniformEXT(loadedId);
  }

  // Special-case: According to the SPIR-V Spec: There is no physical size or
  // bit pattern defined for boolean type. Therefore an unsigned integer is used
  // to represent booleans when layout is required. In such cases, after loading
  // the uint, we should perform a comparison.
  {
    uint32_t vecSize = 1, numRows = 0, numCols = 0;
    if (info.getLayoutRule() != LayoutRule::Void &&
        isBoolOrVecMatOfBoolType(expr->getType())) {
      const auto exprType = expr->getType();
      QualType uintType = astContext.UnsignedIntTy;
      QualType boolType = astContext.BoolTy;
      if (TypeTranslator::isScalarType(exprType) ||
          TypeTranslator::isVectorType(exprType, nullptr, &vecSize)) {
        const auto fromType =
            vecSize == 1 ? uintType
                         : astContext.getExtVectorType(uintType, vecSize);
        const auto toType =
            vecSize == 1 ? boolType
                         : astContext.getExtVectorType(boolType, vecSize);
        loadedId = castToBool(loadedId, fromType, toType);
      } else {
        const bool isMat =
            TypeTranslator::isMxNMatrix(exprType, nullptr, &numRows, &numCols);
        assert(isMat);
        (void)isMat;
        const auto uintRowQualType =
            astContext.getExtVectorType(uintType, numCols);
        const auto uintRowQualTypeId =
            typeTranslator.translateType(uintRowQualType);
        const auto boolRowQualType =
            astContext.getExtVectorType(boolType, numCols);
        const auto boolRowQualTypeId =
            typeTranslator.translateType(boolRowQualType);
        const uint32_t resultTypeId =
            theBuilder.getMatType(boolType, boolRowQualTypeId, numRows);
        llvm::SmallVector<uint32_t, 4> rows;
        for (uint32_t i = 0; i < numRows; ++i) {
          const auto row = theBuilder.createCompositeExtract(uintRowQualTypeId,
                                                             loadedId, {i});
          rows.push_back(castToBool(row, uintRowQualType, boolRowQualType));
        }
        loadedId = theBuilder.createCompositeConstruct(resultTypeId, rows);
      }
      // Now that it is converted to Bool, it has no layout rule.
      // This result-id should be evaluated as bool from here on out.
      info.setLayoutRule(LayoutRule::Void);
    }
  }

  return info.setResultId(loadedId).setRValue();
}

SpirvEvalInfo SPIRVEmitter::loadIfAliasVarRef(const Expr *expr) {
  auto info = doExpr(expr);
  loadIfAliasVarRef(expr, info);
  return info;
}

bool SPIRVEmitter::loadIfAliasVarRef(const Expr *varExpr, SpirvEvalInfo &info) {
  if (info.containsAliasComponent() &&
      TypeTranslator::isAKindOfStructuredOrByteBuffer(varExpr->getType())) {
    // Aliased-to variables are all in the Uniform storage class with GLSL
    // std430 layout rules.
    const auto ptrType = typeTranslator.translateType(varExpr->getType());

    // Load the pointer of the aliased-to-variable if the expression has a
    // pointer to pointer type. That is, the expression itself is a lvalue.
    // (Note that we translate alias function return values as pointer types,
    // not pointer to pointer types.)
    if (varExpr->isGLValue())
      info.setResultId(theBuilder.createLoad(ptrType, info));

    info.setStorageClass(spv::StorageClass::Uniform)
        .setLayoutRule(spirvOptions.sBufferLayoutRule)
        // Now it is a pointer to the global resource, which is lvalue.
        .setRValue(false)
        // Set to false to indicate that we've performed dereference over the
        // pointer-to-pointer and now should fallback to the normal path
        .setContainsAliasComponent(false);

    return true;
  }

  return false;
}

uint32_t SPIRVEmitter::castToType(uint32_t value, QualType fromType,
                                  QualType toType, SourceLocation srcLoc) {
  if (isFloatOrVecOfFloatType(toType))
    return castToFloat(value, fromType, toType, srcLoc);

  // Order matters here. Bool (vector) values will also be considered as uint
  // (vector) values. So given a bool (vector) argument, isUintOrVecOfUintType()
  // will also return true. We need to check bool before uint. The opposite is
  // not true.
  if (isBoolOrVecOfBoolType(toType))
    return castToBool(value, fromType, toType);

  if (isSintOrVecOfSintType(toType) || isUintOrVecOfUintType(toType))
    return castToInt(value, fromType, toType, srcLoc);

  emitError("casting to type %0 unimplemented", {}) << toType;
  return 0;
}

void SPIRVEmitter::doFunctionDecl(const FunctionDecl *decl) {
  assert(decl->isThisDeclarationADefinition());

  // A RAII class for maintaining the current function under traversal.
  class FnEnvRAII {
  public:
    // Creates a new instance which sets fnEnv to the newFn on creation,
    // and resets fnEnv to its original value on destruction.
    FnEnvRAII(const FunctionDecl **fnEnv, const FunctionDecl *newFn)
        : oldFn(*fnEnv), fnSlot(fnEnv) {
      *fnEnv = newFn;
    }
    ~FnEnvRAII() { *fnSlot = oldFn; }

  private:
    const FunctionDecl *oldFn;
    const FunctionDecl **fnSlot;
  };

  FnEnvRAII fnEnvRAII(&curFunction, decl);

  // We are about to start translation for a new function. Clear the break stack
  // and the continue stack.
  breakStack = std::stack<uint32_t>();
  continueStack = std::stack<uint32_t>();

  // This will allow the entry-point name to be something like
  // myNamespace::myEntrypointFunc.
  std::string funcName = getFnName(decl);

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
    funcId = declIdMapper.getDeclEvalInfo(decl);
  }

  const uint32_t retType =
      declIdMapper.getTypeAndCreateCounterForPotentialAliasVar(decl);

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
  }

  for (const auto *param : decl->params()) {
    const uint32_t valueType =
        declIdMapper.getTypeAndCreateCounterForPotentialAliasVar(param);
    const uint32_t ptrType =
        theBuilder.getPointerType(valueType, spv::StorageClass::Function);
    paramTypes.push_back(ptrType);
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
    (void)declIdMapper.createFnParam(paramDecl);
  }

  if (decl->hasBody()) {
    // The entry basic block.
    const uint32_t entryLabel = theBuilder.createBasicBlock("bb.entry");
    theBuilder.setInsertPoint(entryLabel);

    // Process all statments in the body.
    doStmt(decl->getBody());

    // We have processed all Stmts in this function and now in the last
    // basic block. Make sure we have a termination instruction.
    if (!theBuilder.isCurrentBasicBlockTerminated()) {
      const auto retType = decl->getReturnType();

      if (retType->isVoidType()) {
        theBuilder.createReturn();
      } else {
        // If the source code does not provide a proper return value for some
        // control flow path, it's undefined behavior. We just return null
        // value here.
        theBuilder.createReturnValue(
            theBuilder.getConstantNull(typeTranslator.translateType(retType)));
      }
    }
  }

  theBuilder.endFunction();
}

bool SPIRVEmitter::validateVKAttributes(const NamedDecl *decl) {
  bool success = true;

  if (const auto *varDecl = dyn_cast<VarDecl>(decl)) {
    const auto varType = varDecl->getType();
    if ((TypeTranslator::isSubpassInput(varType) ||
         TypeTranslator::isSubpassInputMS(varType)) &&
        !varDecl->hasAttr<VKInputAttachmentIndexAttr>()) {
      emitError("missing vk::input_attachment_index attribute",
                varDecl->getLocation());
      success = false;
    }
  }

  if (decl->getAttr<VKInputAttachmentIndexAttr>()) {
    if (!shaderModel.IsPS()) {
      emitError("SubpassInput(MS) only allowed in pixel shader",
                decl->getLocation());
      success = false;
    }

    if (!decl->isExternallyVisible()) {
      emitError("SubpassInput(MS) must be externally visible",
                decl->getLocation());
      success = false;
    }

    // We only allow VKInputAttachmentIndexAttr to be attached to global
    // variables. So it should be fine to cast here.
    const auto elementType =
        hlsl::GetHLSLResourceResultType(cast<VarDecl>(decl)->getType());

    if (!TypeTranslator::isScalarType(elementType) &&
        !TypeTranslator::isVectorType(elementType)) {
      emitError(
          "only scalar/vector types allowed as SubpassInput(MS) parameter type",
          decl->getLocation());
      // Return directly to avoid further type processing, which will hit
      // asserts in TypeTranslator.
      return false;
    }
  }

  // The frontend will make sure that
  // * vk::push_constant applies to global variables of struct type
  // * vk::binding applies to global variables or cbuffers/tbuffers
  // * vk::counter_binding applies to global variables of RW/Append/Consume
  //   StructuredBuffer
  // * vk::location applies to function parameters/returns and struct fields
  // So the only case we need to check co-existence is vk::push_constant and
  // vk::binding.

  if (const auto *pcAttr = decl->getAttr<VKPushConstantAttr>()) {
    const auto loc = pcAttr->getLocation();

    if (seenPushConstantAt.isInvalid()) {
      seenPushConstantAt = loc;
    } else {
      // TODO: Actually this is slightly incorrect. The Vulkan spec says:
      //   There must be no more than one push constant block statically used
      //   per shader entry point.
      // But we are checking whether there are more than one push constant
      // blocks defined. Tracking usage requires more work.
      emitError("cannot have more than one push constant block", loc);
      emitNote("push constant block previously defined here",
               seenPushConstantAt);
      success = false;
    }

    if (decl->hasAttr<VKBindingAttr>()) {
      emitError("vk::push_constant attribute cannot be used together with "
                "vk::binding attribute",
                loc);
      success = false;
    }
  }

  return success;
}

void SPIRVEmitter::doHLSLBufferDecl(const HLSLBufferDecl *bufferDecl) {
  // This is a cbuffer/tbuffer decl.
  // Check and emit warnings for member intializers which are not
  // supported in Vulkan
  for (const auto *member : bufferDecl->decls()) {
    if (const auto *varMember = dyn_cast<VarDecl>(member)) {
      if (!spirvOptions.noWarnIgnoredFeatures) {
        if (const auto *init = varMember->getInit())
          emitWarning("%select{tbuffer|cbuffer}0 member initializer "
                      "ignored since no Vulkan equivalent",
                      init->getExprLoc())
              << bufferDecl->isCBuffer() << init->getSourceRange();
      }

      // We cannot handle external initialization of column-major matrices now.
      if (typeTranslator.isOrContainsNonFpColMajorMatrix(varMember->getType(),
                                                         varMember)) {
        emitError("externally initialized non-floating-point column-major "
                  "matrices not supported yet",
                  varMember->getLocation());
      }
    }
  }
  if (!validateVKAttributes(bufferDecl))
    return;
  (void)declIdMapper.createCTBuffer(bufferDecl);
}

void SPIRVEmitter::doRecordDecl(const RecordDecl *recordDecl) {
  // Ignore implict records
  // Somehow we'll have implicit records with:
  //   static const int Length = count;
  // that can mess up with the normal CodeGen.
  if (recordDecl->isImplicit())
    return;

  // Handle each static member with inline initializer.
  // Each static member has a corresponding VarDecl inside the
  // RecordDecl. For those defined in the translation unit,
  // their VarDecls do not have initializer.
  for (auto *subDecl : recordDecl->decls())
    if (auto *varDecl = dyn_cast<VarDecl>(subDecl))
      if (varDecl->isStaticDataMember() && varDecl->hasInit())
        doVarDecl(varDecl);
}

void SPIRVEmitter::doVarDecl(const VarDecl *decl) {
  if (!validateVKAttributes(decl))
    return;

  // We cannot handle external initialization of column-major matrices now.
  if (isExternalVar(decl) &&
      typeTranslator.isOrContainsNonFpColMajorMatrix(decl->getType(), decl)) {
    emitError("externally initialized non-floating-point column-major "
              "matrices not supported yet",
              decl->getLocation());
  }

  // Reject arrays of RW/append/consume structured buffers. They have assoicated
  // counters, which are quite nasty to handle.
  if (decl->getType()->isArrayType()) {
    auto type = decl->getType();
    do {
      type = type->getAsArrayTypeUnsafe()->getElementType();
    } while (type->isArrayType());

    if (TypeTranslator::isRWAppendConsumeSBuffer(type)) {
      emitError("arrays of RW/append/consume structured buffers unsupported",
                decl->getLocation());
      return;
    }
  }

  if (decl->hasAttr<VKConstantIdAttr>()) {
    // This is a VarDecl for specialization constant.
    createSpecConstant(decl);
    return;
  }

  if (decl->hasAttr<VKPushConstantAttr>()) {
    // This is a VarDecl for PushConstant block.
    (void)declIdMapper.createPushConstant(decl);
    return;
  }

  if (isa<HLSLBufferDecl>(decl->getDeclContext())) {
    // This is a VarDecl of a ConstantBuffer/TextureBuffer type.
    (void)declIdMapper.createCTBuffer(decl);
    return;
  }

  SpirvEvalInfo varId(0);

  // The contents in externally visible variables can be updated via the
  // pipeline. They should be handled differently from file and function scope
  // variables.
  // File scope variables (static "global" and "local" variables) belongs to
  // the Private storage class, while function scope variables (normal "local"
  // variables) belongs to the Function storage class.
  if (isExternalVar(decl)) {
    varId = declIdMapper.createExternVar(decl);
  } else {
    // We already know the variable is not externally visible here. If it does
    // not have local storage, it should be file scope variable.
    const bool isFileScopeVar = !decl->hasLocalStorage();

    if (isFileScopeVar)
      varId = declIdMapper.createFileVar(decl, llvm::None);
    else
      varId = declIdMapper.createFnVar(decl, llvm::None);

    // Emit OpStore to initialize the variable
    // TODO: revert back to use OpVariable initializer

    // We should only evaluate the initializer once for a static variable.
    if (isFileScopeVar) {
      if (decl->isStaticLocal()) {
        initOnce(decl->getType(), decl->getName(), varId, decl->getInit());
      } else {
        // Defer to initialize these global variables at the beginning of the
        // entry function.
        toInitGloalVars.push_back(decl);
      }

    }
    // Function local variables. Just emit OpStore at the current insert point.
    else if (const Expr *init = decl->getInit()) {
      if (const auto constId = tryToEvaluateAsConst(init))
        theBuilder.createStore(varId, constId);
      else
        storeValue(varId, loadIfGLValue(init), decl->getType());

      // Update counter variable associated with local variables
      tryToAssignCounterVar(decl, init);
    }

    // Variables that are not externally visible and of opaque types should
    // request legalization.
    if (!needsLegalization && TypeTranslator::isOpaqueType(decl->getType()))
      needsLegalization = true;
  }

  if (TypeTranslator::isRelaxedPrecisionType(decl->getType(), spirvOptions)) {
    theBuilder.decorateRelaxedPrecision(varId);
  }

  // All variables that are of opaque struct types should request legalization.
  if (!needsLegalization && TypeTranslator::isOpaqueStructType(decl->getType()))
    needsLegalization = true;
}

spv::LoopControlMask SPIRVEmitter::translateLoopAttribute(const Stmt *stmt,
                                                          const Attr &attr) {
  switch (attr.getKind()) {
  case attr::HLSLLoop:
  case attr::HLSLFastOpt:
    return spv::LoopControlMask::DontUnroll;
  case attr::HLSLUnroll:
    return spv::LoopControlMask::Unroll;
  case attr::HLSLAllowUAVCondition:
    emitWarning("unsupported allow_uav_condition attribute ignored",
                stmt->getLocStart());
    break;
  default:
    llvm_unreachable("found unknown loop attribute");
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
                    : translateLoopAttribute(theDoStmt, *attrs.front());

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
                    : translateLoopAttribute(whileStmt, *attrs.front());

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
                    : translateLoopAttribute(forStmt, *attrs.front());

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

void SPIRVEmitter::doIfStmt(const IfStmt *ifStmt,
                            llvm::ArrayRef<const Attr *> attrs) {
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

  auto selectionControl = spv::SelectionControlMask::MaskNone;
  if (!attrs.empty()) {
    const Attr *attribute = attrs.front();
    switch (attribute->getKind()) {
    case attr::HLSLBranch:
      selectionControl = spv::SelectionControlMask::DontFlatten;
      break;
    case attr::HLSLFlatten:
      selectionControl = spv::SelectionControlMask::Flatten;
      break;
    default:
      emitWarning("unknown if statement attribute '%0' ignored",
                  attribute->getLocation())
          << attribute->getSpelling();
      break;
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
  theBuilder.createConditionalBranch(condition, thenBB, elseBB, mergeBB,
                                     /*continue*/ 0, selectionControl);
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
    // Update counter variable associated with function returns
    tryToAssignCounterVar(curFunction, retVal);

    const auto retInfo = loadIfGLValue(retVal);
    const auto retType = retVal->getType();
    if (retInfo.getStorageClass() != spv::StorageClass::Function &&
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

  // We are translating a ReturnStmt, we should be in some function's body.
  assert(curFunction->hasBody());
  // If this return statement is the last statement in the function, then
  // whe have no more work to do.
  if (cast<CompoundStmt>(curFunction->getBody())->body_back() == stmt)
    return;

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
    emitWarning("ignored 'forcecase' attribute for the switch statement "
                "since one or more case values are not integer literals",
                switchStmt->getLocStart());

  if (canUseSpirvOpSwitch)
    processSwitchStmtUsingSpirvOpSwitch(switchStmt);
  else
    processSwitchStmtUsingIfStmts(switchStmt);
}

SpirvEvalInfo
SPIRVEmitter::doArraySubscriptExpr(const ArraySubscriptExpr *expr) {
  // Make sure we don't have previously unhandled NonUniformResourceIndex()
  assert(!foundNonUniformResourceIndex);

  llvm::SmallVector<uint32_t, 4> indices;
  const auto *base = collectArrayStructIndices(expr, &indices);
  auto info = loadIfAliasVarRef(base);

  if (foundNonUniformResourceIndex) {
    // Add the necessary capability required for indexing into this kind
    // of resource
    theBuilder.requireCapability(getNonUniformCapability(base->getType()));
    info.setNonUniform(); // Carry forward the NonUniformEXT decoration
    foundNonUniformResourceIndex = false;
  }

  if (!indices.empty()) {
    (void)turnIntoElementPtr(info, expr->getType(), indices);
  }

  return info;
}

SpirvEvalInfo SPIRVEmitter::doBinaryOperator(const BinaryOperator *expr) {
  const auto opcode = expr->getOpcode();

  // Handle assignment first since we need to evaluate rhs before lhs.
  // For other binary operations, we need to evaluate lhs before rhs.
  if (opcode == BO_Assign) {
    // Update counter variable associated with lhs of assignments
    tryToAssignCounterVar(expr->getLHS(), expr->getRHS());

    return processAssignment(expr->getLHS(), loadIfGLValue(expr->getRHS()),
                             /*isCompoundAssignment=*/false);
  }

  // Try to optimize floatMxN * float and floatN * float case
  if (opcode == BO_Mul) {
    if (SpirvEvalInfo result = tryToGenFloatMatrixScale(expr))
      return result;
    if (SpirvEvalInfo result = tryToGenFloatVectorScale(expr))
      return result;
  }

  return processBinaryOp(expr->getLHS(), expr->getRHS(), opcode,
                         expr->getLHS()->getType(), expr->getType(),
                         expr->getSourceRange());
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

SpirvEvalInfo SPIRVEmitter::processCall(const CallExpr *callExpr) {
  const FunctionDecl *callee = getCalleeDefinition(callExpr);

  // Note that we always want the defintion because Stmts/Exprs in the
  // function body references the parameters in the definition.
  if (!callee) {
    emitError("found undefined function", callExpr->getExprLoc());
    return 0;
  }

  const auto numParams = callee->getNumParams();

  bool isNonStaticMemberCall = false;
  QualType objectType = {};         // Type of the object (if exists)
  SpirvEvalInfo objectEvalInfo = 0; // EvalInfo for the object (if exists)
  bool needsTempVar = false;        // Whether we need temporary variable.

  llvm::SmallVector<uint32_t, 4> vars;      // Variables for function call
  llvm::SmallVector<bool, 4> isTempVar;     // Temporary variable or not
  llvm::SmallVector<SpirvEvalInfo, 4> args; // Evaluated arguments

  if (const auto *memberCall = dyn_cast<CXXMemberCallExpr>(callExpr)) {
    const auto *memberFn = cast<CXXMethodDecl>(memberCall->getCalleeDecl());
    isNonStaticMemberCall = !memberFn->isStatic();

    if (isNonStaticMemberCall) {
      // For non-static member calls, evaluate the object and pass it as the
      // first argument.
      const auto *object = memberCall->getImplicitObjectArgument();
      object = object->IgnoreParenNoopCasts(astContext);

      // Update counter variable associated with the implicit object
      tryToAssignCounterVar(getOrCreateDeclForMethodObject(memberFn), object);

      objectType = object->getType();
      objectEvalInfo = doExpr(object);
      uint32_t objectId = objectEvalInfo;

      // If not already a variable, we need to create a temporary variable and
      // pass the object pointer to the function. Example:
      // getObject().objectMethod();
      // Also, any parameter passed to the member function must be of Function
      // storage class.
      needsTempVar =
          objectEvalInfo.isRValue() ||
          objectEvalInfo.getStorageClass() != spv::StorageClass::Function;

      if (needsTempVar) {
        objectId =
            createTemporaryVar(objectType, TypeTranslator::getName(objectType),
                               // May need to load to use as initializer
                               loadIfGLValue(object, objectEvalInfo));
      }

      args.push_back(objectId);
      // We do not need to create a new temporary variable for the this
      // object. Use the evaluated argument.
      vars.push_back(args.back());
      isTempVar.push_back(false);
    }
  }

  // Evaluate parameters
  for (uint32_t i = 0; i < numParams; ++i) {
    // We want the argument variable here so that we can write back to it
    // later. We will do the OpLoad of this argument manually. So ingore
    // the LValueToRValue implicit cast here.
    auto *arg = callExpr->getArg(i)->IgnoreParenLValueCasts();
    const auto *param = callee->getParamDecl(i);

    // Get the evaluation info if this argument is referencing some variable
    // *as a whole*, in which case we can avoid creating the temporary variable
    // for it if it is Function scope and can act as out parameter.
    SpirvEvalInfo argInfo = 0;
    if (const auto *declRefExpr = dyn_cast<DeclRefExpr>(arg)) {
      argInfo = declIdMapper.getDeclEvalInfo(declRefExpr->getDecl());
    }

    if (argInfo && argInfo.getStorageClass() == spv::StorageClass::Function &&
        canActAsOutParmVar(param)) {
      vars.push_back(argInfo);
      isTempVar.push_back(false);
      args.push_back(doExpr(arg));
    } else {
      // We need to create variables for holding the values to be used as
      // arguments. The variables themselves are of pointer types.
      const uint32_t varType =
          declIdMapper.getTypeAndCreateCounterForPotentialAliasVar(param);
      const std::string varName = "param.var." + param->getNameAsString();
      const uint32_t tempVarId = theBuilder.addFnVar(varType, varName);

      vars.push_back(tempVarId);
      isTempVar.push_back(true);
      args.push_back(doExpr(arg));

      // Update counter variable associated with function parameters
      tryToAssignCounterVar(param, arg);

      // Manually load the argument here
      const auto rhsVal = loadIfGLValue(arg, args.back());
      // Initialize the temporary variables using the contents of the arguments
      storeValue(tempVarId, rhsVal, param->getType());
    }
  }

  assert(vars.size() == isTempVar.size());
  assert(vars.size() == args.size());

  // Push the callee into the work queue if it is not there.
  if (!workQueue.count(callee)) {
    workQueue.insert(callee);
  }

  const uint32_t retType =
      declIdMapper.getTypeAndCreateCounterForPotentialAliasVar(callee);
  // Get or forward declare the function <result-id>
  const uint32_t funcId = declIdMapper.getOrRegisterFnResultId(callee);

  const uint32_t retVal = theBuilder.createFunctionCall(retType, funcId, vars);

  // If we created a temporary variable for the lvalue object this method is
  // invoked upon, we need to copy the contents in the temporary variable back
  // to the original object's variable in case there are side effects.
  if (needsTempVar && !objectEvalInfo.isRValue()) {
    const uint32_t typeId = typeTranslator.translateType(objectType);
    const uint32_t value = theBuilder.createLoad(typeId, vars.front());
    storeValue(objectEvalInfo, value, objectType);
  }

  // Go through all parameters and write those marked as out/inout
  for (uint32_t i = 0; i < numParams; ++i) {
    const auto *param = callee->getParamDecl(i);
    if (isTempVar[i] && canActAsOutParmVar(param)) {
      const auto *arg = callExpr->getArg(i);
      const uint32_t index = i + isNonStaticMemberCall;
      const uint32_t typeId = typeTranslator.translateType(param->getType());
      const uint32_t value = theBuilder.createLoad(typeId, vars[index]);

      processAssignment(arg, value, false, args[index]);
    }
  }

  // Inherit the SpirvEvalInfo from the function definition
  return declIdMapper.getDeclEvalInfo(callee).setResultId(retVal);
}

SpirvEvalInfo SPIRVEmitter::doCastExpr(const CastExpr *expr) {
  const Expr *subExpr = expr->getSubExpr();
  const QualType subExprType = subExpr->getType();
  const QualType toType = expr->getType();

  // Unfortunately the front-end fails to deduce some types in certain cases.
  // Provide a hint about literal type usage if possible.
  TypeTranslator::LiteralTypeHint hint(typeTranslator);

  // 'literal int' to 'float' conversion. If a literal integer is to be used as
  // a 32-bit float, the hint is a 32-bit integer.
  if (toType->isFloatingType() &&
      subExprType->isSpecificBuiltinType(BuiltinType::LitInt) &&
      llvm::APFloat::getSizeInBits(astContext.getFloatTypeSemantics(toType)) ==
          32)
    hint.setHint(astContext.IntTy);
  // 'literal float' to 'float' conversion where intended type is float32.
  if (toType->isFloatingType() &&
      subExprType->isSpecificBuiltinType(BuiltinType::LitFloat) &&
      llvm::APFloat::getSizeInBits(astContext.getFloatTypeSemantics(toType)) ==
          32)
    hint.setHint(astContext.FloatTy);

  // TODO: We could provide other useful hints. For instance:
  // For the case of toType being a boolean, if the fromType is a literal float,
  // we could provide a FloatTy hint and if the fromType is a literal integer,
  // we could provide an IntTy hint. The front-end, however, seems to deduce the
  // correct type in these cases; therefore we currently don't provide any
  // additional hints.

  switch (expr->getCastKind()) {
  case CastKind::CK_LValueToRValue:
    return loadIfGLValue(subExpr);
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
    if (const uint32_t valueId = tryToEvaluateAsConst(expr))
      return SpirvEvalInfo(valueId).setConstant().setRValue();

    const auto valueId =
        castToInt(doExpr(subExpr), subExprType, toType, subExpr->getExprLoc());
    return SpirvEvalInfo(valueId).setRValue();
  }
  case CastKind::CK_FloatingCast:
  case CastKind::CK_IntegralToFloating:
  case CastKind::CK_HLSLCC_FloatingCast:
  case CastKind::CK_HLSLCC_IntegralToFloating: {
    // First try to see if we can do constant folding for floating point
    // numbers like what we are doing for integers in the above.
    if (const uint32_t valueId = tryToEvaluateAsConst(expr))
      return SpirvEvalInfo(valueId).setConstant().setRValue();

    const auto valueId = castToFloat(doExpr(subExpr), subExprType, toType,
                                     subExpr->getExprLoc());
    return SpirvEvalInfo(valueId).setRValue();
  }
  case CastKind::CK_IntegralToBoolean:
  case CastKind::CK_FloatingToBoolean:
  case CastKind::CK_HLSLCC_IntegralToBoolean:
  case CastKind::CK_HLSLCC_FloatingToBoolean: {
    // First try to see if we can do constant folding.
    if (const uint32_t valueId = tryToEvaluateAsConst(expr))
      return SpirvEvalInfo(valueId).setConstant().setRValue();

    const auto valueId = castToBool(doExpr(subExpr), subExprType, toType);
    return SpirvEvalInfo(valueId).setRValue();
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

    auto valueId = elements.front();
    if (toSize > 1)
      valueId = theBuilder.createCompositeConstruct(toVecTypeId, elements);

    return SpirvEvalInfo(valueId).setRValue();
  }
  case CastKind::CK_HLSLVectorToScalarCast: {
    // The underlying should already be a vector of size 1.
    assert(hlsl::GetHLSLVecSize(subExprType) == 1);
    return doExpr(subExpr);
  }
  case CastKind::CK_HLSLVectorToMatrixCast: {
    // If target type is already an 1xN matrix type, we just return the
    // underlying vector.
    if (TypeTranslator::is1xNMatrix(toType))
      return doExpr(subExpr);

    // A vector can have no more than 4 elements. The only remaining case
    // is casting from size-4 vector to size-2-by-2 matrix.

    const auto vec = loadIfGLValue(subExpr);

    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    const bool isMat =
        TypeTranslator::isMxNMatrix(toType, &elemType, &rowCount, &colCount);

    assert(isMat && rowCount == 2 && colCount == 2);
    (void)isMat;

    uint32_t vec2Type =
        theBuilder.getVecType(typeTranslator.translateType(elemType), 2);
    const auto subVec1 =
        theBuilder.createVectorShuffle(vec2Type, vec, vec, {0, 1});
    const auto subVec2 =
        theBuilder.createVectorShuffle(vec2Type, vec, vec, {2, 3});

    const auto mat = theBuilder.createCompositeConstruct(
        theBuilder.getMatType(elemType, vec2Type, 2), {subVec1, subVec2});

    return SpirvEvalInfo(mat).setRValue();
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

    if (vecSplat.isConstant()) {
      const auto valueId = theBuilder.getConstantComposite(matType, vectors);
      return SpirvEvalInfo(valueId).setConstant().setRValue();
    } else {
      const auto valueId =
          theBuilder.createCompositeConstruct(matType, vectors);
      return SpirvEvalInfo(valueId).setRValue();
    }
  }
  case CastKind::CK_HLSLMatrixTruncationCast: {
    const QualType srcType = subExprType;
    const uint32_t srcId = doExpr(subExpr);
    const QualType elemType = hlsl::GetHLSLMatElementType(srcType);
    const uint32_t dstTypeId = typeTranslator.translateType(toType);
    llvm::SmallVector<uint32_t, 4> indexes;

    // It is possible that the source matrix is in fact a vector.
    // For example: Truncate float1x3 --> float1x2.
    // The front-end disallows float1x3 --> float2x1.
    {
      uint32_t srcVecSize = 0, dstVecSize = 0;
      if (TypeTranslator::isVectorType(srcType, nullptr, &srcVecSize) &&
          TypeTranslator::isVectorType(toType, nullptr, &dstVecSize)) {
        for (uint32_t i = 0; i < dstVecSize; ++i)
          indexes.push_back(i);
        const auto valId =
            theBuilder.createVectorShuffle(dstTypeId, srcId, srcId, indexes);
        return SpirvEvalInfo(valId).setRValue();
      }
    }

    uint32_t srcRows = 0, srcCols = 0, dstRows = 0, dstCols = 0;
    hlsl::GetHLSLMatRowColCount(srcType, srcRows, srcCols);
    hlsl::GetHLSLMatRowColCount(toType, dstRows, dstCols);
    const uint32_t elemTypeId = typeTranslator.translateType(elemType);
    const uint32_t srcRowType = theBuilder.getVecType(elemTypeId, srcCols);

    // Indexes to pass to OpVectorShuffle
    for (uint32_t i = 0; i < dstCols; ++i)
      indexes.push_back(i);

    llvm::SmallVector<uint32_t, 4> extractedVecs;
    for (uint32_t row = 0; row < dstRows; ++row) {
      // Extract a row
      uint32_t rowId =
          theBuilder.createCompositeExtract(srcRowType, srcId, {row});
      // Extract the necessary columns from that row.
      // The front-end ensures dstCols <= srcCols.
      // If dstCols equals srcCols, we can use the whole row directly.
      if (dstCols == 1) {
        rowId = theBuilder.createCompositeExtract(elemTypeId, rowId, {0});
      } else if (dstCols < srcCols) {
        rowId = theBuilder.createVectorShuffle(
            theBuilder.getVecType(elemTypeId, dstCols), rowId, rowId, indexes);
      }
      extractedVecs.push_back(rowId);
    }

    uint32_t valId = extractedVecs.front();
    if (extractedVecs.size() > 1) {
      valId = theBuilder.createCompositeConstruct(
          typeTranslator.translateType(toType), extractedVecs);
    }
    return SpirvEvalInfo(valId).setRValue();
  }
  case CastKind::CK_HLSLMatrixToScalarCast: {
    // The underlying should already be a matrix of 1x1.
    assert(TypeTranslator::is1x1Matrix(subExprType));
    return doExpr(subExpr);
  }
  case CastKind::CK_HLSLMatrixToVectorCast: {
    // The underlying should already be a matrix of 1xN.
    assert(TypeTranslator::is1xNMatrix(subExprType) ||
           TypeTranslator::isMx1Matrix(subExprType));
    return doExpr(subExpr);
  }
  case CastKind::CK_FunctionToPointerDecay:
    // Just need to return the function id
    return doExpr(subExpr);
  case CastKind::CK_FlatConversion: {
    uint32_t subExprId = 0;
    QualType evalType = subExprType;

    // Optimization: we can use OpConstantNull for cases where we want to
    // initialize an entire data structure to zeros.
    if (evaluatesToConstZero(subExpr, astContext)) {
      subExprId =
          theBuilder.getConstantNull(typeTranslator.translateType(toType));
      return SpirvEvalInfo(subExprId).setRValue().setConstant();
    }

    TypeTranslator::LiteralTypeHint hint(typeTranslator);
    // Try to evaluate float literals as float rather than double.
    if (const auto *floatLiteral = dyn_cast<FloatingLiteral>(subExpr)) {
      subExprId = tryToEvaluateAsFloat32(floatLiteral->getValue());
      if (subExprId)
        evalType = astContext.FloatTy;
    }
    // Evaluate 'literal float' initializer type as float rather than double.
    // TODO: This could result in rounding error if the initializer is a
    // non-literal expression that requires larger than 32 bits and has the
    // 'literal float' type.
    else if (subExprType->isSpecificBuiltinType(BuiltinType::LitFloat)) {
      evalType = astContext.FloatTy;
      hint.setHint(astContext.FloatTy);
    }
    // Try to evaluate integer literals as 32-bit int rather than 64-bit int.
    else if (const auto *intLiteral = dyn_cast<IntegerLiteral>(subExpr)) {
      const bool isSigned = subExprType->isSignedIntegerType();
      subExprId = tryToEvaluateAsInt32(intLiteral->getValue(), isSigned);
      if (subExprId)
        evalType = isSigned ? astContext.IntTy : astContext.UnsignedIntTy;
    }
    // For assigning one array instance to another one with the same array type
    // (regardless of constness and literalness), the rhs will be wrapped in a
    // FlatConversion:
    //  |- <lhs>
    //  `- ImplicitCastExpr <FlatConversion>
    //     `- ImplicitCastExpr <LValueToRValue>
    //        `- <rhs>
    // This FlatConversion does not affect CodeGen, so that we can ignore it.
    else if (subExprType->isArrayType() &&
             typeTranslator.isSameType(expr->getType(), subExprType)) {
      return doExpr(subExpr);
    }

    if (!subExprId)
      subExprId = doExpr(subExpr);
    const auto valId =
        processFlatConversion(toType, evalType, subExprId, expr->getExprLoc());
    return SpirvEvalInfo(valId).setRValue();
  }
  case CastKind::CK_UncheckedDerivedToBase:
  case CastKind::CK_HLSLDerivedToBase: {
    // Find the index sequence of the base to which we are casting
    llvm::SmallVector<uint32_t, 4> baseIndices;
    getBaseClassIndices(expr, &baseIndices);

    // Turn them in to SPIR-V constants
    for (uint32_t i = 0; i < baseIndices.size(); ++i)
      baseIndices[i] = theBuilder.getConstantUint32(baseIndices[i]);

    auto derivedInfo = doExpr(subExpr);
    return turnIntoElementPtr(derivedInfo, expr->getType(), baseIndices);
  }
  default:
    emitError("implicit cast kind '%0' unimplemented", expr->getExprLoc())
        << expr->getCastKindName() << expr->getSourceRange();
    expr->dump();
    return 0;
  }
}

uint32_t SPIRVEmitter::processFlatConversion(const QualType type,
                                             const QualType initType,
                                             const uint32_t initId,
                                             SourceLocation srcLoc) {
  // Try to translate the canonical type first
  const auto canonicalType = type.getCanonicalType();
  if (canonicalType != type)
    return processFlatConversion(canonicalType, initType, initId, srcLoc);

  // Primitive types
  {
    QualType ty = {};
    if (TypeTranslator::isScalarType(type, &ty)) {
      if (const auto *builtinType = ty->getAs<BuiltinType>()) {
        switch (builtinType->getKind()) {
        case BuiltinType::Void: {
          emitError("cannot create a constant of void type", srcLoc);
          return 0;
        }
        case BuiltinType::Bool:
          return castToBool(initId, initType, ty);
        // Target type is an integer variant.
        case BuiltinType::Int:
        case BuiltinType::Short:
        case BuiltinType::Min12Int:
        case BuiltinType::UShort:
        case BuiltinType::UInt:
        case BuiltinType::Long:
        case BuiltinType::LongLong:
        case BuiltinType::ULong:
        case BuiltinType::ULongLong:
          return castToInt(initId, initType, ty, srcLoc);
        // Target type is a float variant.
        case BuiltinType::Double:
        case BuiltinType::Float:
        case BuiltinType::Half:
        case BuiltinType::Min10Float:
          return castToFloat(initId, initType, ty, srcLoc);
        default:
          emitError("flat conversion of type %0 unimplemented", srcLoc)
              << builtinType->getTypeClassName();
          return 0;
        }
      }
    }
  }
  // Vector types
  {
    QualType elemType = {};
    uint32_t elemCount = {};
    if (TypeTranslator::isVectorType(type, &elemType, &elemCount)) {
      const uint32_t elemId =
          processFlatConversion(elemType, initType, initId, srcLoc);
      llvm::SmallVector<uint32_t, 4> constituents(size_t(elemCount), elemId);
      return theBuilder.createCompositeConstruct(
          typeTranslator.translateType(type), constituents);
    }
  }

  // Matrix types
  {
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (TypeTranslator::isMxNMatrix(type, &elemType, &rowCount, &colCount)) {
      // By default HLSL matrices are row major, while SPIR-V matrices are
      // column major. We are mapping what HLSL semantically mean a row into a
      // column here.
      const uint32_t vecType = theBuilder.getVecType(
          typeTranslator.translateType(elemType), colCount);
      const uint32_t elemId =
          processFlatConversion(elemType, initType, initId, srcLoc);
      const llvm::SmallVector<uint32_t, 4> constituents(size_t(colCount),
                                                        elemId);
      const uint32_t colId =
          theBuilder.createCompositeConstruct(vecType, constituents);
      const llvm::SmallVector<uint32_t, 4> rows(size_t(rowCount), colId);
      return theBuilder.createCompositeConstruct(
          typeTranslator.translateType(type), rows);
    }
  }

  // Struct type
  if (const auto *structType = type->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();
    llvm::SmallVector<uint32_t, 4> fields;

    for (const auto *field : decl->fields()) {
      // There is a special case for FlatConversion. If T is a struct with only
      // one member, S, then (T)<an-instance-of-S> is allowed, which essentially
      // constructs a new T instance using the instance of S as its only member.
      // Check whether we are handling that case here first.
      if (field->getType().getCanonicalType() == initType.getCanonicalType()) {
        fields.push_back(initId);
      } else {
        fields.push_back(
            processFlatConversion(field->getType(), initType, initId, srcLoc));
      }
    }

    return theBuilder.createCompositeConstruct(
        typeTranslator.translateType(type), fields);
  }

  // Array type
  if (const auto *arrayType = astContext.getAsConstantArrayType(type)) {
    const auto size =
        static_cast<uint32_t>(arrayType->getSize().getZExtValue());
    const uint32_t elemId = processFlatConversion(arrayType->getElementType(),
                                                  initType, initId, srcLoc);
    llvm::SmallVector<uint32_t, 4> constituents(size_t(size), elemId);
    return theBuilder.createCompositeConstruct(
        typeTranslator.translateType(type), constituents);
  }

  emitError("flat conversion of type %0 unimplemented", {})
      << type->getTypeClassName();
  type->dump();
  return 0;
}

SpirvEvalInfo
SPIRVEmitter::doCompoundAssignOperator(const CompoundAssignOperator *expr) {
  const auto opcode = expr->getOpcode();

  // Try to optimize floatMxN *= float and floatN *= float case
  if (opcode == BO_MulAssign) {
    if (SpirvEvalInfo result = tryToGenFloatMatrixScale(expr))
      return result;
    if (SpirvEvalInfo result = tryToGenFloatVectorScale(expr))
      return result;
  }

  const auto *rhs = expr->getRHS();
  const auto *lhs = expr->getLHS();

  SpirvEvalInfo lhsPtr = 0;
  const auto result =
      processBinaryOp(lhs, rhs, opcode, expr->getComputationLHSType(),
                      expr->getType(), expr->getSourceRange(), &lhsPtr);
  return processAssignment(lhs, result, true, lhsPtr);
}

SpirvEvalInfo
SPIRVEmitter::doConditionalOperator(const ConditionalOperator *expr) {
  const auto type = expr->getType();

  // Enhancement for special case when the ConditionalOperator return type is a
  // literal type. For example:
  //
  // float a = cond ? 1 : 2;
  // int   b = cond ? 1.5 : 2.5;
  //
  // There will be no indications about whether '1' and '2' should be used as
  // 32-bit or 64-bit integers. Similarly, there will be no indication about
  // whether '1.5' and '2.5' should be used as 32-bit or 64-bit floats.
  //
  // We want to avoid using 64-bit int and 64-bit float as much as possible.
  //
  // Note that if the literal is in fact large enough that it can't be
  // represented in 32 bits (e.g. integer larger than 3e+9), we should *not*
  // provide a hint.

  TypeTranslator::LiteralTypeHint hint(typeTranslator);
  const bool isLitInt = type->isSpecificBuiltinType(BuiltinType::LitInt);
  const bool isLitFloat = type->isSpecificBuiltinType(BuiltinType::LitFloat);
  // Return type of ConditionalOperator is a 'literal int' or 'literal float'
  if (isLitInt || isLitFloat) {
    // There is no hint about the intended usage of the literal type.
    if (typeTranslator.getIntendedLiteralType(type) == type) {
      // If either branch is a literal that is larger than 32-bits, do not
      // provide a hint.
      if (!isLiteralLargerThan32Bits(expr->getTrueExpr()) &&
          !isLiteralLargerThan32Bits(expr->getFalseExpr())) {
        if (isLitInt)
          hint.setHint(astContext.IntTy);
        else if (isLitFloat)
          hint.setHint(astContext.FloatTy);
      }
    }
  }

  // According to HLSL doc, all sides of the ?: expression are always
  // evaluated.
  const uint32_t typeId = typeTranslator.translateType(type);

  // If we are selecting between two SampleState objects, none of the three
  // operands has a LValueToRValue implicit cast.
  uint32_t condition = loadIfGLValue(expr->getCond());
  const auto trueBranch = loadIfGLValue(expr->getTrueExpr());
  const auto falseBranch = loadIfGLValue(expr->getFalseExpr());

  // For cases where the return type is a scalar or a vector, we can use
  // OpSelect to choose between the two. OpSelect's return type must be either
  // scalar or vector.
  if (TypeTranslator::isScalarType(type) ||
      TypeTranslator::isVectorType(type)) {
    // The SPIR-V OpSelect instruction must have a selection argument that is
    // the same size as the return type. If the return type is a vector, the
    // selection must be a vector of booleans (one per output component).
    uint32_t count = 0;
    if (TypeTranslator::isVectorType(expr->getType(), nullptr, &count) &&
        !TypeTranslator::isVectorType(expr->getCond()->getType())) {
      const uint32_t condVecType =
          theBuilder.getVecType(theBuilder.getBoolType(), count);
      const llvm::SmallVector<uint32_t, 4> components(size_t(count), condition);
      condition = theBuilder.createCompositeConstruct(condVecType, components);
    }

    auto valueId =
        theBuilder.createSelect(typeId, condition, trueBranch, falseBranch);
    return SpirvEvalInfo(valueId).setRValue();
  }

  // If we can't use OpSelect, we need to create if-else control flow.
  const uint32_t tempVar = theBuilder.addFnVar(typeId, "temp.var.ternary");
  const uint32_t thenBB = theBuilder.createBasicBlock("if.true");
  const uint32_t mergeBB = theBuilder.createBasicBlock("if.merge");
  const uint32_t elseBB = theBuilder.createBasicBlock("if.false");

  // Create the branch instruction. This will end the current basic block.
  theBuilder.createConditionalBranch(condition, thenBB, elseBB, mergeBB);
  theBuilder.addSuccessor(thenBB);
  theBuilder.addSuccessor(elseBB);
  theBuilder.setMergeTarget(mergeBB);
  // Handle the then branch
  theBuilder.setInsertPoint(thenBB);
  theBuilder.createStore(tempVar, trueBranch);
  theBuilder.createBranch(mergeBB);
  theBuilder.addSuccessor(mergeBB);
  // Handle the else branch
  theBuilder.setInsertPoint(elseBB);
  theBuilder.createStore(tempVar, falseBranch);
  theBuilder.createBranch(mergeBB);
  theBuilder.addSuccessor(mergeBB);
  // From now on, emit instructions into the merge block.
  theBuilder.setInsertPoint(mergeBB);
  return SpirvEvalInfo(theBuilder.createLoad(typeId, tempVar)).setRValue();
}

uint32_t SPIRVEmitter::processByteAddressBufferStructuredBufferGetDimensions(
    const CXXMemberCallExpr *expr) {
  const auto *object = expr->getImplicitObjectArgument();
  const auto objectId = loadIfAliasVarRef(object);
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
        type, spirvOptions.sBufferLayoutRule, &stride);
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
  const auto objectInfo = loadIfAliasVarRef(object);

  const auto uintType = theBuilder.getUint32Type();
  const uint32_t zero = theBuilder.getConstantUint32(0);

  const uint32_t offset = doExpr(expr->getArg(0));
  // Right shift by 2 to convert the byte offset to uint32_t offset
  const uint32_t address =
      theBuilder.createBinaryOp(spv::Op::OpShiftRightLogical, uintType, offset,
                                theBuilder.getConstantUint32(2));
  const auto ptrType =
      theBuilder.getPointerType(uintType, objectInfo.getStorageClass());
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

uint32_t SPIRVEmitter::processGetSamplePosition(const CXXMemberCallExpr *expr) {
  const auto *object = expr->getImplicitObjectArgument()->IgnoreParens();
  const auto sampleCount = theBuilder.createUnaryOp(
      spv::Op::OpImageQuerySamples, theBuilder.getUint32Type(),
      loadIfGLValue(object));
  emitWarning(
      "GetSamplePosition only supports standard sample settings with 1, 2, 4, "
      "8, or 16 samples and will return float2(0, 0) for other cases",
      expr->getCallee()->getExprLoc());
  return emitGetSamplePosition(sampleCount, doExpr(expr->getArg(0)));
}

SpirvEvalInfo SPIRVEmitter::processSubpassLoad(const CXXMemberCallExpr *expr) {
  const auto *object = expr->getImplicitObjectArgument()->IgnoreParens();
  const uint32_t sample = expr->getNumArgs() == 1
                              ? static_cast<uint32_t>(doExpr(expr->getArg(0)))
                              : 0;
  const uint32_t zero = theBuilder.getConstantInt32(0);
  const uint32_t location = theBuilder.getConstantComposite(
      theBuilder.getVecType(theBuilder.getInt32Type(), 2), {zero, zero});

  return processBufferTextureLoad(object, location, /*constOffset*/ 0,
                                  /*varOffset*/ 0, /*lod*/ sample,
                                  /*residencyCode*/ 0);
}

uint32_t
SPIRVEmitter::processBufferTextureGetDimensions(const CXXMemberCallExpr *expr) {
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

  // For TextureCube, arguments are either:
  // a) width, height
  // b) MipLevel, width, height, NumLevels

  // For TextureCubeArray, arguments are either:
  // a) width, height, elements
  // b) MipLevel, width, height, elements, NumLevels

  // Note: SPIR-V Spec requires return type of OpImageQuerySize(Lod) to be a
  // scalar/vector of integers. SPIR-V Spec also requires return type of
  // OpImageQueryLevels and OpImageQuerySamples to be scalar integers.
  // The HLSL methods, however, have overloaded functions which have float
  // output arguments. Since the AST naturally won't have casting AST nodes for
  // such cases, we'll have to perform the cast ourselves.
  const auto storeToOutputArg = [this](const Expr *outputArg, uint32_t id,
                                       QualType type) {
    id = castToType(id, type, outputArg->getType(), outputArg->getExprLoc());
    theBuilder.createStore(doExpr(outputArg), id);
  };

  if ((typeName == "Texture1D" && numArgs > 1) ||
      (typeName == "Texture2D" && numArgs > 2) ||
      (typeName == "TextureCube" && numArgs > 2) ||
      (typeName == "Texture3D" && numArgs > 3) ||
      (typeName == "Texture1DArray" && numArgs > 2) ||
      (typeName == "TextureCubeArray" && numArgs > 3) ||
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
  const QualType resultQualType =
      querySize == 1
          ? astContext.UnsignedIntTy
          : astContext.getExtVectorType(astContext.UnsignedIntTy, querySize);
  const uint32_t resultTypeId = typeTranslator.translateType(resultQualType);

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
      lod ? theBuilder.createBinaryOp(spv::Op::OpImageQuerySizeLod,
                                      resultTypeId, objectId, lod)
          : theBuilder.createUnaryOp(spv::Op::OpImageQuerySize, resultTypeId,
                                     objectId);

  if (querySize == 1) {
    const uint32_t argIndex = mipLevel ? 1 : 0;
    storeToOutputArg(expr->getArg(argIndex), query, resultQualType);
  } else {
    for (uint32_t i = 0; i < querySize; ++i) {
      const uint32_t component =
          theBuilder.createCompositeExtract(uintId, query, {i});
      // If the first arg is the mipmap level, we must write the results
      // starting from Arg(i+1), not Arg(i).
      const uint32_t argIndex = mipLevel ? i + 1 : i;
      storeToOutputArg(expr->getArg(argIndex), component,
                       astContext.UnsignedIntTy);
    }
  }

  if (numLevels || numSamples) {
    const Expr *numLevelsSamplesArg = numLevels ? numLevels : numSamples;
    const spv::Op opcode =
        numLevels ? spv::Op::OpImageQueryLevels : spv::Op::OpImageQuerySamples;
    const uint32_t numLevelsSamplesQuery =
        theBuilder.createUnaryOp(opcode, uintId, objectId);
    storeToOutputArg(numLevelsSamplesArg, numLevelsSamplesQuery,
                     astContext.UnsignedIntTy);
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
  const auto *object = expr->getImplicitObjectArgument();
  const auto objectInfo = loadIfGLValue(object);
  const auto samplerState = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  const uint32_t sampledImageType = theBuilder.getSampledImageType(
      typeTranslator.translateType(object->getType()));
  const uint32_t sampledImage = theBuilder.createBinaryOp(
      spv::Op::OpSampledImage, sampledImageType, objectInfo, samplerState);

  if (objectInfo.isNonUniform() || samplerState.isNonUniform()) {
    // The sampled image will be used to access resource's memory, so we need
    // to decorate it with NonUniformEXT.
    theBuilder.decorateNonUniformEXT(sampledImage);
  }

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
  // An additional 'out uint status' parameter can appear in both of the above.
  //
  // Parameters for .GatherCmp{Red|Green|Blue|Alpha}() are one of the following
  // two sets:
  // * SamplerState s, float2 location, float compare_value, int2 offset
  // * SamplerState s, float2 location, float compare_value, int2 offset1,
  //   int2 offset2, int2 offset3, int2 offset4
  //
  // An additional 'out uint status' parameter can appear in both of the above.
  //
  // TextureCube's signature is somewhat different from the rest.
  // Parameters for .Gather{Red|Green|Blue|Alpha}() for TextureCube are:
  // * SamplerState s, float2 location, out uint status
  // Parameters for .GatherCmp{Red|Green|Blue|Alpha}() for TextureCube are:
  // * SamplerState s, float2 location, float compare_value, out uint status
  //
  // Return type is always a 4-component vector.
  const FunctionDecl *callee = expr->getDirectCallee();
  const auto numArgs = expr->getNumArgs();
  const auto *imageExpr = expr->getImplicitObjectArgument();
  const QualType imageType = imageExpr->getType();
  const auto imageTypeId = typeTranslator.translateType(imageType);
  const auto retTypeId = typeTranslator.translateType(callee->getReturnType());

  // If the last arg is an unsigned integer, it must be the status.
  const bool hasStatusArg =
      expr->getArg(numArgs - 1)->getType()->isUnsignedIntegerType();

  // Subtract 1 for status arg (if it exists), subtract 1 for compare_value (if
  // it exists), and subtract 2 for SamplerState and location.
  const auto numOffsetArgs = numArgs - hasStatusArg - isCmp - 2;
  // No offset args for TextureCube, 1 or 4 offset args for the rest.
  assert(numOffsetArgs == 0 || numOffsetArgs == 1 || numOffsetArgs == 4);

  const auto image = loadIfGLValue(imageExpr);
  const auto sampler = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  const uint32_t compareVal =
      isCmp ? static_cast<uint32_t>(doExpr(expr->getArg(2))) : 0;

  // Handle offsets (if any).
  bool needsEmulation = false;
  uint32_t constOffset = 0, varOffset = 0, constOffsets = 0;
  if (numOffsetArgs == 1) {
    // The offset arg is not optional.
    handleOffsetInMethodCall(expr, 2 + isCmp, &constOffset, &varOffset);
  } else if (numOffsetArgs == 4) {
    const auto offset0 = tryToEvaluateAsConst(expr->getArg(2 + isCmp));
    const auto offset1 = tryToEvaluateAsConst(expr->getArg(3 + isCmp));
    const auto offset2 = tryToEvaluateAsConst(expr->getArg(4 + isCmp));
    const auto offset3 = tryToEvaluateAsConst(expr->getArg(5 + isCmp));

    // If any of the offsets is not constant, we then need to emulate the call
    // using 4 OpImageGather instructions. Otherwise, we can leverage the
    // ConstOffsets image operand.
    if (offset0 && offset1 && offset2 && offset3) {
      const uint32_t v2i32 =
          theBuilder.getVecType(theBuilder.getInt32Type(), 2);
      const uint32_t offsetType =
          theBuilder.getArrayType(v2i32, theBuilder.getConstantUint32(4));
      constOffsets = theBuilder.getConstantComposite(
          offsetType, {offset0, offset1, offset2, offset3});
    } else {
      needsEmulation = true;
    }
  }

  const auto status =
      hasStatusArg ? static_cast<uint32_t>(doExpr(expr->getArg(numArgs - 1)))
                   : 0;
  const bool isNonUniform = image.isNonUniform() || sampler.isNonUniform();

  if (needsEmulation) {
    const auto elemType = typeTranslator.translateType(
        hlsl::GetHLSLVecElementType(callee->getReturnType()));

    uint32_t texels[4];
    for (uint32_t i = 0; i < 4; ++i) {
      varOffset = doExpr(expr->getArg(2 + isCmp + i));
      const uint32_t gatherRet = theBuilder.createImageGather(
          retTypeId, imageTypeId, image, sampler, isNonUniform, coordinate,
          theBuilder.getConstantInt32(component), compareVal, /*constOffset*/ 0,
          varOffset, /*constOffsets*/ 0, /*sampleNumber*/ 0, status);
      texels[i] = theBuilder.createCompositeExtract(elemType, gatherRet, {i});
    }
    return theBuilder.createCompositeConstruct(
        retTypeId, {texels[0], texels[1], texels[2], texels[3]});
  }

  return theBuilder.createImageGather(
      retTypeId, imageTypeId, image, sampler, isNonUniform, coordinate,
      theBuilder.getConstantInt32(component), compareVal, constOffset,
      varOffset, constOffsets, /*sampleNumber*/ 0, status);
}

uint32_t SPIRVEmitter::processTextureGatherCmp(const CXXMemberCallExpr *expr) {
  // Signature for Texture2D/Texture2DArray:
  //
  // float4 GatherCmp(
  //   in SamplerComparisonState s,
  //   in float2 location,
  //   in float compare_value
  //   [,in int2 offset]
  //   [,out uint Status]
  // );
  //
  // Signature for TextureCube/TextureCubeArray:
  //
  // float4 GatherCmp(
  //   in SamplerComparisonState s,
  //   in float2 location,
  //   in float compare_value,
  //   out uint Status
  // );
  //
  // Other Texture types do not have the GatherCmp method.

  const FunctionDecl *callee = expr->getDirectCallee();
  const auto numArgs = expr->getNumArgs();
  const bool hasStatusArg =
      expr->getArg(numArgs - 1)->getType()->isUnsignedIntegerType();
  const bool hasOffsetArg = (numArgs == 5) || (numArgs == 4 && !hasStatusArg);

  const auto *imageExpr = expr->getImplicitObjectArgument();
  const auto image = loadIfGLValue(imageExpr);
  const auto sampler = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  const uint32_t comparator = doExpr(expr->getArg(2));
  uint32_t constOffset = 0, varOffset = 0;
  if (hasOffsetArg)
    handleOffsetInMethodCall(expr, 3, &constOffset, &varOffset);

  const auto retType = typeTranslator.translateType(callee->getReturnType());
  const auto imageType = typeTranslator.translateType(imageExpr->getType());
  const auto status =
      hasStatusArg ? static_cast<uint32_t>(doExpr(expr->getArg(numArgs - 1)))
                   : 0;

  return theBuilder.createImageGather(
      retType, imageType, image, sampler,
      image.isNonUniform() || sampler.isNonUniform(), coordinate,
      /*component*/ 0, comparator, constOffset, varOffset, /*constOffsets*/ 0,
      /*sampleNumber*/ 0, status);
}

SpirvEvalInfo SPIRVEmitter::processBufferTextureLoad(
    const Expr *object, const uint32_t locationId, uint32_t constOffset,
    uint32_t varOffset, uint32_t lod, uint32_t residencyCode) {
  // Loading for Buffer and RWBuffer translates to an OpImageFetch.
  // The result type of an OpImageFetch must be a vec4 of float or int.
  const auto type = object->getType();
  assert(TypeTranslator::isBuffer(type) || TypeTranslator::isRWBuffer(type) ||
         TypeTranslator::isTexture(type) || TypeTranslator::isRWTexture(type) ||
         TypeTranslator::isSubpassInput(type) ||
         TypeTranslator::isSubpassInputMS(type));

  const bool doFetch =
      TypeTranslator::isBuffer(type) || TypeTranslator::isTexture(type);

  const auto objectInfo = loadIfGLValue(object);

  if (objectInfo.isNonUniform()) {
    // Decoreate the image handle for OpImageFetch/OpImageRead
    theBuilder.decorateNonUniformEXT(objectInfo);
  }

  // For Texture2DMS and Texture2DMSArray, Sample must be used rather than Lod.
  uint32_t sampleNumber = 0;
  if (TypeTranslator::isTextureMS(type) ||
      TypeTranslator::isSubpassInputMS(type)) {
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
    emitError("buffer/texture type unimplemented", object->getExprLoc());
    return 0;
  }

  // OpImageFetch and OpImageRead can only fetch a vector of 4 elements.
  const uint32_t texelTypeId = theBuilder.getVecType(elemTypeId, 4u);
  const uint32_t texel = theBuilder.createImageFetchOrRead(
      doFetch, texelTypeId, type, objectInfo, locationId, lod, constOffset,
      varOffset, /*constOffsets*/ 0, sampleNumber, residencyCode);

  // If the result type is a vec1, vec2, or vec3, some extra processing
  // (extraction) is required.
  uint32_t retVal = extractVecFromVec4(texel, elemCount, elemTypeId);
  return SpirvEvalInfo(retVal).setRValue();
}

SpirvEvalInfo SPIRVEmitter::processByteAddressBufferLoadStore(
    const CXXMemberCallExpr *expr, uint32_t numWords, bool doStore) {
  uint32_t resultId = 0;
  const auto object = expr->getImplicitObjectArgument();
  const auto objectInfo = loadIfAliasVarRef(object);
  assert(numWords >= 1 && numWords <= 4);
  if (doStore) {
    assert(typeTranslator.isRWByteAddressBuffer(object->getType()));
    assert(expr->getNumArgs() == 2);
  } else {
    assert(typeTranslator.isRWByteAddressBuffer(object->getType()) ||
           typeTranslator.isByteAddressBuffer(object->getType()));
    if (expr->getNumArgs() == 2) {
      emitError(
          "(RW)ByteAddressBuffer::Load(in address, out status) not supported",
          expr->getExprLoc());
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
      theBuilder.getPointerType(uintTypeId, objectInfo.getStorageClass());
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
  return SpirvEvalInfo(resultId).setRValue();
}

SpirvEvalInfo
SPIRVEmitter::processStructuredBufferLoad(const CXXMemberCallExpr *expr) {
  if (expr->getNumArgs() == 2) {
    emitError(
        "(RW)StructuredBuffer::Load(in location, out status) not supported",
        expr->getExprLoc());
    return 0;
  }

  const auto *buffer = expr->getImplicitObjectArgument();
  auto info = loadIfAliasVarRef(buffer);

  const QualType structType =
      hlsl::GetHLSLResourceResultType(buffer->getType());

  const uint32_t zero = theBuilder.getConstantInt32(0);
  const uint32_t index = doExpr(expr->getArg(0));

  return turnIntoElementPtr(info, structType, {zero, index});
}

uint32_t SPIRVEmitter::incDecRWACSBufferCounter(const CXXMemberCallExpr *expr,
                                                bool isInc, bool loadObject) {
  const uint32_t i32Type = theBuilder.getInt32Type();
  const uint32_t one = theBuilder.getConstantUint32(1);  // As scope: Device
  const uint32_t zero = theBuilder.getConstantUint32(0); // As memory sema: None
  const uint32_t sOne = theBuilder.getConstantInt32(1);

  const auto *object =
      expr->getImplicitObjectArgument()->IgnoreParenNoopCasts(astContext);

  if (loadObject) {
    // We don't need the object's <result-id> here since counter variable is a
    // separate variable. But we still need the side effects of evaluating the
    // object, e.g., if the source code is foo(...).IncrementCounter(), we still
    // want to emit the code for foo(...).
    (void)doExpr(object);
  }

  const auto *counterPair = getFinalACSBufferCounter(object);
  if (!counterPair) {
    emitFatalError("cannot find the associated counter variable",
                   object->getExprLoc());
    return 0;
  }

  const uint32_t counterPtrType = theBuilder.getPointerType(
      theBuilder.getInt32Type(), spv::StorageClass::Uniform);
  const uint32_t counterPtr = theBuilder.createAccessChain(
      counterPtrType, counterPair->get(theBuilder, typeTranslator), {zero});

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

bool SPIRVEmitter::tryToAssignCounterVar(const DeclaratorDecl *dstDecl,
                                         const Expr *srcExpr) {
  // We are handling associated counters here. Casts should not alter which
  // associated counter to manipulate.
  srcExpr = srcExpr->IgnoreParenCasts();

  // For parameters of forward-declared functions. We must make sure the
  // associated counter variable is created. But for forward-declared functions,
  // the translation of the real definition may not be started yet.
  if (const auto *param = dyn_cast<ParmVarDecl>(dstDecl))
    declIdMapper.createFnParamCounterVar(param);
  // For implicit objects of methods. Similar to the above.
  else if (const auto *thisObject = dyn_cast<ImplicitParamDecl>(dstDecl))
    declIdMapper.createFnParamCounterVar(thisObject);

  // Handle AssocCounter#1 (see CounterVarFields comment)
  if (const auto *dstPair = declIdMapper.getCounterIdAliasPair(dstDecl)) {
    const auto *srcPair = getFinalACSBufferCounter(srcExpr);
    if (!srcPair) {
      emitFatalError("cannot find the associated counter variable",
                     srcExpr->getExprLoc());
      return false;
    }
    dstPair->assign(*srcPair, theBuilder, typeTranslator);
    return true;
  }

  // Handle AssocCounter#3
  llvm::SmallVector<uint32_t, 4> srcIndices;
  const auto *dstFields = declIdMapper.getCounterVarFields(dstDecl);
  const auto *srcFields = getIntermediateACSBufferCounter(srcExpr, &srcIndices);

  if (dstFields && srcFields) {
    if (!dstFields->assign(*srcFields, theBuilder, typeTranslator)) {
      emitFatalError("cannot handle associated counter variable assignment",
                     srcExpr->getExprLoc());
      return false;
    }
    return true;
  }

  // AssocCounter#2 and AssocCounter#4 for the lhs cannot happen since the lhs
  // is a stand-alone decl in this method.

  return false;
}

bool SPIRVEmitter::tryToAssignCounterVar(const Expr *dstExpr,
                                         const Expr *srcExpr) {
  dstExpr = dstExpr->IgnoreParenCasts();
  srcExpr = srcExpr->IgnoreParenCasts();

  const auto *dstPair = getFinalACSBufferCounter(dstExpr);
  const auto *srcPair = getFinalACSBufferCounter(srcExpr);

  if ((dstPair == nullptr) != (srcPair == nullptr)) {
    emitFatalError("cannot handle associated counter variable assignment",
                   srcExpr->getExprLoc());
    return false;
  }

  // Handle AssocCounter#1 & AssocCounter#2
  if (dstPair && srcPair) {
    dstPair->assign(*srcPair, theBuilder, typeTranslator);
    return true;
  }

  // Handle AssocCounter#3 & AssocCounter#4
  llvm::SmallVector<uint32_t, 4> dstIndices;
  llvm::SmallVector<uint32_t, 4> srcIndices;
  const auto *srcFields = getIntermediateACSBufferCounter(srcExpr, &srcIndices);
  const auto *dstFields = getIntermediateACSBufferCounter(dstExpr, &dstIndices);

  if (dstFields && srcFields) {
    return dstFields->assign(*srcFields, dstIndices, srcIndices, theBuilder,
                             typeTranslator);
  }

  return false;
}

const CounterIdAliasPair *
SPIRVEmitter::getFinalACSBufferCounter(const Expr *expr) {
  // AssocCounter#1: referencing some stand-alone variable
  if (const auto *decl = getReferencedDef(expr))
    return declIdMapper.getCounterIdAliasPair(decl);

  // AssocCounter#2: referencing some non-struct field
  llvm::SmallVector<uint32_t, 4> indices;

  const auto *base =
      collectArrayStructIndices(expr, &indices, /*rawIndex=*/true);
  const auto *decl =
      (base && isa<CXXThisExpr>(base))
          ? getOrCreateDeclForMethodObject(cast<CXXMethodDecl>(curFunction))
          : getReferencedDef(base);
  return declIdMapper.getCounterIdAliasPair(decl, &indices);
}

const CounterVarFields *SPIRVEmitter::getIntermediateACSBufferCounter(
    const Expr *expr, llvm::SmallVector<uint32_t, 4> *indices) {
  const auto *base =
      collectArrayStructIndices(expr, indices, /*rawIndex=*/true);
  const auto *decl =
      (base && isa<CXXThisExpr>(base))
          // Use the decl we created to represent the implicit object
          ? getOrCreateDeclForMethodObject(cast<CXXMethodDecl>(curFunction))
          // Find the referenced decl from the original source code
          : getReferencedDef(base);

  return declIdMapper.getCounterVarFields(decl);
}

const ImplicitParamDecl *
SPIRVEmitter::getOrCreateDeclForMethodObject(const CXXMethodDecl *method) {
  const auto found = thisDecls.find(method);
  if (found != thisDecls.end())
    return found->second;

  const std::string name = method->getName().str() + ".this";
  // Create a new identifier to convey the name
  auto &identifier = astContext.Idents.get(name);

  return thisDecls[method] = ImplicitParamDecl::Create(
             astContext, /*DC=*/nullptr, SourceLocation(), &identifier,
             method->getThisType(astContext)->getPointeeType());
}

SpirvEvalInfo
SPIRVEmitter::processACSBufferAppendConsume(const CXXMemberCallExpr *expr) {
  const bool isAppend = expr->getNumArgs() == 1;

  const uint32_t zero = theBuilder.getConstantUint32(0);

  const auto *object =
      expr->getImplicitObjectArgument()->IgnoreParenNoopCasts(astContext);

  auto bufferInfo = loadIfAliasVarRef(object);

  uint32_t index = incDecRWACSBufferCounter(
      expr, isAppend,
      // We have already translated the object in the above. Avoid duplication.
      /*loadObject=*/false);

  const auto bufferElemTy = hlsl::GetHLSLResourceResultType(object->getType());

  (void)turnIntoElementPtr(bufferInfo, bufferElemTy, {zero, index});

  if (isAppend) {
    // Write out the value
    auto arg0 = doExpr(expr->getArg(0));
    if (!arg0.isRValue()) {
      arg0.setResultId(theBuilder.createLoad(
          typeTranslator.translateType(bufferElemTy), arg0));
    }
    storeValue(bufferInfo, arg0, bufferElemTy);
    return 0;
  } else {
    // Note that we are returning a pointer (lvalue) here inorder to further
    // acess the fields in this element, e.g., buffer.Consume().a.b. So we
    // cannot forcefully set all normal function calls as returning rvalue.
    return bufferInfo;
  }
}

uint32_t
SPIRVEmitter::processStreamOutputAppend(const CXXMemberCallExpr *expr) {
  // TODO: handle multiple stream-output objects
  const auto *object =
      expr->getImplicitObjectArgument()->IgnoreParenNoopCasts(astContext);
  const auto *stream = cast<DeclRefExpr>(object)->getDecl();
  const uint32_t value = doExpr(expr->getArg(0));

  declIdMapper.writeBackOutputStream(stream, stream->getType(), value);
  theBuilder.createEmitVertex();

  return 0;
}

uint32_t
SPIRVEmitter::processStreamOutputRestart(const CXXMemberCallExpr *expr) {
  // TODO: handle multiple stream-output objects
  theBuilder.createEndPrimitive();
  return 0;
}

uint32_t SPIRVEmitter::emitGetSamplePosition(const uint32_t sampleCount,
                                             const uint32_t sampleIndex) {
  struct Float2 {
    float x;
    float y;
  };

  static const Float2 pos2[] = {
      {4.0 / 16.0, 4.0 / 16.0},
      {-4.0 / 16.0, -4.0 / 16.0},
  };

  static const Float2 pos4[] = {
      {-2.0 / 16.0, -6.0 / 16.0},
      {6.0 / 16.0, -2.0 / 16.0},
      {-6.0 / 16.0, 2.0 / 16.0},
      {2.0 / 16.0, 6.0 / 16.0},
  };

  static const Float2 pos8[] = {
      {1.0 / 16.0, -3.0 / 16.0}, {-1.0 / 16.0, 3.0 / 16.0},
      {5.0 / 16.0, 1.0 / 16.0},  {-3.0 / 16.0, -5.0 / 16.0},
      {-5.0 / 16.0, 5.0 / 16.0}, {-7.0 / 16.0, -1.0 / 16.0},
      {3.0 / 16.0, 7.0 / 16.0},  {7.0 / 16.0, -7.0 / 16.0},
  };

  static const Float2 pos16[] = {
      {1.0 / 16.0, 1.0 / 16.0},   {-1.0 / 16.0, -3.0 / 16.0},
      {-3.0 / 16.0, 2.0 / 16.0},  {4.0 / 16.0, -1.0 / 16.0},
      {-5.0 / 16.0, -2.0 / 16.0}, {2.0 / 16.0, 5.0 / 16.0},
      {5.0 / 16.0, 3.0 / 16.0},   {3.0 / 16.0, -5.0 / 16.0},
      {-2.0 / 16.0, 6.0 / 16.0},  {0.0 / 16.0, -7.0 / 16.0},
      {-4.0 / 16.0, -6.0 / 16.0}, {-6.0 / 16.0, 4.0 / 16.0},
      {-8.0 / 16.0, 0.0 / 16.0},  {7.0 / 16.0, -4.0 / 16.0},
      {6.0 / 16.0, 7.0 / 16.0},   {-7.0 / 16.0, -8.0 / 16.0},
  };

  // We are emitting the SPIR-V for the following HLSL source code:
  //
  //   float2 position;
  //
  //   if (count == 2) {
  //     position = pos2[index];
  //   }
  //   else if (count == 4) {
  //     position = pos4[index];
  //   }
  //   else if (count == 8) {
  //     position = pos8[index];
  //   }
  //   else if (count == 16) {
  //     position = pos16[index];
  //   }
  //   else {
  //     position = float2(0.0f, 0.0f);
  //   }

  const uint32_t boolType = theBuilder.getBoolType();
  const auto v2f32Type = theBuilder.getVecType(theBuilder.getFloat32Type(), 2);
  const uint32_t ptrType =
      theBuilder.getPointerType(v2f32Type, spv::StorageClass::Function);

  // Creates a SPIR-V function scope variable of type float2[len].
  const auto createArray = [this, v2f32Type](const Float2 *ptr, uint32_t len) {
    llvm::SmallVector<uint32_t, 16> components;
    for (uint32_t i = 0; i < len; ++i) {
      const auto x = theBuilder.getConstantFloat32(ptr[i].x);
      const auto y = theBuilder.getConstantFloat32(ptr[i].y);
      components.push_back(theBuilder.getConstantComposite(v2f32Type, {x, y}));
    }

    const auto arrType =
        theBuilder.getArrayType(v2f32Type, theBuilder.getConstantUint32(len));
    const auto val = theBuilder.getConstantComposite(arrType, components);

    const std::string varName =
        "var.GetSamplePosition.data." + std::to_string(len);
    const auto var = theBuilder.addFnVar(arrType, varName);
    theBuilder.createStore(var, val);
    return var;
  };

  const uint32_t pos2Arr = createArray(pos2, 2);
  const uint32_t pos4Arr = createArray(pos4, 4);
  const uint32_t pos8Arr = createArray(pos8, 8);
  const uint32_t pos16Arr = createArray(pos16, 16);

  const uint32_t resultVar =
      theBuilder.addFnVar(v2f32Type, "var.GetSamplePosition.result");

  const uint32_t then2BB =
      theBuilder.createBasicBlock("if.GetSamplePosition.then2");
  const uint32_t then4BB =
      theBuilder.createBasicBlock("if.GetSamplePosition.then4");
  const uint32_t then8BB =
      theBuilder.createBasicBlock("if.GetSamplePosition.then8");
  const uint32_t then16BB =
      theBuilder.createBasicBlock("if.GetSamplePosition.then16");

  const uint32_t else2BB =
      theBuilder.createBasicBlock("if.GetSamplePosition.else2");
  const uint32_t else4BB =
      theBuilder.createBasicBlock("if.GetSamplePosition.else4");
  const uint32_t else8BB =
      theBuilder.createBasicBlock("if.GetSamplePosition.else8");
  const uint32_t else16BB =
      theBuilder.createBasicBlock("if.GetSamplePosition.else16");

  const uint32_t merge2BB =
      theBuilder.createBasicBlock("if.GetSamplePosition.merge2");
  const uint32_t merge4BB =
      theBuilder.createBasicBlock("if.GetSamplePosition.merge4");
  const uint32_t merge8BB =
      theBuilder.createBasicBlock("if.GetSamplePosition.merge8");
  const uint32_t merge16BB =
      theBuilder.createBasicBlock("if.GetSamplePosition.merge16");

  //   if (count == 2) {
  const auto check2 =
      theBuilder.createBinaryOp(spv::Op::OpIEqual, boolType, sampleCount,
                                theBuilder.getConstantUint32(2));
  theBuilder.createConditionalBranch(check2, then2BB, else2BB, merge2BB);
  theBuilder.addSuccessor(then2BB);
  theBuilder.addSuccessor(else2BB);
  theBuilder.setMergeTarget(merge2BB);

  //     position = pos2[index];
  //   }
  theBuilder.setInsertPoint(then2BB);
  auto ac = theBuilder.createAccessChain(ptrType, pos2Arr, {sampleIndex});
  theBuilder.createStore(resultVar, theBuilder.createLoad(v2f32Type, ac));
  theBuilder.createBranch(merge2BB);
  theBuilder.addSuccessor(merge2BB);

  //   else if (count == 4) {
  theBuilder.setInsertPoint(else2BB);
  const auto check4 =
      theBuilder.createBinaryOp(spv::Op::OpIEqual, boolType, sampleCount,
                                theBuilder.getConstantUint32(4));
  theBuilder.createConditionalBranch(check4, then4BB, else4BB, merge4BB);
  theBuilder.addSuccessor(then4BB);
  theBuilder.addSuccessor(else4BB);
  theBuilder.setMergeTarget(merge4BB);

  //     position = pos4[index];
  //   }
  theBuilder.setInsertPoint(then4BB);
  ac = theBuilder.createAccessChain(ptrType, pos4Arr, {sampleIndex});
  theBuilder.createStore(resultVar, theBuilder.createLoad(v2f32Type, ac));
  theBuilder.createBranch(merge4BB);
  theBuilder.addSuccessor(merge4BB);

  //   else if (count == 8) {
  theBuilder.setInsertPoint(else4BB);
  const auto check8 =
      theBuilder.createBinaryOp(spv::Op::OpIEqual, boolType, sampleCount,
                                theBuilder.getConstantUint32(8));
  theBuilder.createConditionalBranch(check8, then8BB, else8BB, merge8BB);
  theBuilder.addSuccessor(then8BB);
  theBuilder.addSuccessor(else8BB);
  theBuilder.setMergeTarget(merge8BB);

  //     position = pos8[index];
  //   }
  theBuilder.setInsertPoint(then8BB);
  ac = theBuilder.createAccessChain(ptrType, pos8Arr, {sampleIndex});
  theBuilder.createStore(resultVar, theBuilder.createLoad(v2f32Type, ac));
  theBuilder.createBranch(merge8BB);
  theBuilder.addSuccessor(merge8BB);

  //   else if (count == 16) {
  theBuilder.setInsertPoint(else8BB);
  const auto check16 =
      theBuilder.createBinaryOp(spv::Op::OpIEqual, boolType, sampleCount,
                                theBuilder.getConstantUint32(16));
  theBuilder.createConditionalBranch(check16, then16BB, else16BB, merge16BB);
  theBuilder.addSuccessor(then16BB);
  theBuilder.addSuccessor(else16BB);
  theBuilder.setMergeTarget(merge16BB);

  //     position = pos16[index];
  //   }
  theBuilder.setInsertPoint(then16BB);
  ac = theBuilder.createAccessChain(ptrType, pos16Arr, {sampleIndex});
  theBuilder.createStore(resultVar, theBuilder.createLoad(v2f32Type, ac));
  theBuilder.createBranch(merge16BB);
  theBuilder.addSuccessor(merge16BB);

  //   else {
  //     position = float2(0.0f, 0.0f);
  //   }
  theBuilder.setInsertPoint(else16BB);
  const auto zero = theBuilder.getConstantFloat32(0);
  const auto v2f32Zero =
      theBuilder.getConstantComposite(v2f32Type, {zero, zero});
  theBuilder.createStore(resultVar, v2f32Zero);
  theBuilder.createBranch(merge16BB);
  theBuilder.addSuccessor(merge16BB);

  theBuilder.setInsertPoint(merge16BB);
  theBuilder.createBranch(merge8BB);
  theBuilder.addSuccessor(merge8BB);

  theBuilder.setInsertPoint(merge8BB);
  theBuilder.createBranch(merge4BB);
  theBuilder.addSuccessor(merge4BB);

  theBuilder.setInsertPoint(merge4BB);
  theBuilder.createBranch(merge2BB);
  theBuilder.addSuccessor(merge2BB);

  theBuilder.setInsertPoint(merge2BB);
  return theBuilder.createLoad(v2f32Type, resultVar);
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

void SPIRVEmitter::handleOffsetInMethodCall(const CXXMemberCallExpr *expr,
                                            uint32_t index,
                                            uint32_t *constOffset,
                                            uint32_t *varOffset) {
  // Ensure the given arg index is not out-of-range.
  assert(index < expr->getNumArgs());

  *constOffset = *varOffset = 0; // Initialize both first
  if ((*constOffset = tryToEvaluateAsConst(expr->getArg(index))))
    return; // Constant offset
  else
    *varOffset = doExpr(expr->getArg(index));
}

SpirvEvalInfo
SPIRVEmitter::processIntrinsicMemberCall(const CXXMemberCallExpr *expr,
                                         hlsl::IntrinsicOp opcode) {
  using namespace hlsl;

  uint32_t retVal = 0;
  switch (opcode) {
  case IntrinsicOp::MOP_Sample:
    retVal = processTextureSampleGather(expr, /*isSample=*/true);
    break;
  case IntrinsicOp::MOP_Gather:
    retVal = processTextureSampleGather(expr, /*isSample=*/false);
    break;
  case IntrinsicOp::MOP_SampleBias:
    retVal = processTextureSampleBiasLevel(expr, /*isBias=*/true);
    break;
  case IntrinsicOp::MOP_SampleLevel:
    retVal = processTextureSampleBiasLevel(expr, /*isBias=*/false);
    break;
  case IntrinsicOp::MOP_SampleGrad:
    retVal = processTextureSampleGrad(expr);
    break;
  case IntrinsicOp::MOP_SampleCmp:
    retVal = processTextureSampleCmpCmpLevelZero(expr, /*isCmp=*/true);
    break;
  case IntrinsicOp::MOP_SampleCmpLevelZero:
    retVal = processTextureSampleCmpCmpLevelZero(expr, /*isCmp=*/false);
    break;
  case IntrinsicOp::MOP_GatherRed:
    retVal = processTextureGatherRGBACmpRGBA(expr, /*isCmp=*/false, 0);
    break;
  case IntrinsicOp::MOP_GatherGreen:
    retVal = processTextureGatherRGBACmpRGBA(expr, /*isCmp=*/false, 1);
    break;
  case IntrinsicOp::MOP_GatherBlue:
    retVal = processTextureGatherRGBACmpRGBA(expr, /*isCmp=*/false, 2);
    break;
  case IntrinsicOp::MOP_GatherAlpha:
    retVal = processTextureGatherRGBACmpRGBA(expr, /*isCmp=*/false, 3);
    break;
  case IntrinsicOp::MOP_GatherCmp:
    retVal = processTextureGatherCmp(expr);
    break;
  case IntrinsicOp::MOP_GatherCmpRed:
    retVal = processTextureGatherRGBACmpRGBA(expr, /*isCmp=*/true, 0);
    break;
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
    retVal = processGetDimensions(expr);
    break;
  case IntrinsicOp::MOP_CalculateLevelOfDetail:
    retVal = processTextureLevelOfDetail(expr);
    break;
  case IntrinsicOp::MOP_IncrementCounter:
    retVal = theBuilder.createUnaryOp(
        spv::Op::OpBitcast, theBuilder.getUint32Type(),
        incDecRWACSBufferCounter(expr, /*isInc*/ true));
    break;
  case IntrinsicOp::MOP_DecrementCounter:
    retVal = theBuilder.createUnaryOp(
        spv::Op::OpBitcast, theBuilder.getUint32Type(),
        incDecRWACSBufferCounter(expr, /*isInc*/ false));
    break;
  case IntrinsicOp::MOP_Append:
    if (hlsl::IsHLSLStreamOutputType(
            expr->getImplicitObjectArgument()->getType()))
      return processStreamOutputAppend(expr);
    else
      return processACSBufferAppendConsume(expr);
  case IntrinsicOp::MOP_Consume:
    return processACSBufferAppendConsume(expr);
  case IntrinsicOp::MOP_RestartStrip:
    retVal = processStreamOutputRestart(expr);
    break;
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
    retVal = processRWByteAddressBufferAtomicMethods(opcode, expr);
    break;
  case IntrinsicOp::MOP_GetSamplePosition:
    retVal = processGetSamplePosition(expr);
    break;
  case IntrinsicOp::MOP_SubpassLoad:
    retVal = processSubpassLoad(expr);
    break;
  case IntrinsicOp::MOP_GatherCmpGreen:
  case IntrinsicOp::MOP_GatherCmpBlue:
  case IntrinsicOp::MOP_GatherCmpAlpha:
  case IntrinsicOp::MOP_CalculateLevelOfDetailUnclamped:
    emitError("no equivalent for %0 intrinsic method in Vulkan",
              expr->getCallee()->getExprLoc())
        << expr->getMethodDecl()->getName();
    return 0;
  default:
    emitError("intrinsic '%0' method unimplemented",
              expr->getCallee()->getExprLoc())
        << expr->getDirectCallee()->getName();
    return 0;
  }

  return SpirvEvalInfo(retVal).setRValue();
}

uint32_t SPIRVEmitter::createImageSample(
    QualType retType, uint32_t imageType, uint32_t image, uint32_t sampler,
    bool isNonUniform, uint32_t coordinate, uint32_t compareVal, uint32_t bias,
    uint32_t lod, std::pair<uint32_t, uint32_t> grad, uint32_t constOffset,
    uint32_t varOffset, uint32_t constOffsets, uint32_t sample, uint32_t minLod,
    uint32_t residencyCodeId) {

  const auto retTypeId = typeTranslator.translateType(retType);

  // SampleDref* instructions in SPIR-V always return a scalar.
  // They also have the correct type in HLSL.
  if (compareVal) {
    return theBuilder.createImageSample(
        retTypeId, imageType, image, sampler, isNonUniform, coordinate,
        compareVal, bias, lod, grad, constOffset, varOffset, constOffsets,
        sample, minLod, residencyCodeId);
  }

  // Non-Dref Sample instructions in SPIR-V must always return a vec4.
  auto texelTypeId = retTypeId;
  QualType elemType = {};
  uint32_t elemTypeId = 0;
  uint32_t retVecSize = 0;
  if (TypeTranslator::isVectorType(retType, &elemType, &retVecSize) &&
      retVecSize != 4) {
    elemTypeId = typeTranslator.translateType(elemType);
    texelTypeId = theBuilder.getVecType(elemTypeId, 4);
  } else if (TypeTranslator::isScalarType(retType)) {
    retVecSize = 1;
    elemTypeId = typeTranslator.translateType(retType);
    texelTypeId = theBuilder.getVecType(elemTypeId, 4);
  }

  // The Lod and Grad image operands requires explicit-lod instructions.
  // Otherwise we use implicit-lod instructions.
  const bool isExplicit = lod || (grad.first && grad.second);

  // Implicit-lod instructions are only allowed in pixel shader.
  if (!shaderModel.IsPS() && !isExplicit)
    needsLegalization = true;

  uint32_t retVal = theBuilder.createImageSample(
      texelTypeId, imageType, image, sampler, isNonUniform, coordinate,
      compareVal, bias, lod, grad, constOffset, varOffset, constOffsets, sample,
      minLod, residencyCodeId);

  // Extract smaller vector from the vec4 result if necessary.
  if (texelTypeId != retTypeId) {
    retVal = extractVecFromVec4(retVal, retVecSize, elemTypeId);
  }

  return retVal;
}

uint32_t SPIRVEmitter::processTextureSampleGather(const CXXMemberCallExpr *expr,
                                                  const bool isSample) {
  // Signatures:
  // For Texture1D, Texture1DArray, Texture2D, Texture2DArray, Texture3D:
  // DXGI_FORMAT Object.Sample(sampler_state S,
  //                           float Location
  //                           [, int Offset]
  //                           [, float Clamp]
  //                           [, out uint Status]);
  //
  // For TextureCube and TextureCubeArray:
  // DXGI_FORMAT Object.Sample(sampler_state S,
  //                           float Location
  //                           [, float Clamp]
  //                           [, out uint Status]);
  //
  // For Texture2D/Texture2DArray:
  // <Template Type>4 Object.Gather(sampler_state S,
  //                                float2|3|4 Location,
  //                                int2 Offset
  //                                [, uint Status]);
  //
  // For TextureCube/TextureCubeArray:
  // <Template Type>4 Object.Gather(sampler_state S,
  //                                float2|3|4 Location
  //                                [, uint Status]);
  //
  // Other Texture types do not have a Gather method.

  const auto numArgs = expr->getNumArgs();
  const bool hasStatusArg =
      expr->getArg(numArgs - 1)->getType()->isUnsignedIntegerType();

  uint32_t clamp = 0;
  if (numArgs > 2 && expr->getArg(2)->getType()->isFloatingType())
    clamp = doExpr(expr->getArg(2));
  else if (numArgs > 3 && expr->getArg(3)->getType()->isFloatingType())
    clamp = doExpr(expr->getArg(3));
  const bool hasClampArg = (clamp != 0);
  const auto status =
      hasStatusArg ? static_cast<uint32_t>(doExpr(expr->getArg(numArgs - 1)))
                   : 0;

  // Subtract 1 for status (if it exists), subtract 1 for clamp (if it exists),
  // and subtract 2 for sampler_state and location.
  const bool hasOffsetArg = numArgs - hasStatusArg - hasClampArg - 2 > 0;

  const auto *imageExpr = expr->getImplicitObjectArgument();
  const uint32_t imageType = typeTranslator.translateType(imageExpr->getType());
  const auto image = loadIfGLValue(imageExpr);
  const auto sampler = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  // .Sample()/.Gather() may have a third optional paramter for offset.
  uint32_t constOffset = 0, varOffset = 0;
  if (hasOffsetArg)
    handleOffsetInMethodCall(expr, 2, &constOffset, &varOffset);
  const bool isNonUniform = image.isNonUniform() || sampler.isNonUniform();

  const auto retType = expr->getDirectCallee()->getReturnType();
  const auto retTypeId = typeTranslator.translateType(retType);
  if (isSample) {
    return createImageSample(
        retType, imageType, image, sampler, isNonUniform, coordinate,
        /*compareVal*/ 0, /*bias*/ 0, /*lod*/ 0, std::make_pair(0, 0),
        constOffset, varOffset, /*constOffsets*/ 0, /*sampleNumber*/ 0,
        /*minLod*/ clamp, status);
  } else {
    return theBuilder.createImageGather(
        retTypeId, imageType, image, sampler, isNonUniform, coordinate,
        // .Gather() doc says we return four components of red data.
        theBuilder.getConstantInt32(0), /*compareVal*/ 0, constOffset,
        varOffset, /*constOffsets*/ 0, /*sampleNumber*/ 0, status);
  }
}

uint32_t
SPIRVEmitter::processTextureSampleBiasLevel(const CXXMemberCallExpr *expr,
                                            const bool isBias) {
  // Signatures:
  // For Texture1D, Texture1DArray, Texture2D, Texture2DArray, and Texture3D:
  // DXGI_FORMAT Object.SampleBias(sampler_state S,
  //                               float Location,
  //                               float Bias
  //                               [, int Offset]
  //                               [, float clamp]
  //                               [, out uint Status]);
  //
  // For TextureCube and TextureCubeArray:
  // DXGI_FORMAT Object.SampleBias(sampler_state S,
  //                               float Location,
  //                               float Bias
  //                               [, float clamp]
  //                               [, out uint Status]);
  //
  // For Texture1D, Texture1DArray, Texture2D, Texture2DArray, and Texture3D:
  // DXGI_FORMAT Object.SampleLevel(sampler_state S,
  //                                float Location,
  //                                float LOD
  //                                [, int Offset]
  //                                [, out uint Status]);
  //
  // For TextureCube and TextureCubeArray:
  // DXGI_FORMAT Object.SampleLevel(sampler_state S,
  //                                float Location,
  //                                float LOD
  //                                [, out uint Status]);

  const auto numArgs = expr->getNumArgs();
  const bool hasStatusArg =
      expr->getArg(numArgs - 1)->getType()->isUnsignedIntegerType();
  const auto status =
      hasStatusArg ? static_cast<uint32_t>(doExpr(expr->getArg(numArgs - 1)))
                   : 0;

  uint32_t clamp = 0;
  // The .SampleLevel() methods do not take the clamp argument.
  if (isBias) {
    if (numArgs > 3 && expr->getArg(3)->getType()->isFloatingType())
      clamp = doExpr(expr->getArg(3));
    else if (numArgs > 4 && expr->getArg(4)->getType()->isFloatingType())
      clamp = doExpr(expr->getArg(4));
  }
  const bool hasClampArg = clamp != 0;

  // Subtract 1 for clamp (if it exists), 1 for status (if it exists),
  // and 3 for sampler_state, location, and Bias/LOD.
  const bool hasOffsetArg = numArgs - hasClampArg - hasStatusArg - 3 > 0;

  const auto *imageExpr = expr->getImplicitObjectArgument();
  const uint32_t imageType = typeTranslator.translateType(imageExpr->getType());
  const auto image = loadIfGLValue(imageExpr);
  const auto sampler = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  uint32_t lod = 0;
  uint32_t bias = 0;
  if (isBias) {
    bias = doExpr(expr->getArg(2));
  } else {
    lod = doExpr(expr->getArg(2));
  }
  // If offset is present in .Bias()/.SampleLevel(), it is the fourth argument.
  uint32_t constOffset = 0, varOffset = 0;
  if (hasOffsetArg)
    handleOffsetInMethodCall(expr, 3, &constOffset, &varOffset);

  const auto retType = expr->getDirectCallee()->getReturnType();

  return createImageSample(
      retType, imageType, image, sampler,
      image.isNonUniform() || sampler.isNonUniform(), coordinate,
      /*compareVal*/ 0, bias, lod, std::make_pair(0, 0), constOffset, varOffset,
      /*constOffsets*/ 0, /*sampleNumber*/ 0, /*minLod*/ clamp, status);
}

uint32_t SPIRVEmitter::processTextureSampleGrad(const CXXMemberCallExpr *expr) {
  // Signature:
  // For Texture1D, Texture1DArray, Texture2D, Texture2DArray, and Texture3D:
  // DXGI_FORMAT Object.SampleGrad(sampler_state S,
  //                               float Location,
  //                               float DDX,
  //                               float DDY
  //                               [, int Offset]
  //                               [, float Clamp]
  //                               [, out uint Status]);
  //
  // For TextureCube and TextureCubeArray:
  // DXGI_FORMAT Object.SampleGrad(sampler_state S,
  //                               float Location,
  //                               float DDX,
  //                               float DDY
  //                               [, float Clamp]
  //                               [, out uint Status]);

  const auto numArgs = expr->getNumArgs();
  const bool hasStatusArg =
      expr->getArg(numArgs - 1)->getType()->isUnsignedIntegerType();
  const auto status =
      hasStatusArg ? static_cast<uint32_t>(doExpr(expr->getArg(numArgs - 1)))
                   : 0;

  uint32_t clamp = 0;
  if (numArgs > 4 && expr->getArg(4)->getType()->isFloatingType())
    clamp = doExpr(expr->getArg(4));
  else if (numArgs > 5 && expr->getArg(5)->getType()->isFloatingType())
    clamp = doExpr(expr->getArg(5));
  const bool hasClampArg = clamp != 0;

  // Subtract 1 for clamp (if it exists), 1 for status (if it exists),
  // and 4 for sampler_state, location, DDX, and DDY;
  const bool hasOffsetArg = numArgs - hasClampArg - hasStatusArg - 4 > 0;

  const auto *imageExpr = expr->getImplicitObjectArgument();
  const uint32_t imageType = typeTranslator.translateType(imageExpr->getType());
  const auto image = loadIfGLValue(imageExpr);
  const auto sampler = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  const uint32_t ddx = doExpr(expr->getArg(2));
  const uint32_t ddy = doExpr(expr->getArg(3));
  // If offset is present in .SampleGrad(), it is the fifth argument.
  uint32_t constOffset = 0, varOffset = 0;
  if (hasOffsetArg)
    handleOffsetInMethodCall(expr, 4, &constOffset, &varOffset);

  const auto retType = expr->getDirectCallee()->getReturnType();
  return createImageSample(
      retType, imageType, image, sampler,
      image.isNonUniform() || sampler.isNonUniform(), coordinate,
      /*compareVal*/ 0, /*bias*/ 0, /*lod*/ 0, std::make_pair(ddx, ddy),
      constOffset, varOffset, /*constOffsets*/ 0, /*sampleNumber*/ 0,
      /*minLod*/ clamp, status);
}

uint32_t
SPIRVEmitter::processTextureSampleCmpCmpLevelZero(const CXXMemberCallExpr *expr,
                                                  const bool isCmp) {
  // .SampleCmp() Signature:
  //
  // For Texture1D, Texture1DArray, Texture2D, Texture2DArray:
  // float Object.SampleCmp(
  //   SamplerComparisonState S,
  //   float Location,
  //   float CompareValue
  //   [, int Offset]
  //   [, float Clamp]
  //   [, out uint Status]
  // );
  //
  // For TextureCube and TextureCubeArray:
  // float Object.SampleCmp(
  //   SamplerComparisonState S,
  //   float Location,
  //   float CompareValue
  //   [, float Clamp]
  //   [, out uint Status]
  // );
  //
  // .SampleCmpLevelZero() is identical to .SampleCmp() on mipmap level 0 only.
  // It never takes a clamp argument, which is good because lod and clamp may
  // not be used together.
  //
  // .SampleCmpLevelZero() Signature:
  //
  // For Texture1D, Texture1DArray, Texture2D, Texture2DArray:
  // float Object.SampleCmpLevelZero(
  //   SamplerComparisonState S,
  //   float Location,
  //   float CompareValue
  //   [, int Offset]
  //   [, out uint Status]
  // );
  //
  // For TextureCube and TextureCubeArray:
  // float Object.SampleCmpLevelZero(
  //   SamplerComparisonState S,
  //   float Location,
  //   float CompareValue
  //   [, out uint Status]
  // );

  const auto numArgs = expr->getNumArgs();
  const bool hasStatusArg =
      expr->getArg(numArgs - 1)->getType()->isUnsignedIntegerType();
  const auto status =
      hasStatusArg ? static_cast<uint32_t>(doExpr(expr->getArg(numArgs - 1)))
                   : 0;

  uint32_t clamp = 0;
  // The .SampleCmpLevelZero() methods do not take the clamp argument.
  if (isCmp) {
    if (numArgs > 3 && expr->getArg(3)->getType()->isFloatingType())
      clamp = doExpr(expr->getArg(3));
    else if (numArgs > 4 && expr->getArg(4)->getType()->isFloatingType())
      clamp = doExpr(expr->getArg(4));
  }
  const bool hasClampArg = clamp != 0;

  // Subtract 1 for clamp (if it exists), 1 for status (if it exists),
  // and 3 for sampler_state, location, and compare_value.
  const bool hasOffsetArg = numArgs - hasClampArg - hasStatusArg - 3 > 0;

  const auto *imageExpr = expr->getImplicitObjectArgument();
  const auto image = loadIfGLValue(imageExpr);
  const auto sampler = doExpr(expr->getArg(0));
  const uint32_t coordinate = doExpr(expr->getArg(1));
  const uint32_t compareVal = doExpr(expr->getArg(2));
  // If offset is present in .SampleCmp(), it will be the fourth argument.
  uint32_t constOffset = 0, varOffset = 0;
  if (hasOffsetArg)
    handleOffsetInMethodCall(expr, 3, &constOffset, &varOffset);
  const uint32_t lod = isCmp ? 0 : theBuilder.getConstantFloat32(0);

  const auto retType = expr->getDirectCallee()->getReturnType();
  const auto imageType = typeTranslator.translateType(imageExpr->getType());

  return createImageSample(
      retType, imageType, image, sampler,
      image.isNonUniform() || sampler.isNonUniform(), coordinate, compareVal,
      /*bias*/ 0, lod, std::make_pair(0, 0), constOffset, varOffset,
      /*constOffsets*/ 0, /*sampleNumber*/ 0, /*minLod*/ clamp, status);
}

SpirvEvalInfo
SPIRVEmitter::processBufferTextureLoad(const CXXMemberCallExpr *expr) {
  // Signature:
  // For Texture1D, Texture1DArray, Texture2D, Texture2DArray, Texture3D:
  // ret Object.Load(int Location
  //                 [, int Offset]
  //                 [, uint status]);
  //
  // For Texture2DMS and Texture2DMSArray, there is one additional argument:
  // ret Object.Load(int Location
  //                 [, int SampleIndex]
  //                 [, int Offset]
  //                 [, uint status]);
  //
  // For (RW)Buffer, RWTexture1D, RWTexture1DArray, RWTexture2D,
  // RWTexture2DArray, RWTexture3D:
  // ret Object.Load (int Location
  //                  [, uint status]);
  //
  // Note: (RW)ByteAddressBuffer and (RW)StructuredBuffer types also have Load
  // methods that take an additional Status argument. However, since these types
  // are not represented as OpTypeImage in SPIR-V, we don't have a way of
  // figuring out the Residency Code for them. Therefore having the Status
  // argument for these types is not supported.
  //
  // For (RW)ByteAddressBuffer:
  // ret Object.{Load,Load2,Load3,Load4} (int Location
  //                                      [, uint status]);
  //
  // For (RW)StructuredBuffer:
  // ret Object.Load (int Location
  //                  [, uint status]);
  //

  const auto *object = expr->getImplicitObjectArgument();
  const auto objectType = object->getType();

  if (typeTranslator.isRWByteAddressBuffer(objectType) ||
      typeTranslator.isByteAddressBuffer(objectType))
    return processByteAddressBufferLoadStore(expr, 1, /*doStore*/ false);

  if (TypeTranslator::isStructuredBuffer(objectType))
    return processStructuredBufferLoad(expr);

  const auto numArgs = expr->getNumArgs();
  const auto *location = expr->getArg(0);
  const bool isTextureMS = TypeTranslator::isTextureMS(objectType);
  const bool hasStatusArg =
      expr->getArg(numArgs - 1)->getType()->isUnsignedIntegerType();
  const auto status =
      hasStatusArg ? static_cast<uint32_t>(doExpr(expr->getArg(numArgs - 1)))
                   : 0;

  if (TypeTranslator::isBuffer(objectType) ||
      TypeTranslator::isRWBuffer(objectType) ||
      TypeTranslator::isRWTexture(objectType))
    return processBufferTextureLoad(object, doExpr(location), /*constOffset*/ 0,
                                    /*varOffset*/ 0, /*lod*/ 0,
                                    /*residencyCode*/ status);

  // Subtract 1 for status (if it exists), and 1 for sampleIndex (if it exists),
  // and 1 for location.
  const bool hasOffsetArg = numArgs - hasStatusArg - isTextureMS - 1 > 0;

  if (TypeTranslator::isTexture(objectType)) {
    // .Load() has a second optional paramter for offset.
    const auto locationId = doExpr(location);
    uint32_t constOffset = 0, varOffset = 0;
    uint32_t coordinate = locationId, lod = 0;

    if (isTextureMS) {
      // SampleIndex is only available when the Object is of Texture2DMS or
      // Texture2DMSArray types. Under those cases, Offset will be the third
      // parameter (index 2).
      lod = doExpr(expr->getArg(1));
      if (hasOffsetArg)
        handleOffsetInMethodCall(expr, 2, &constOffset, &varOffset);
    } else {
      // For Texture Load() functions, the location parameter is a vector
      // that consists of both the coordinate and the mipmap level (via the
      // last vector element). We need to split it here since the
      // OpImageFetch SPIR-V instruction encodes them as separate arguments.
      splitVecLastElement(location->getType(), locationId, &coordinate, &lod);
      // For textures other than Texture2DMS(Array), offset should be the
      // second parameter (index 1).
      if (hasOffsetArg)
        handleOffsetInMethodCall(expr, 1, &constOffset, &varOffset);
    }

    return processBufferTextureLoad(object, coordinate, constOffset, varOffset,
                                    lod, status);
  }
  emitError("Load() of the given object type unimplemented",
            object->getExprLoc());
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
    emitError("GetDimensions() of the given object type unimplemented",
              expr->getExprLoc());
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
                                      /*constOffset*/ 0, /*varOffset*/ 0, lod,
                                      /*residencyCode*/ 0);
    }
    // .mips[][] or .sample[][] must use the correct slice.
    if (isTextureMipsSampleIndexing(expr, &baseExpr, &indexExpr, &lodExpr)) {
      const uint32_t lod = doExpr(lodExpr);
      return processBufferTextureLoad(baseExpr, doExpr(indexExpr),
                                      /*constOffset*/ 0, /*varOffset*/ 0, lod,
                                      /*residencyCode*/ 0);
    }
  }

  llvm::SmallVector<uint32_t, 4> indices;
  const Expr *baseExpr = collectArrayStructIndices(expr, &indices);

  auto base = loadIfAliasVarRef(baseExpr);

  if (indices.empty())
    return base; // For indexing into size-1 vectors and 1xN matrices

  // If we are indexing into a rvalue, to use OpAccessChain, we first need
  // to create a local variable to hold the rvalue.
  //
  // TODO: We can optimize the codegen by emitting OpCompositeExtract if
  // all indices are contant integers.
  if (base.isRValue()) {
    base = createTemporaryVar(baseExpr->getType(), "vector", base);
  }

  return turnIntoElementPtr(base, expr->getType(), indices);
}

SpirvEvalInfo
SPIRVEmitter::doExtMatrixElementExpr(const ExtMatrixElementExpr *expr) {
  const Expr *baseExpr = expr->getBase();
  const auto baseInfo = doExpr(baseExpr);
  const auto layoutRule = baseInfo.getLayoutRule();
  const auto elemType = hlsl::GetHLSLMatElementType(baseExpr->getType());
  const auto accessor = expr->getEncodedElementAccess();
  const uint32_t elemTypeId =
      typeTranslator.translateType(elemType, layoutRule);

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
          theBuilder.getPointerType(elemTypeId, baseInfo.getStorageClass());
      if (!indices.empty()) {
        assert(!baseInfo.isRValue());
        // Load the element via access chain
        elem = theBuilder.createAccessChain(ptrType, baseInfo, indices);
      } else {
        // The matrix is of size 1x1. No need to use access chain, base should
        // be the source pointer.
        elem = baseInfo;
      }
      elem = theBuilder.createLoad(elemTypeId, elem);
    } else { // e.g., (mat1 + mat2)._m11
      elem = theBuilder.createCompositeExtract(elemTypeId, baseInfo, indices);
    }
    elements.push_back(elem);
  }

  const auto size = elements.size();
  auto valueId = elements.front();
  if (size > 1) {
    const uint32_t vecType = theBuilder.getVecType(elemTypeId, size);
    valueId = theBuilder.createCompositeConstruct(vecType, elements);
  }

  // Note: Special-case: Booleans have no physical layout, and therefore when
  // layout is required booleans are represented as unsigned integers.
  // Therefore, after loading the uint we should convert it boolean.
  if (elemType->isBooleanType() && layoutRule != LayoutRule::Void) {
    const auto fromType =
        size == 1 ? astContext.UnsignedIntTy
                  : astContext.getExtVectorType(astContext.UnsignedIntTy, size);
    const auto toType =
        size == 1 ? astContext.BoolTy
                  : astContext.getExtVectorType(astContext.BoolTy, size);
    valueId = castToBool(valueId, fromType, toType);
  }
  return SpirvEvalInfo(valueId).setRValue();
}

SpirvEvalInfo
SPIRVEmitter::doHLSLVectorElementExpr(const HLSLVectorElementExpr *expr) {
  const Expr *baseExpr = nullptr;
  hlsl::VectorMemberAccessPositions accessor;
  condenseVectorElementExpr(expr, &baseExpr, &accessor);

  const QualType baseType = baseExpr->getType();
  assert(hlsl::IsHLSLVecType(baseType));
  const auto baseSize = hlsl::GetHLSLVecSize(baseType);
  const auto accessorSize = static_cast<size_t>(accessor.Count);

  // Depending on the number of elements selected, we emit different
  // instructions.
  // For vectors of size greater than 1, if we are only selecting one element,
  // typical access chain or composite extraction should be fine. But if we
  // are selecting more than one elements, we must resolve to vector specific
  // operations.
  // For size-1 vectors, if we are selecting their single elements multiple
  // times, we need composite construct instructions.

  if (accessorSize == 1) {
    auto baseInfo = doExpr(baseExpr);

    if (baseSize == 1) {
      // Selecting one element from a size-1 vector. The underlying vector is
      // already treated as a scalar.
      return baseInfo;
    }

    // If the base is an lvalue, we should emit an access chain instruction
    // so that we can load/store the specified element. For rvalue base,
    // we should use composite extraction. We should check the immediate base
    // instead of the original base here since we can have something like
    // v.xyyz to turn a lvalue v into rvalue.
    const auto type =
        typeTranslator.translateType(expr->getType(), baseInfo.getLayoutRule());
    if (!baseInfo.isRValue()) { // E.g., v.x;
      const uint32_t ptrType =
          theBuilder.getPointerType(type, baseInfo.getStorageClass());
      const uint32_t index = theBuilder.getConstantInt32(accessor.Swz0);
      // We need a lvalue here. Do not try to load.
      return baseInfo.setResultId(
          theBuilder.createAccessChain(ptrType, baseInfo, {index}));
    } else { // E.g., (v + w).x;
      // The original base vector may not be a rvalue. Need to load it if
      // it is lvalue since ImplicitCastExpr (LValueToRValue) will be missing
      // for that case.
      auto result =
          theBuilder.createCompositeExtract(type, baseInfo, {accessor.Swz0});
      // Special-case: Booleans in SPIR-V do not have a physical layout. Uint is
      // used to represent them when layout is required.
      if (expr->getType()->isBooleanType() &&
          baseInfo.getLayoutRule() != LayoutRule::Void)
        result =
            castToBool(result, astContext.UnsignedIntTy, astContext.BoolTy);
      return baseInfo.setResultId(result);
    }
  }

  if (baseSize == 1) {
    // Selecting more than one element from a size-1 vector, for example,
    // <scalar>.xx. Construct the vector.
    auto info = loadIfGLValue(baseExpr);
    const auto type =
        typeTranslator.translateType(expr->getType(), info.getLayoutRule());
    llvm::SmallVector<uint32_t, 4> components(accessorSize, info);
    return info
        .setResultId(theBuilder.createCompositeConstruct(type, components))
        .setRValue();
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

  auto info = loadIfGLValue(baseExpr);
  const auto type =
      typeTranslator.translateType(expr->getType(), info.getLayoutRule());
  // Use base for both vectors. But we are only selecting values from the
  // first one.
  return info.setResultId(
      theBuilder.createVectorShuffle(type, info, info, selectors));
}

SpirvEvalInfo SPIRVEmitter::doInitListExpr(const InitListExpr *expr) {
  if (const uint32_t id = tryToEvaluateAsConst(expr))
    return SpirvEvalInfo(id).setRValue();

  return SpirvEvalInfo(InitListHandler(*this).process(expr)).setRValue();
}

SpirvEvalInfo SPIRVEmitter::doMemberExpr(const MemberExpr *expr) {
  llvm::SmallVector<uint32_t, 4> indices;
  const Expr *base = collectArrayStructIndices(expr, &indices);
  auto info = loadIfAliasVarRef(base);

  if (!indices.empty()) {
    // Sometime we are accessing the member of a rvalue, e.g.,
    // <some-function-returing-a-struct>().<some-field>
    // Create a temporary variable to hold the rvalue so that we can use access
    // chain to index into it.
    if (info.isRValue()) {
      SpirvEvalInfo tempVar = createTemporaryVar(
          base->getType(), TypeTranslator::getName(base->getType()), info);
      (void)turnIntoElementPtr(tempVar, expr->getType(), indices);
      info.setResultId(theBuilder.createLoad(
          typeTranslator.translateType(expr->getType()), tempVar));
    } else {
      (void)turnIntoElementPtr(info, expr->getType(), indices);
    }
  }

  return info;
}

uint32_t SPIRVEmitter::createTemporaryVar(QualType type, llvm::StringRef name,
                                          const SpirvEvalInfo &init) {
  // We are creating a temporary variable in the Function storage class here,
  // which means it has void layout rule.
  const uint32_t varType = typeTranslator.translateType(type);
  const std::string varName = "temp.var." + name.str();
  const uint32_t varId = theBuilder.addFnVar(varType, varName);
  storeValue(varId, init, type);
  return varId;
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
    if (TypeTranslator::isMxNMatrix(subType)) {
      // For matrices, we can only increment/decrement each vector of it.
      const auto actOnEachVec = [this, spvOp, one](uint32_t /*index*/,
                                                   uint32_t vecType,
                                                   uint32_t lhsVec) {
        const auto valId =
            theBuilder.createBinaryOp(spvOp, vecType, lhsVec, one);
        return SpirvEvalInfo(valId).setRValue();
      };
      incValue = processEachVectorInMatrix(subExpr, originValue, actOnEachVec);
    } else {
      incValue = theBuilder.createBinaryOp(spvOp, subTypeId, originValue, one);
    }
    theBuilder.createStore(subValue, incValue);

    // Prefix increment/decrement operator returns a lvalue, while postfix
    // increment/decrement returns a rvalue.
    return isPre ? subValue : subValue.setResultId(originValue).setRValue();
  }
  case UO_Not: {
    return subValue
        .setResultId(
            theBuilder.createUnaryOp(spv::Op::OpNot, subTypeId, subValue))
        .setRValue();
  }
  case UO_LNot: {
    // Parsing will do the necessary casting to make sure we are applying the
    // ! operator on boolean values.
    return subValue
        .setResultId(theBuilder.createUnaryOp(spv::Op::OpLogicalNot, subTypeId,
                                              subValue))
        .setRValue();
  }
  case UO_Plus:
    // No need to do anything for the prefix + operator.
    return subValue;
  case UO_Minus: {
    // SPIR-V have two opcodes for negating values: OpSNegate and OpFNegate.
    const spv::Op spvOp = isFloatOrVecOfFloatType(subType) ? spv::Op::OpFNegate
                                                           : spv::Op::OpSNegate;
    return subValue
        .setResultId(theBuilder.createUnaryOp(spvOp, subTypeId, subValue))
        .setRValue();
  }
  default:
    break;
  }

  emitError("unary operator '%0' unimplemented", expr->getExprLoc())
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
  case BO_##kind: {                                                            \
    if (isSintType || isUintType) {                                            \
      return spv::Op::Op##intBinOp;                                            \
    }                                                                          \
    if (isFloatType) {                                                         \
      return spv::Op::Op##floatBinOp;                                          \
    }                                                                          \
  } break

#define BIN_OP_CASE_SINT_UINT_FLOAT(kind, sintBinOp, uintBinOp, floatBinOp)    \
                                                                               \
  case BO_##kind: {                                                            \
    if (isSintType) {                                                          \
      return spv::Op::Op##sintBinOp;                                           \
    }                                                                          \
    if (isUintType) {                                                          \
      return spv::Op::Op##uintBinOp;                                           \
    }                                                                          \
    if (isFloatType) {                                                         \
      return spv::Op::Op##floatBinOp;                                          \
    }                                                                          \
  } break

#define BIN_OP_CASE_SINT_UINT(kind, sintBinOp, uintBinOp)                      \
                                                                               \
  case BO_##kind: {                                                            \
    if (isSintType) {                                                          \
      return spv::Op::Op##sintBinOp;                                           \
    }                                                                          \
    if (isUintType) {                                                          \
      return spv::Op::Op##uintBinOp;                                           \
    }                                                                          \
  } break

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

  emitError("translating binary operator '%0' unimplemented", {})
      << BinaryOperator::getOpcodeStr(op);
  return spv::Op::OpNop;
}

SpirvEvalInfo SPIRVEmitter::processAssignment(const Expr *lhs,
                                              const SpirvEvalInfo &rhs,
                                              const bool isCompoundAssignment,
                                              SpirvEvalInfo lhsPtr) {
  lhs = lhs->IgnoreParenNoopCasts(astContext);

  // Assigning to vector swizzling should be handled differently.
  if (SpirvEvalInfo result = tryToAssignToVectorElements(lhs, rhs))
    return result;

  // Assigning to matrix swizzling should be handled differently.
  if (SpirvEvalInfo result = tryToAssignToMatrixElements(lhs, rhs))
    return result;

  // Assigning to a RWBuffer/RWTexture should be handled differently.
  if (SpirvEvalInfo result = tryToAssignToRWBufferRWTexture(lhs, rhs))
    return result;

  // Normal assignment procedure

  if (!lhsPtr)
    lhsPtr = doExpr(lhs);

  storeValue(lhsPtr, rhs, lhs->getType());

  // Plain assignment returns a rvalue, while compound assignment returns
  // lvalue.
  return isCompoundAssignment ? lhsPtr : rhs;
}

void SPIRVEmitter::storeValue(const SpirvEvalInfo &lhsPtr,
                              const SpirvEvalInfo &rhsVal,
                              QualType lhsValType) {
  if (const auto *refType = lhsValType->getAs<ReferenceType>())
    lhsValType = refType->getPointeeType();

  QualType matElemType = {};
  const bool lhsIsMat = typeTranslator.isMxNMatrix(lhsValType, &matElemType);
  const bool lhsIsFloatMat = lhsIsMat && matElemType->isFloatingType();
  const bool lhsIsNonFpMat = lhsIsMat && !matElemType->isFloatingType();

  if (typeTranslator.isScalarType(lhsValType) ||
      typeTranslator.isVectorType(lhsValType) || lhsIsFloatMat) {
    uint32_t rhsValId = rhsVal;

    // Special-case: According to the SPIR-V Spec: There is no physical size
    // or bit pattern defined for boolean type. Therefore an unsigned integer
    // is used to represent booleans when layout is required. In such cases,
    // we should cast the boolean to uint before creating OpStore.
    if (isBoolOrVecOfBoolType(lhsValType) &&
        lhsPtr.getLayoutRule() != LayoutRule::Void) {
      uint32_t vecSize = 1;
      const bool isVec =
          TypeTranslator::isVectorType(lhsValType, nullptr, &vecSize);
      const auto toType =
          isVec ? astContext.getExtVectorType(astContext.UnsignedIntTy, vecSize)
                : astContext.UnsignedIntTy;
      const auto fromType =
          isVec ? astContext.getExtVectorType(astContext.BoolTy, vecSize)
                : astContext.BoolTy;
      rhsValId = castToInt(rhsValId, fromType, toType, {});
    }

    theBuilder.createStore(lhsPtr, rhsValId);
  } else if (TypeTranslator::isOpaqueType(lhsValType)) {
    // Resource types are represented using RecordType in the AST.
    // Handle them before the general RecordType.
    //
    // HLSL allows to put resource types that translating into SPIR-V opaque
    // types in structs, or assign to variables of resource types. These can all
    // result in illegal SPIR-V for Vulkan. We just translate here literally and
    // let SPIRV-Tools opt to do the legalization work.
    //
    // Note: legalization specific code
    theBuilder.createStore(lhsPtr, rhsVal);
    needsLegalization = true;
  } else if (TypeTranslator::isAKindOfStructuredOrByteBuffer(lhsValType)) {
    // The rhs should be a pointer and the lhs should be a pointer-to-pointer.
    // Directly store the pointer here and let SPIRV-Tools opt to do the clean
    // up.
    //
    // Note: legalization specific code
    theBuilder.createStore(lhsPtr, rhsVal);
    needsLegalization = true;

    // For ConstantBuffers/TextureBuffers, we decompose and assign each field
    // recursively like normal structs using the following logic.
    //
    // The frontend forbids declaring ConstantBuffer<T> or TextureBuffer<T>
    // variables as function parameters/returns/variables, but happily accepts
    // assignments/returns from ConstantBuffer<T>/TextureBuffer<T> to function
    // parameters/returns/variables of type T. And ConstantBuffer<T> is not
    // represented differently as struct T.
  } else if (TypeTranslator::isOpaqueArrayType(lhsValType)) {
    // For opaque array types, we cannot perform OpLoad on the whole array and
    // then write out as a whole; instead, we need to OpLoad each element
    // using access chains. This is to influence later SPIR-V transformations
    // to use access chains to access each opaque object; if we do array
    // wholesale handling here, they will be in the final transformed code.
    // Drivers don't like that.
    // TODO: consider moving this hack into SPIRV-Tools as a transformation.
    assert(lhsValType->isConstantArrayType());
    assert(!rhsVal.isRValue());

    const auto *arrayType = astContext.getAsConstantArrayType(lhsValType);
    const auto elemType = arrayType->getElementType();
    const auto arraySize =
        static_cast<uint32_t>(arrayType->getSize().getZExtValue());

    // Do separate load of each element via access chain
    llvm::SmallVector<uint32_t, 8> elements;
    for (uint32_t i = 0; i < arraySize; ++i) {
      const auto subRhsValType =
          typeTranslator.translateType(elemType, rhsVal.getLayoutRule());
      const auto subRhsPtrType =
          theBuilder.getPointerType(subRhsValType, rhsVal.getStorageClass());
      const auto subRhsPtr = theBuilder.createAccessChain(
          subRhsPtrType, rhsVal, {theBuilder.getConstantInt32(i)});

      elements.push_back(theBuilder.createLoad(subRhsValType, subRhsPtr));
    }

    // Create a new composite and write out once
    const auto lhsValTypeId =
        typeTranslator.translateType(lhsValType, lhsPtr.getLayoutRule());
    theBuilder.createStore(
        lhsPtr, theBuilder.createCompositeConstruct(lhsValTypeId, elements));
  } else if (lhsPtr.getLayoutRule() == rhsVal.getLayoutRule()) {
    // If lhs and rhs has the same memory layout, we should be safe to load
    // from rhs and directly store into lhs and avoid decomposing rhs.
    // Note: this check should happen after those setting needsLegalization.
    // TODO: is this optimization always correct?
    theBuilder.createStore(lhsPtr, rhsVal);
  } else if (lhsValType->isRecordType() || lhsValType->isConstantArrayType() ||
             lhsIsNonFpMat) {
    theBuilder.createStore(
        lhsPtr, reconstructValue(rhsVal, lhsValType, lhsPtr.getLayoutRule()));
  } else {
    emitError("storing value of type %0 unimplemented", {}) << lhsValType;
  }
}

uint32_t SPIRVEmitter::reconstructValue(const SpirvEvalInfo &srcVal,
                                        const QualType valType,
                                        LayoutRule dstLR) {
  // Lambda for casting scalar or vector of bool<-->uint in cases where one side
  // of the reconstruction (lhs or rhs) has a layout rule.
  const auto handleBooleanLayout = [this, &srcVal, dstLR](uint32_t val,
                                                          QualType valType) {
    // We only need to cast if we have a scalar or vector of booleans.
    if (!isBoolOrVecOfBoolType(valType))
      return val;

    LayoutRule srcLR = srcVal.getLayoutRule();
    // Source value has a layout rule, and has therefore been represented
    // as a uint. Cast it to boolean before using.
    bool shouldCastToBool =
        srcLR != LayoutRule::Void && dstLR == LayoutRule::Void;
    // Destination has a layout rule, and should therefore be represented
    // as a uint. Cast to uint before using.
    bool shouldCastToUint =
        srcLR == LayoutRule::Void && dstLR != LayoutRule::Void;
    // No boolean layout issues to take care of.
    if (!shouldCastToBool && !shouldCastToUint)
      return val;

    uint32_t vecSize = 1;
    TypeTranslator::isVectorType(valType, nullptr, &vecSize);
    QualType boolType =
        vecSize == 1 ? astContext.BoolTy
                     : astContext.getExtVectorType(astContext.BoolTy, vecSize);
    QualType uintType =
        vecSize == 1
            ? astContext.UnsignedIntTy
            : astContext.getExtVectorType(astContext.UnsignedIntTy, vecSize);

    if (shouldCastToBool)
      return castToBool(val, uintType, boolType);
    if (shouldCastToUint)
      return castToInt(val, boolType, uintType, {});

    return val;
  };

  // Lambda for cases where we want to reconstruct an array
  const auto reconstructArray = [this, &srcVal, valType,
                                 dstLR](uint32_t arraySize,
                                        QualType arrayElemType) {
    llvm::SmallVector<uint32_t, 4> elements;
    for (uint32_t i = 0; i < arraySize; ++i) {
      const auto subSrcValType =
          typeTranslator.translateType(arrayElemType, srcVal.getLayoutRule());
      const auto subSrcVal =
          theBuilder.createCompositeExtract(subSrcValType, srcVal, {i});
      elements.push_back(reconstructValue(srcVal.substResultId(subSrcVal),
                                          arrayElemType, dstLR));
    }
    const auto dstValType = typeTranslator.translateType(valType, dstLR);
    return theBuilder.createCompositeConstruct(dstValType, elements);
  };

  // Constant arrays
  if (const auto *arrayType = astContext.getAsConstantArrayType(valType)) {
    const auto elemType = arrayType->getElementType();
    const auto size =
        static_cast<uint32_t>(arrayType->getSize().getZExtValue());
    return reconstructArray(size, elemType);
  }

  // Non-floating-point matrices
  QualType matElemType = {};
  uint32_t numRows = 0, numCols = 0;
  const bool isNonFpMat =
      typeTranslator.isMxNMatrix(valType, &matElemType, &numRows, &numCols) &&
      !matElemType->isFloatingType();

  if (isNonFpMat) {
    // Note: This check should happen before the RecordType check.
    // Non-fp matrices are represented as arrays of vectors in SPIR-V.
    // Each array element is a vector. Get the QualType for the vector.
    const auto elemType = astContext.getExtVectorType(matElemType, numCols);
    return reconstructArray(numRows, elemType);
  }

  // Note: This check should happen before the RecordType check since
  // vector/matrix/resource types are represented as RecordType in the AST.
  if (hlsl::IsHLSLVecMatType(valType) || hlsl::IsHLSLResourceType(valType))
    return handleBooleanLayout(srcVal, valType);

  // Structs
  if (const auto *recordType = valType->getAs<RecordType>()) {
    uint32_t index = 0;
    llvm::SmallVector<uint32_t, 4> elements;
    for (const auto *field : recordType->getDecl()->fields()) {
      const auto subSrcValType = typeTranslator.translateType(
          field->getType(), srcVal.getLayoutRule());
      const auto subSrcVal =
          theBuilder.createCompositeExtract(subSrcValType, srcVal, {index});

      elements.push_back(reconstructValue(srcVal.substResultId(subSrcVal),
                                          field->getType(), dstLR));
      ++index;
    }
    const auto dstValType = typeTranslator.translateType(valType, dstLR);
    return theBuilder.createCompositeConstruct(dstValType, elements);
  }

  return handleBooleanLayout(srcVal, valType);
}

SpirvEvalInfo SPIRVEmitter::processBinaryOp(const Expr *lhs, const Expr *rhs,
                                            const BinaryOperatorKind opcode,
                                            const QualType computationType,
                                            const QualType resultType,
                                            SourceRange sourceRange,
                                            SpirvEvalInfo *lhsInfo,
                                            const spv::Op mandateGenOpcode) {
  const QualType lhsType = lhs->getType();
  const QualType rhsType = rhs->getType();

  // Binary logical operations (such as ==, !=, etc) that return a boolean type
  // may get a literal (e.g. 0, 1, etc.) as lhs or rhs args. Since only
  // non-zero-ness of these literals matter, they can be translated as 32-bits.
  TypeTranslator::LiteralTypeHint hint(typeTranslator);
  if (resultType->isBooleanType()) {
    if (lhsType->isSpecificBuiltinType(BuiltinType::LitInt) ||
        rhsType->isSpecificBuiltinType(BuiltinType::LitInt))
      hint.setHint(astContext.IntTy);
    if (lhsType->isSpecificBuiltinType(BuiltinType::LitFloat) ||
        rhsType->isSpecificBuiltinType(BuiltinType::LitFloat))
      hint.setHint(astContext.FloatTy);
  }

  // If the operands are of matrix type, we need to dispatch the operation
  // onto each element vector iff the operands are not degenerated matrices
  // and we don't have a matrix specific SPIR-V instruction for the operation.
  if (!isSpirvMatrixOp(mandateGenOpcode) &&
      TypeTranslator::isMxNMatrix(lhsType)) {
    return processMatrixBinaryOp(lhs, rhs, opcode, sourceRange);
  }

  // Comma operator works differently from other binary operations as there is
  // no SPIR-V instruction for it. For each comma, we must evaluate lhs and rhs
  // respectively, and return the results of rhs.
  if (opcode == BO_Comma) {
    (void)doExpr(lhs);
    return doExpr(rhs);
  }

  SpirvEvalInfo rhsVal = 0, lhsPtr = 0, lhsVal = 0;

  if (BinaryOperator::isCompoundAssignmentOp(opcode)) {
    // Evalute rhs before lhs
    rhsVal = loadIfGLValue(rhs);
    lhsVal = lhsPtr = doExpr(lhs);
    // This is a compound assignment. We need to load the lhs value if lhs
    // is not already rvalue and does not generate a vector shuffle.
    if (!lhsPtr.isRValue() && !isVectorShuffle(lhs)) {
      lhsVal = loadIfGLValue(lhs, lhsPtr);
    }
    // For a compound assignments, the AST does not have the proper implicit
    // cast if lhs and rhs have different types. So we need to manually cast lhs
    // to the computation type.
    if (computationType != lhsType)
      lhsVal.setResultId(
          castToType(lhsVal, lhsType, computationType, lhs->getExprLoc()));
  } else {
    // Evalute lhs before rhs
    lhsPtr = doExpr(lhs);
    lhsVal = loadIfGLValue(lhs, lhsPtr);
    rhsVal = loadIfGLValue(rhs);
  }

  if (lhsInfo)
    *lhsInfo = lhsPtr;

  const spv::Op spvOp = (mandateGenOpcode == spv::Op::Max)
                            ? translateOp(opcode, computationType)
                            : mandateGenOpcode;

  switch (opcode) {
  case BO_Shl:
  case BO_Shr:
  case BO_ShlAssign:
  case BO_ShrAssign:
    // We need to cull the RHS to make sure that we are not shifting by an
    // amount that is larger than the bitwidth of the LHS.
    rhsVal.setResultId(theBuilder.createBinaryOp(
        spv::Op::OpBitwiseAnd, typeTranslator.translateType(computationType),
        rhsVal, getMaskForBitwidthValue(rhsType)));
    // Fall through
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
  case BO_LAnd:
  case BO_LOr:
  case BO_AddAssign:
  case BO_SubAssign:
  case BO_MulAssign:
  case BO_DivAssign:
  case BO_RemAssign:
  case BO_AndAssign:
  case BO_OrAssign:
  case BO_XorAssign: {

    // To evaluate this expression as an OpSpecConstantOp, we need to make sure
    // both operands are constant and at least one of them is a spec constant.
    if (lhsVal.isConstant() && rhsVal.isConstant() &&
        (lhsVal.isSpecConstant() || rhsVal.isSpecConstant()) &&
        isAcceptedSpecConstantBinaryOp(spvOp)) {
      const auto valId = theBuilder.createSpecConstantBinaryOp(
          spvOp, typeTranslator.translateType(resultType), lhsVal, rhsVal);
      return SpirvEvalInfo(valId).setRValue().setSpecConstant();
    }

    // Normal binary operation
    uint32_t valId = 0;
    if (BinaryOperator::isCompoundAssignmentOp(opcode)) {
      valId = theBuilder.createBinaryOp(
          spvOp, typeTranslator.translateType(computationType), lhsVal, rhsVal);
      // For a compound assignments, the AST does not have the proper implicit
      // cast if lhs and rhs have different types. So we need to manually cast
      // the result back to lhs' type.
      if (computationType != lhsType)
        valId = castToType(valId, computationType, lhsType, lhs->getExprLoc());
    } else {
      valId = theBuilder.createBinaryOp(
          spvOp, typeTranslator.translateType(resultType), lhsVal, rhsVal);
    }

    auto result = SpirvEvalInfo(valId).setRValue();

    // Propagate RelaxedPrecision
    if (lhsVal.isRelaxedPrecision() || rhsVal.isRelaxedPrecision())
      result.setRelaxedPrecision();
    // Propagate NonUniformEXT
    if (lhsVal.isNonUniform() || rhsVal.isNonUniform())
      result.setNonUniform();

    return result;
  }
  case BO_Assign:
    llvm_unreachable("assignment should not be handled here");
    break;
  case BO_PtrMemD:
  case BO_PtrMemI:
  case BO_Comma:
    // Unimplemented
    break;
  }

  emitError("binary operator '%0' unimplemented", lhs->getExprLoc())
      << BinaryOperator::getOpcodeStr(opcode) << sourceRange;
  return 0;
}

void SPIRVEmitter::initOnce(QualType varType, std::string varName,
                            uint32_t varPtr, const Expr *varInit) {
  // For uninitialized resource objects, we do nothing since there is no
  // meaningful zero values for them.
  if (!varInit && hlsl::IsHLSLResourceType(varType))
    return;

  const uint32_t boolType = theBuilder.getBoolType();
  varName = "init.done." + varName;

  // Create a file/module visible variable to hold the initialization state.
  const uint32_t initDoneVar =
      theBuilder.addModuleVar(boolType, spv::StorageClass::Private, varName,
                              theBuilder.getConstantBool(false));

  const uint32_t condition = theBuilder.createLoad(boolType, initDoneVar);

  const uint32_t todoBB = theBuilder.createBasicBlock("if.init.todo");
  const uint32_t doneBB = theBuilder.createBasicBlock("if.init.done");

  // If initDoneVar contains true, we jump to the "done" basic block; otherwise,
  // jump to the "todo" basic block.
  theBuilder.createConditionalBranch(condition, doneBB, todoBB, doneBB);
  theBuilder.addSuccessor(todoBB);
  theBuilder.addSuccessor(doneBB);
  theBuilder.setMergeTarget(doneBB);

  theBuilder.setInsertPoint(todoBB);
  // Do initialization and mark done
  if (varInit) {
    storeValue(
        // Static function variable are of private storage class
        SpirvEvalInfo(varPtr).setStorageClass(spv::StorageClass::Private),
        doExpr(varInit), varInit->getType());
  } else {
    const auto typeId = typeTranslator.translateType(varType);
    theBuilder.createStore(varPtr, theBuilder.getConstantNull(typeId));
  }
  theBuilder.createStore(initDoneVar, theBuilder.getConstantBool(true));
  theBuilder.createBranch(doneBB);
  theBuilder.addSuccessor(doneBB);

  theBuilder.setInsertPoint(doneBB);
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
  SpirvEvalInfo scalarVal = 0;

  // Try to evaluate the element as constant first. If successful, then we
  // can generate constant instructions for this vector splat.
  if ((scalarVal = tryToEvaluateAsConst(scalarExpr))) {
    isConstVal = true;
  } else {
    scalarVal = doExpr(scalarExpr);
  }

  if (size == 1) {
    // Just return the scalar value for vector splat with size 1.
    // Note that can be used as an lvalue, so we need to carry over
    // the lvalueness for non-constant cases.
    return isConstVal ? scalarVal.setConstant().setRValue() : scalarVal;
  }

  const uint32_t vecType = theBuilder.getVecType(
      typeTranslator.translateType(scalarExpr->getType()), size);
  llvm::SmallVector<uint32_t, 4> elements(size_t(size), scalarVal);

  // TODO: we are saying the constant has Function storage class here.
  // Should find a more meaningful one.
  if (isConstVal) {
    const auto valueId = theBuilder.getConstantComposite(vecType, elements);
    return SpirvEvalInfo(valueId).setConstant().setRValue();
  } else {
    const auto valueId = theBuilder.createCompositeConstruct(vecType, elements);
    return SpirvEvalInfo(valueId).setRValue();
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
  const SourceRange range = expr->getSourceRange();

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
        const QualType vecType = expr->getType();
        if (isa<CompoundAssignOperator>(expr)) {
          SpirvEvalInfo lhsPtr = 0;
          const auto result = processBinaryOp(
              lhs, cast->getSubExpr(), expr->getOpcode(), vecType, vecType,
              range, &lhsPtr, spv::Op::OpVectorTimesScalar);
          return processAssignment(lhs, result, true, lhsPtr);
        } else {
          return processBinaryOp(lhs, cast->getSubExpr(), expr->getOpcode(),
                                 vecType, vecType, range, nullptr,
                                 spv::Op::OpVectorTimesScalar);
        }
      }
    }
  }

  // scalar * vector
  if (hlsl::IsHLSLVecType(rhs->getType())) {
    if (const auto *cast = dyn_cast<ImplicitCastExpr>(lhs)) {
      if (cast->getCastKind() == CK_HLSLVectorSplat) {
        const QualType vecType = expr->getType();
        // We need to switch the positions of lhs and rhs here because
        // OpVectorTimesScalar requires the first operand to be a vector and
        // the second to be a scalar.
        return processBinaryOp(rhs, cast->getSubExpr(), expr->getOpcode(),
                               vecType, vecType, range, nullptr,
                               spv::Op::OpVectorTimesScalar);
      }
    }
  }

  return 0;
}

SpirvEvalInfo
SPIRVEmitter::tryToGenFloatMatrixScale(const BinaryOperator *expr) {
  const QualType type = expr->getType();
  const SourceRange range = expr->getSourceRange();

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
        const QualType matType = expr->getType();
        const spv::Op opcode = selectOpcode(lhsType);
        if (isa<CompoundAssignOperator>(expr)) {
          SpirvEvalInfo lhsPtr = 0;
          const auto result =
              processBinaryOp(lhs, cast->getSubExpr(), expr->getOpcode(),
                              matType, matType, range, &lhsPtr, opcode);
          return processAssignment(lhs, result, true, lhsPtr);
        } else {
          return processBinaryOp(lhs, cast->getSubExpr(), expr->getOpcode(),
                                 matType, matType, range, nullptr, opcode);
        }
      }
    }
  }

  // scalar * matrix
  if (hlsl::IsHLSLMatType(rhsType)) {
    if (const auto *cast = dyn_cast<ImplicitCastExpr>(lhs)) {
      if (cast->getCastKind() == CK_HLSLMatrixSplat) {
        const QualType matType = expr->getType();
        const spv::Op opcode = selectOpcode(rhsType);
        // We need to switch the positions of lhs and rhs here because
        // OpMatrixTimesScalar requires the first operand to be a matrix and
        // the second to be a scalar.
        return processBinaryOp(rhs, cast->getSubExpr(), expr->getOpcode(),
                               matType, matType, range, nullptr, opcode);
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

  // Special case for <scalar-value>.x, which will have an AST of
  // HLSLVectorElementExpr whose base is an ImplicitCastExpr
  // (CK_HLSLVectorSplat). We just need to assign to <scalar-value>
  // for such case.
  if (const auto *baseCast = dyn_cast<CastExpr>(lhsExpr->getBase()))
    if (baseCast->getCastKind() == CastKind::CK_HLSLVectorSplat &&
        hlsl::GetHLSLVecSize(baseCast->getType()) == 1)
      return processAssignment(baseCast->getSubExpr(), rhs, false);

  const Expr *base = nullptr;
  hlsl::VectorMemberAccessPositions accessor;
  condenseVectorElementExpr(lhsExpr, &base, &accessor);

  const QualType baseType = base->getType();
  assert(hlsl::IsHLSLVecType(baseType));
  const uint32_t baseTypeId = typeTranslator.translateType(baseType);
  const auto baseSize = hlsl::GetHLSLVecSize(baseType);
  const auto accessorSize = accessor.Count;
  // Whether selecting the whole original vector
  bool isSelectOrigin = accessorSize == baseSize;

  // Assigning to one component
  if (accessorSize == 1) {
    if (isBufferTextureIndexing(dyn_cast_or_null<CXXOperatorCallExpr>(base))) {
      // Assigning to one component of a RWBuffer/RWTexture element
      // We need to use OpImageWrite here.
      // Compose the new vector value first
      const uint32_t oldVec = doExpr(base);
      const uint32_t newVec = theBuilder.createCompositeInsert(
          baseTypeId, oldVec, {accessor.Swz0}, rhs);
      const auto result = tryToAssignToRWBufferRWTexture(base, newVec);
      assert(result); // Definitely RWBuffer/RWTexture assignment
      (void)result;
      return rhs;     // TODO: incorrect for compound assignments
    } else {
      // Assigning to one normal vector component. Nothing special, just fall
      // back to the normal CodeGen path.
      return 0;
    }
  }

  if (isSelectOrigin) {
    for (uint32_t i = 0; i < accessorSize; ++i) {
      uint32_t position;
      accessor.GetPosition(i, &position);
      if (position != i)
        isSelectOrigin = false;
    }
  }

  // Assigning to the original vector
  if (isSelectOrigin) {
    // Ignore this HLSLVectorElementExpr and dispatch to base
    return processAssignment(base, rhs, false);
  }

  llvm::SmallVector<uint32_t, 4> selectors;
  selectors.resize(baseSize);
  // Assume we are selecting all original elements first.
  for (uint32_t i = 0; i < baseSize; ++i) {
    selectors[i] = i;
  }

  // Now fix up the elements that actually got overwritten by the rhs vector.
  // Since we are using the rhs vector as the second vector, their index
  // should be offset'ed by the size of the lhs base vector.
  for (uint32_t i = 0; i < accessor.Count; ++i) {
    uint32_t position;
    accessor.GetPosition(i, &position);
    selectors[position] = baseSize + i;
  }

  const auto vec1 = doExpr(base);
  const uint32_t vec1Val = vec1.isRValue()
                               ? static_cast<uint32_t>(vec1)
                               : theBuilder.createLoad(baseTypeId, vec1);
  const uint32_t shuffle =
      theBuilder.createVectorShuffle(baseTypeId, vec1Val, rhs, selectors);

  if (!tryToAssignToRWBufferRWTexture(base, shuffle))
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
    const QualType imageType = baseExpr->getType();
    const auto baseInfo = doExpr(baseExpr);
    const uint32_t imageId = theBuilder.createLoad(
        typeTranslator.translateType(imageType), baseInfo);
    theBuilder.createImageWrite(imageType, imageId, locId, rhs);
    if (baseInfo.isNonUniform()) {
      // Decorate the image handle for OpImageWrite
      theBuilder.decorateNonUniformEXT(imageId);
    }
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
  const auto base = doExpr(baseMat);
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
        theBuilder.getPointerType(elemTypeId, base.getStorageClass());

    // If the lhs is actually a matrix of size 1x1, we don't need the access
    // chain. base is already the dest pointer.
    uint32_t lhsElemPtr = base;
    if (!indices.empty()) {
      assert(!base.isRValue());
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

SpirvEvalInfo SPIRVEmitter::processEachVectorInMatrix(
    const Expr *matrix, const uint32_t matrixVal,
    llvm::function_ref<uint32_t(uint32_t, uint32_t, uint32_t)>
        actOnEachVector) {
  const auto matType = matrix->getType();
  assert(TypeTranslator::isMxNMatrix(matType));
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
  const auto valId = theBuilder.createCompositeConstruct(
      typeTranslator.translateType(matType), vectors);
  return SpirvEvalInfo(valId).setRValue();
}

void SPIRVEmitter::createSpecConstant(const VarDecl *varDecl) {
  class SpecConstantEnvRAII {
  public:
    // Creates a new instance which sets mode to true on creation,
    // and resets mode to false on destruction.
    SpecConstantEnvRAII(bool *mode) : modeSlot(mode) { *modeSlot = true; }
    ~SpecConstantEnvRAII() { *modeSlot = false; }

  private:
    bool *modeSlot;
  };

  const QualType varType = varDecl->getType();

  bool hasError = false;

  if (!varDecl->isExternallyVisible()) {
    emitError("specialization constant must be externally visible",
              varDecl->getLocation());
    hasError = true;
  }

  if (const auto *builtinType = varType->getAs<BuiltinType>()) {
    switch (builtinType->getKind()) {
    case BuiltinType::Bool:
    case BuiltinType::Int:
    case BuiltinType::UInt:
    case BuiltinType::Float:
      break;
    default:
      emitError("unsupported specialization constant type",
                varDecl->getLocStart());
      hasError = true;
    }
  }

  const auto *init = varDecl->getInit();

  if (!init) {
    emitError("missing default value for specialization constant",
              varDecl->getLocation());
    hasError = true;
  } else if (!isAcceptedSpecConstantInit(init)) {
    emitError("unsupported specialization constant initializer",
              init->getLocStart())
        << init->getSourceRange();
    hasError = true;
  }

  if (hasError)
    return;

  SpecConstantEnvRAII specConstantEnvRAII(&isSpecConstantMode);

  const auto specConstant = doExpr(init);

  // We are not creating a variable to hold the spec constant, instead, we
  // translate the varDecl directly into the spec constant here.

  theBuilder.decorateSpecId(
      specConstant, varDecl->getAttr<VKConstantIdAttr>()->getSpecConstId());

  declIdMapper.registerSpecConstant(varDecl, specConstant);
}

SpirvEvalInfo
SPIRVEmitter::processMatrixBinaryOp(const Expr *lhs, const Expr *rhs,
                                    const BinaryOperatorKind opcode,
                                    SourceRange range) {
  // TODO: some code are duplicated from processBinaryOp. Try to unify them.
  const auto lhsType = lhs->getType();
  assert(TypeTranslator::isMxNMatrix(lhsType));
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
    const auto actOnEachVec = [this, spvOp, rhsVal](uint32_t index,
                                                    uint32_t vecType,
                                                    uint32_t lhsVec) {
      // For each vector of lhs, we need to load the corresponding vector of
      // rhs and do the operation on them.
      const uint32_t rhsVec =
          theBuilder.createCompositeExtract(vecType, rhsVal, {index});
      const auto valId =
          theBuilder.createBinaryOp(spvOp, vecType, lhsVec, rhsVec);
      return SpirvEvalInfo(valId).setRValue();
    };
    return processEachVectorInMatrix(lhs, lhsVal, actOnEachVec);
  }
  case BO_Assign:
    llvm_unreachable("assignment should not be handled here");
  default:
    break;
  }

  emitError("binary operator '%0' over matrix type unimplemented",
            lhs->getExprLoc())
      << BinaryOperator::getOpcodeStr(opcode) << range;
  return 0;
}

const Expr *SPIRVEmitter::collectArrayStructIndices(
    const Expr *expr, llvm::SmallVectorImpl<uint32_t> *indices, bool rawIndex) {
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
        indexing->getBase()->IgnoreParenNoopCasts(astContext), indices,
        rawIndex);

    // Append the index of the current level
    const auto *fieldDecl = cast<FieldDecl>(indexing->getMemberDecl());
    assert(fieldDecl);
    // If we are accessing a derived struct, we need to account for the number
    // of base structs, since they are placed as fields at the beginning of the
    // derived struct.
    const uint32_t index = getNumBaseClasses(indexing->getBase()->getType()) +
                           fieldDecl->getFieldIndex();
    indices->push_back(rawIndex ? index : theBuilder.getConstantInt32(index));

    return base;
  }

  // Provide a hint to the TypeTranslator that the integer literal used to
  // index into the following cases should be translated as a 32-bit integer.
  TypeTranslator::LiteralTypeHint hint(typeTranslator, astContext.IntTy);

  if (const auto *indexing = dyn_cast<ArraySubscriptExpr>(expr)) {
    if (rawIndex)
      return nullptr; // TODO: handle constant array index

    // The base of an ArraySubscriptExpr has a wrapping LValueToRValue implicit
    // cast. We need to ingore it to avoid creating OpLoad.
    const Expr *thisBase = indexing->getBase()->IgnoreParenLValueCasts();
    const Expr *base = collectArrayStructIndices(thisBase, indices, rawIndex);
    indices->push_back(doExpr(indexing->getIdx()));
    return base;
  }

  if (const auto *indexing = dyn_cast<CXXOperatorCallExpr>(expr))
    if (indexing->getOperator() == OverloadedOperatorKind::OO_Subscript) {
      if (rawIndex)
        return nullptr; // TODO: handle constant array index

      // If this is indexing into resources, we need specific OpImage*
      // instructions for accessing. Return directly to avoid further building
      // up the access chain.
      if (isBufferTextureIndexing(indexing))
        return indexing;

      const Expr *thisBase =
          indexing->getArg(0)->IgnoreParenNoopCasts(astContext);

      const auto thisBaseType = thisBase->getType();
      const Expr *base = collectArrayStructIndices(thisBase, indices, rawIndex);

      if (thisBaseType != base->getType() &&
          TypeTranslator::isAKindOfStructuredOrByteBuffer(thisBaseType)) {
        // The immediate base is a kind of structured or byte buffer. It should
        // be an alias variable. Break the normal index collecting chain.
        // Return the immediate base as the base so that we can apply other
        // hacks for legalization over it.
        //
        // Note: legalization specific code
        indices->clear();
        base = thisBase;
      }

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
      if (rawIndex)
        return nullptr; // TODO: handle constant array index

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

SpirvEvalInfo &SPIRVEmitter::turnIntoElementPtr(
    SpirvEvalInfo &info, QualType elemType,
    const llvm::SmallVector<uint32_t, 4> &indices) {
  assert(!info.isRValue());
  const uint32_t ptrType = theBuilder.getPointerType(
      typeTranslator.translateType(elemType, info.getLayoutRule()),
      info.getStorageClass());
  return info.setResultId(theBuilder.createAccessChain(ptrType, info, indices));
}

uint32_t SPIRVEmitter::castToBool(const uint32_t fromVal, QualType fromType,
                                  QualType toBoolType) {
  if (TypeTranslator::isSameScalarOrVecType(fromType, toBoolType))
    return fromVal;

  const uint32_t boolType = typeTranslator.translateType(toBoolType);

  { // Special case handling for converting to a matrix of booleans.
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (TypeTranslator::isMxNMatrix(fromType, &elemType, &rowCount,
                                    &colCount)) {
      const auto fromRowQualType =
          astContext.getExtVectorType(elemType, colCount);
      const auto fromRowQualTypeId =
          typeTranslator.translateType(fromRowQualType);
      const auto toBoolRowQualType =
          astContext.getExtVectorType(astContext.BoolTy, colCount);
      llvm::SmallVector<uint32_t, 4> rows;
      for (uint32_t i = 0; i < rowCount; ++i) {
        const auto row =
            theBuilder.createCompositeExtract(fromRowQualTypeId, fromVal, {i});
        rows.push_back(castToBool(row, fromRowQualType, toBoolRowQualType));
      }
      return theBuilder.createCompositeConstruct(boolType, rows);
    }
  }

  // Converting to bool means comparing with value zero.
  const spv::Op spvOp = translateOp(BO_NE, fromType);
  const uint32_t zeroVal = getValueZero(fromType);
  return theBuilder.createBinaryOp(spvOp, boolType, fromVal, zeroVal);
}

uint32_t SPIRVEmitter::castToInt(uint32_t fromVal, QualType fromType,
                                 QualType toIntType, SourceLocation srcLoc) {
  if (TypeTranslator::isSameScalarOrVecType(fromType, toIntType))
    return fromVal;

  uint32_t intType = typeTranslator.translateType(toIntType);

  if (isBoolOrVecOfBoolType(fromType)) {
    const uint32_t one = getValueOne(toIntType);
    const uint32_t zero = getValueZero(toIntType);
    return theBuilder.createSelect(intType, fromVal, one, zero);
  }

  if (isSintOrVecOfSintType(fromType) || isUintOrVecOfUintType(fromType)) {
    // First convert the source to the bitwidth of the destination if necessary.
    uint32_t convertedType = 0;
    fromVal = convertBitwidth(fromVal, fromType, toIntType, &convertedType);
    // If bitwidth conversion was the only thing we needed to do, we're done.
    if (convertedType == typeTranslator.translateType(toIntType))
      return fromVal;
    return theBuilder.createUnaryOp(spv::Op::OpBitcast, intType, fromVal);
  }

  if (isFloatOrVecOfFloatType(fromType)) {
    // First convert the source to the bitwidth of the destination if necessary.
    fromVal = convertBitwidth(fromVal, fromType, toIntType);
    if (isSintOrVecOfSintType(toIntType)) {
      return theBuilder.createUnaryOp(spv::Op::OpConvertFToS, intType, fromVal);
    } else if (isUintOrVecOfUintType(toIntType)) {
      return theBuilder.createUnaryOp(spv::Op::OpConvertFToU, intType, fromVal);
    } else {
      emitError("casting from floating point to integer unimplemented", srcLoc);
    }
  }

  {
    QualType elemType = {};
    uint32_t numRows = 0, numCols = 0;
    if (TypeTranslator::isMxNMatrix(fromType, &elemType, &numRows, &numCols)) {
      // The source matrix and the target matrix must have the same dimensions.
      QualType toElemType = {};
      uint32_t toNumRows = 0, toNumCols = 0;
      const bool isMat = TypeTranslator::isMxNMatrix(toIntType, &toElemType,
                                                     &toNumRows, &toNumCols);
      assert(isMat && numRows == toNumRows && numCols == toNumCols);
      (void)isMat;
      (void)toNumRows;
      (void)toNumCols;

      // Casting to a matrix of integers: Cast each row and construct a
      // composite.
      llvm::SmallVector<uint32_t, 4> castedRows;
      const uint32_t vecType = typeTranslator.getComponentVectorType(fromType);
      const auto fromVecQualType =
          astContext.getExtVectorType(elemType, numCols);
      const auto toIntVecQualType =
          astContext.getExtVectorType(toElemType, numCols);
      for (uint32_t row = 0; row < numRows; ++row) {
        const auto rowId =
            theBuilder.createCompositeExtract(vecType, fromVal, {row});
        castedRows.push_back(
            castToInt(rowId, fromVecQualType, toIntVecQualType, srcLoc));
      }
      return theBuilder.createCompositeConstruct(intType, castedRows);
    }
  }

  return 0;
}

uint32_t SPIRVEmitter::convertBitwidth(uint32_t fromVal, QualType fromType,
                                       QualType toType, uint32_t *resultType) {
  // At the moment, we will not make bitwidth conversions for literal int and
  // literal float types because they always indicate 64-bit and do not
  // represent what SPIR-V was actually resolved to.
  // TODO: If the evaluated type is added to SpirvEvalInfo, change 'fromVal' to
  // SpirvEvalInfo and use it to handle literal types more accurately.
  if (fromType->isSpecificBuiltinType(BuiltinType::LitFloat) ||
      fromType->isSpecificBuiltinType(BuiltinType::LitInt))
    return fromVal;

  const auto fromBitwidth = typeTranslator.getElementSpirvBitwidth(fromType);
  const auto toBitwidth = typeTranslator.getElementSpirvBitwidth(toType);
  if (fromBitwidth == toBitwidth) {
    if (resultType)
      *resultType = typeTranslator.translateType(fromType);
    return fromVal;
  }

  // We want the 'fromType' with the 'toBitwidth'.
  const uint32_t targetTypeId =
      typeTranslator.getTypeWithCustomBitwidth(fromType, toBitwidth);
  if (resultType)
    *resultType = targetTypeId;

  if (isFloatOrVecOfFloatType(fromType))
    return theBuilder.createUnaryOp(spv::Op::OpFConvert, targetTypeId, fromVal);
  if (isSintOrVecOfSintType(fromType))
    return theBuilder.createUnaryOp(spv::Op::OpSConvert, targetTypeId, fromVal);
  if (isUintOrVecOfUintType(fromType))
    return theBuilder.createUnaryOp(spv::Op::OpUConvert, targetTypeId, fromVal);
  llvm_unreachable("invalid type passed to convertBitwidth");
}

uint32_t SPIRVEmitter::castToFloat(uint32_t fromVal, QualType fromType,
                                   QualType toFloatType,
                                   SourceLocation srcLoc) {
  if (TypeTranslator::isSameScalarOrVecType(fromType, toFloatType))
    return fromVal;

  const uint32_t floatType = typeTranslator.translateType(toFloatType);

  if (isBoolOrVecOfBoolType(fromType)) {
    const uint32_t one = getValueOne(toFloatType);
    const uint32_t zero = getValueZero(toFloatType);
    return theBuilder.createSelect(floatType, fromVal, one, zero);
  }

  if (isSintOrVecOfSintType(fromType)) {
    // First convert the source to the bitwidth of the destination if necessary.
    fromVal = convertBitwidth(fromVal, fromType, toFloatType);
    return theBuilder.createUnaryOp(spv::Op::OpConvertSToF, floatType, fromVal);
  }

  if (isUintOrVecOfUintType(fromType)) {
    // First convert the source to the bitwidth of the destination if necessary.
    fromVal = convertBitwidth(fromVal, fromType, toFloatType);
    return theBuilder.createUnaryOp(spv::Op::OpConvertUToF, floatType, fromVal);
  }

  if (isFloatOrVecOfFloatType(fromType)) {
    // This is the case of float to float conversion with different bitwidths.
    return convertBitwidth(fromVal, fromType, toFloatType);
  }

  // Casting matrix types
  {
    QualType elemType = {};
    uint32_t numRows = 0, numCols = 0;
    if (TypeTranslator::isMxNMatrix(fromType, &elemType, &numRows, &numCols)) {
      // The source matrix and the target matrix must have the same dimensions.
      QualType toElemType = {};
      uint32_t toNumRows = 0, toNumCols = 0;
      const auto isMat = TypeTranslator::isMxNMatrix(toFloatType, &toElemType,
                                                     &toNumRows, &toNumCols);
      assert(isMat && numRows == toNumRows && numCols == toNumCols);
      (void)isMat;
      (void)toNumRows;
      (void)toNumCols;

      // Casting to a matrix of floats: Cast each row and construct a
      // composite.
      llvm::SmallVector<uint32_t, 4> castedRows;
      const uint32_t vecType = typeTranslator.getComponentVectorType(fromType);
      const auto fromVecQualType =
          astContext.getExtVectorType(elemType, numCols);
      const auto toIntVecQualType =
          astContext.getExtVectorType(toElemType, numCols);
      for (uint32_t row = 0; row < numRows; ++row) {
        const auto rowId =
            theBuilder.createCompositeExtract(vecType, fromVal, {row});
        castedRows.push_back(
            castToFloat(rowId, fromVecQualType, toIntVecQualType, srcLoc));
      }
      return theBuilder.createCompositeConstruct(floatType, castedRows);
    }
  }

  emitError("casting to floating point unimplemented", srcLoc);
  return 0;
}

SpirvEvalInfo SPIRVEmitter::processIntrinsicCallExpr(const CallExpr *callExpr) {
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

  uint32_t retVal = 0;

#define INTRINSIC_SPIRV_OP_WITH_CAP_CASE(intrinsicOp, spirvOp, doEachVec, cap) \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    theBuilder.requireCapability(cap);                                         \
    retVal = processIntrinsicUsingSpirvInst(callExpr, spv::Op::Op##spirvOp,    \
                                            doEachVec);                        \
  } break

#define INTRINSIC_SPIRV_OP_CASE(intrinsicOp, spirvOp, doEachVec)               \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    retVal = processIntrinsicUsingSpirvInst(callExpr, spv::Op::Op##spirvOp,    \
                                            doEachVec);                        \
  } break

#define INTRINSIC_OP_CASE(intrinsicOp, glslOp, doEachVec)                      \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = GLSLstd450::GLSLstd450##glslOp;                               \
    retVal = processIntrinsicUsingGLSLInst(callExpr, glslOpcode, doEachVec);   \
  } break

#define INTRINSIC_OP_CASE_INT_FLOAT(intrinsicOp, glslIntOp, glslFloatOp,       \
                                    doEachVec)                                 \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = isFloatType ? GLSLstd450::GLSLstd450##glslFloatOp             \
                             : GLSLstd450::GLSLstd450##glslIntOp;              \
    retVal = processIntrinsicUsingGLSLInst(callExpr, glslOpcode, doEachVec);   \
  } break

#define INTRINSIC_OP_CASE_SINT_UINT(intrinsicOp, glslSintOp, glslUintOp,       \
                                    doEachVec)                                 \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = isSintType ? GLSLstd450::GLSLstd450##glslSintOp               \
                            : GLSLstd450::GLSLstd450##glslUintOp;              \
    retVal = processIntrinsicUsingGLSLInst(callExpr, glslOpcode, doEachVec);   \
  } break

#define INTRINSIC_OP_CASE_SINT_UINT_FLOAT(intrinsicOp, glslSintOp, glslUintOp, \
                                          glslFloatOp, doEachVec)              \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = isFloatType                                                   \
                     ? GLSLstd450::GLSLstd450##glslFloatOp                     \
                     : isSintType ? GLSLstd450::GLSLstd450##glslSintOp         \
                                  : GLSLstd450::GLSLstd450##glslUintOp;        \
    retVal = processIntrinsicUsingGLSLInst(callExpr, glslOpcode, doEachVec);   \
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
    retVal = processIntrinsicInterlockedMethod(callExpr, hlslOpcode);
    break;
  case hlsl::IntrinsicOp::IOP_NonUniformResourceIndex:
    retVal = processIntrinsicNonUniformResourceIndex(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_tex1D:
  case hlsl::IntrinsicOp::IOP_tex1Dbias:
  case hlsl::IntrinsicOp::IOP_tex1Dgrad:
  case hlsl::IntrinsicOp::IOP_tex1Dlod:
  case hlsl::IntrinsicOp::IOP_tex1Dproj:
  case hlsl::IntrinsicOp::IOP_tex2D:
  case hlsl::IntrinsicOp::IOP_tex2Dbias:
  case hlsl::IntrinsicOp::IOP_tex2Dgrad:
  case hlsl::IntrinsicOp::IOP_tex2Dlod:
  case hlsl::IntrinsicOp::IOP_tex2Dproj:
  case hlsl::IntrinsicOp::IOP_tex3D:
  case hlsl::IntrinsicOp::IOP_tex3Dbias:
  case hlsl::IntrinsicOp::IOP_tex3Dgrad:
  case hlsl::IntrinsicOp::IOP_tex3Dlod:
  case hlsl::IntrinsicOp::IOP_tex3Dproj:
  case hlsl::IntrinsicOp::IOP_texCUBE:
  case hlsl::IntrinsicOp::IOP_texCUBEbias:
  case hlsl::IntrinsicOp::IOP_texCUBEgrad:
  case hlsl::IntrinsicOp::IOP_texCUBElod:
  case hlsl::IntrinsicOp::IOP_texCUBEproj: {
    emitError("deprecated %0 intrinsic function will not be supported",
              callExpr->getExprLoc())
        << callee->getName();
    return 0;
  }
  case hlsl::IntrinsicOp::IOP_dot:
    retVal = processIntrinsicDot(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_GroupMemoryBarrier:
    retVal = processIntrinsicMemoryBarrier(callExpr,
                                           /*isDevice*/ false,
                                           /*groupSync*/ false,
                                           /*isAllBarrier*/ false);
    break;
  case hlsl::IntrinsicOp::IOP_GroupMemoryBarrierWithGroupSync:
    retVal = processIntrinsicMemoryBarrier(callExpr,
                                           /*isDevice*/ false,
                                           /*groupSync*/ true,
                                           /*isAllBarrier*/ false);
    break;
  case hlsl::IntrinsicOp::IOP_DeviceMemoryBarrier:
    retVal = processIntrinsicMemoryBarrier(callExpr, /*isDevice*/ true,
                                           /*groupSync*/ false,
                                           /*isAllBarrier*/ false);
    break;
  case hlsl::IntrinsicOp::IOP_DeviceMemoryBarrierWithGroupSync:
    retVal = processIntrinsicMemoryBarrier(callExpr, /*isDevice*/ true,
                                           /*groupSync*/ true,
                                           /*isAllBarrier*/ false);
    break;
  case hlsl::IntrinsicOp::IOP_AllMemoryBarrier:
    retVal = processIntrinsicMemoryBarrier(callExpr, /*isDevice*/ true,
                                           /*groupSync*/ false,
                                           /*isAllBarrier*/ true);
    break;
  case hlsl::IntrinsicOp::IOP_AllMemoryBarrierWithGroupSync:
    retVal = processIntrinsicMemoryBarrier(callExpr, /*isDevice*/ true,
                                           /*groupSync*/ true,
                                           /*isAllBarrier*/ true);
    break;
  case hlsl::IntrinsicOp::IOP_CheckAccessFullyMapped:
    retVal =
        theBuilder.createImageSparseTexelsResident(doExpr(callExpr->getArg(0)));
    break;

  case hlsl::IntrinsicOp::IOP_mul:
  case hlsl::IntrinsicOp::IOP_umul:
    retVal = processIntrinsicMul(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_all:
    retVal = processIntrinsicAllOrAny(callExpr, spv::Op::OpAll);
    break;
  case hlsl::IntrinsicOp::IOP_any:
    retVal = processIntrinsicAllOrAny(callExpr, spv::Op::OpAny);
    break;
  case hlsl::IntrinsicOp::IOP_asdouble:
  case hlsl::IntrinsicOp::IOP_asfloat:
  case hlsl::IntrinsicOp::IOP_asint:
  case hlsl::IntrinsicOp::IOP_asuint:
    retVal = processIntrinsicAsType(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_clip:
    retVal = processIntrinsicClip(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_dst:
    retVal = processIntrinsicDst(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_clamp:
  case hlsl::IntrinsicOp::IOP_uclamp:
    retVal = processIntrinsicClamp(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_frexp:
    retVal = processIntrinsicFrexp(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_ldexp:
    retVal = processIntrinsicLdexp(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_lit:
    retVal = processIntrinsicLit(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_modf:
    retVal = processIntrinsicModf(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_msad4:
    retVal = processIntrinsicMsad4(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_sign: {
    if (isFloatOrVecMatOfFloatType(callExpr->getArg(0)->getType()))
      retVal = processIntrinsicFloatSign(callExpr);
    else
      retVal =
          processIntrinsicUsingGLSLInst(callExpr, GLSLstd450::GLSLstd450SSign,
                                        /*actPerRowForMatrices*/ true);
  } break;
  case hlsl::IntrinsicOp::IOP_D3DCOLORtoUBYTE4:
    retVal = processD3DCOLORtoUBYTE4(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_isfinite:
    retVal = processIntrinsicIsFinite(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_sincos:
    retVal = processIntrinsicSinCos(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_rcp:
    retVal = processIntrinsicRcp(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_saturate:
    retVal = processIntrinsicSaturate(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_log10:
    retVal = processIntrinsicLog10(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_f16tof32:
    retVal = processIntrinsicF16ToF32(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_f32tof16:
    retVal = processIntrinsicF32ToF16(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_WaveGetLaneCount: {
    featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "WaveGetLaneCount",
                                    callExpr->getExprLoc());
    const uint32_t retType =
        typeTranslator.translateType(callExpr->getCallReturnType(astContext));
    const uint32_t varId =
        declIdMapper.getBuiltinVar(spv::BuiltIn::SubgroupSize);
    retVal = theBuilder.createLoad(retType, varId);
  } break;
  case hlsl::IntrinsicOp::IOP_WaveGetLaneIndex: {
    featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "WaveGetLaneIndex",
                                    callExpr->getExprLoc());
    const uint32_t retType =
        typeTranslator.translateType(callExpr->getCallReturnType(astContext));
    const uint32_t varId =
        declIdMapper.getBuiltinVar(spv::BuiltIn::SubgroupLocalInvocationId);
    retVal = theBuilder.createLoad(retType, varId);
  } break;
  case hlsl::IntrinsicOp::IOP_WaveIsFirstLane:
    retVal = processWaveQuery(callExpr, spv::Op::OpGroupNonUniformElect);
    break;
  case hlsl::IntrinsicOp::IOP_WaveActiveAllTrue:
    retVal = processWaveVote(callExpr, spv::Op::OpGroupNonUniformAll);
    break;
  case hlsl::IntrinsicOp::IOP_WaveActiveAnyTrue:
    retVal = processWaveVote(callExpr, spv::Op::OpGroupNonUniformAny);
    break;
  case hlsl::IntrinsicOp::IOP_WaveActiveBallot:
    retVal = processWaveVote(callExpr, spv::Op::OpGroupNonUniformBallot);
    break;
  case hlsl::IntrinsicOp::IOP_WaveActiveAllEqual:
    retVal = processWaveVote(callExpr, spv::Op::OpGroupNonUniformAllEqual);
    break;
  case hlsl::IntrinsicOp::IOP_WaveActiveCountBits:
    retVal = processWaveReductionOrPrefix(
        callExpr, spv::Op::OpGroupNonUniformBallotBitCount,
        spv::GroupOperation::Reduce);
    break;
  case hlsl::IntrinsicOp::IOP_WaveActiveUSum:
  case hlsl::IntrinsicOp::IOP_WaveActiveSum:
  case hlsl::IntrinsicOp::IOP_WaveActiveUProduct:
  case hlsl::IntrinsicOp::IOP_WaveActiveProduct:
  case hlsl::IntrinsicOp::IOP_WaveActiveUMax:
  case hlsl::IntrinsicOp::IOP_WaveActiveMax:
  case hlsl::IntrinsicOp::IOP_WaveActiveUMin:
  case hlsl::IntrinsicOp::IOP_WaveActiveMin:
  case hlsl::IntrinsicOp::IOP_WaveActiveBitAnd:
  case hlsl::IntrinsicOp::IOP_WaveActiveBitOr:
  case hlsl::IntrinsicOp::IOP_WaveActiveBitXor: {
    const auto retType = callExpr->getCallReturnType(astContext);
    retVal = processWaveReductionOrPrefix(
        callExpr, translateWaveOp(hlslOpcode, retType, callExpr->getExprLoc()),
        spv::GroupOperation::Reduce);
  } break;
  case hlsl::IntrinsicOp::IOP_WavePrefixUSum:
  case hlsl::IntrinsicOp::IOP_WavePrefixSum:
  case hlsl::IntrinsicOp::IOP_WavePrefixUProduct:
  case hlsl::IntrinsicOp::IOP_WavePrefixProduct: {
    const auto retType = callExpr->getCallReturnType(astContext);
    retVal = processWaveReductionOrPrefix(
        callExpr, translateWaveOp(hlslOpcode, retType, callExpr->getExprLoc()),
        spv::GroupOperation::ExclusiveScan);
  } break;
  case hlsl::IntrinsicOp::IOP_WavePrefixCountBits:
    retVal = processWaveReductionOrPrefix(
        callExpr, spv::Op::OpGroupNonUniformBallotBitCount,
        spv::GroupOperation::ExclusiveScan);
    break;
  case hlsl::IntrinsicOp::IOP_WaveReadLaneAt:
  case hlsl::IntrinsicOp::IOP_WaveReadLaneFirst:
    retVal = processWaveBroadcast(callExpr);
    break;
  case hlsl::IntrinsicOp::IOP_QuadReadAcrossX:
  case hlsl::IntrinsicOp::IOP_QuadReadAcrossY:
  case hlsl::IntrinsicOp::IOP_QuadReadAcrossDiagonal:
  case hlsl::IntrinsicOp::IOP_QuadReadLaneAt:
    retVal = processWaveQuadWideShuffle(callExpr, hlslOpcode);
    break;
  case hlsl::IntrinsicOp::IOP_abort:
  case hlsl::IntrinsicOp::IOP_GetRenderTargetSampleCount:
  case hlsl::IntrinsicOp::IOP_GetRenderTargetSamplePosition: {
    emitError("no equivalent for %0 intrinsic function in Vulkan",
              callExpr->getExprLoc())
        << callee->getName();
    return 0;
  }
  case hlsl::IntrinsicOp::IOP_transpose: {
    const Expr *mat = callExpr->getArg(0);
    const QualType matType = mat->getType();
    if (hlsl::GetHLSLMatElementType(matType)->isFloatingType())
      retVal =
          processIntrinsicUsingSpirvInst(callExpr, spv::Op::OpTranspose, false);
    else
      retVal = processNonFpMatrixTranspose(matType, doExpr(mat));

    break;
  }
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
    emitError("%0 intrinsic function unimplemented", callExpr->getExprLoc())
        << callee->getName();
    return 0;
  }

#undef INTRINSIC_OP_CASE
#undef INTRINSIC_OP_CASE_INT_FLOAT

  return SpirvEvalInfo(retVal).setRValue();
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
      argId = castToInt(argId, valueExpr->getType(), baseType,
                        valueExpr->getExprLoc());
    return argId;
  };

  const auto writeToOutputArg = [&baseType, dest, this](
                                    uint32_t toWrite, const CallExpr *callExpr,
                                    uint32_t outputArgIndex) {
    const auto outputArg = callExpr->getArg(outputArgIndex);
    const auto outputArgType = outputArg->getType();
    if (baseType != outputArgType)
      toWrite = castToInt(toWrite, baseType, outputArgType, dest->getExprLoc());
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
      auto baseId = doExpr(base);
      if (baseId.isRValue()) {
        // OpImageTexelPointer's Image argument must have a type of
        // OpTypePointer with Type OpTypeImage. Need to create a temporary
        // variable if the baseId is an rvalue.
        baseId = createTemporaryVar(
            base->getType(), TypeTranslator::getName(base->getType()), baseId);
      }
      const auto coordId = doExpr(index);
      ptr = theBuilder.createImageTexelPointer(ptrType, baseId, coordId, zero);
      if (baseId.isNonUniform()) {
        // Image texel pointer will used to access image memory. Vulkan requires
        // it to be decorated with NonUniformEXT.
        theBuilder.decorateNonUniformEXT(ptr);
      }
    }
  }
  if (!ptr)
    ptr = doExpr(dest);

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

SpirvEvalInfo
SPIRVEmitter::processIntrinsicNonUniformResourceIndex(const CallExpr *expr) {
  foundNonUniformResourceIndex = true;
  theBuilder.addExtension(Extension::EXT_descriptor_indexing,
                          "NonUniformResourceIndex", expr->getExprLoc());
  theBuilder.requireCapability(spv::Capability::ShaderNonUniformEXT);

  auto index = doExpr(expr->getArg(0)).setNonUniform();
  // Decorate the expression in NonUniformResourceIndex() with NonUniformEXT.
  // Aside from this, we also need to eventually populate the NonUniformEXT
  // status to the usage of this expression: the "pointer" operand to a memory
  // access instruction. Vulkan spec has the following rules:
  //
  // If an instruction loads from or stores to a resource (including atomics and
  // image instructions) and the resource descriptor being accessed is not
  // dynamically uniform, then the operand corresponding to that resource (e.g.
  // the pointer or sampled image operand) must be decorated with NonUniformEXT.
  theBuilder.decorateNonUniformEXT(index);

  return index;
}

uint32_t SPIRVEmitter::processIntrinsicMsad4(const CallExpr *callExpr) {
  emitWarning("msad4 intrinsic function is emulated using many SPIR-V "
              "instructions due to lack of direct SPIR-V equivalent",
              callExpr->getExprLoc());

  // Compares a 4-byte reference value and an 8-byte source value and
  // accumulates a vector of 4 sums. Each sum corresponds to the masked sum
  // of absolute differences of a different byte alignment between the
  // reference value and the source value.

  // If we have:
  // uint  v0; // reference
  // uint2 v1; // source
  // uint4 v2; // accum
  // uint4 o0; // result of msad4
  // uint4 r0, t0; // temporary values
  //
  // Then msad4(v0, v1, v2) translates to the following SM5 assembly according
  // to fxc:
  //   Step 1:
  //     ushr r0.xyz, v1.xxxx, l(8, 16, 24, 0)
  //   Step 2:
  //         [result], [    width    ], [    offset   ], [ insert ], [ base ]
  //     bfi   t0.yzw, l(0, 8, 16, 24), l(0, 24, 16, 8),  v1.yyyy  , r0.xxyz
  //     mov t0.x, v1.x
  //   Step 3:
  //     msad o0.xyzw, v0.xxxx, t0.xyzw, v2.xyzw

  const uint32_t glsl = theBuilder.getGLSLExtInstSet();
  const auto boolType = theBuilder.getBoolType();
  const auto intType = theBuilder.getInt32Type();
  const auto uintType = theBuilder.getUint32Type();
  const auto uint4Type = theBuilder.getVecType(uintType, 4);
  const uint32_t reference = doExpr(callExpr->getArg(0));
  const uint32_t source = doExpr(callExpr->getArg(1));
  const uint32_t accum = doExpr(callExpr->getArg(2));
  const auto uint0 = theBuilder.getConstantUint32(0);
  const auto uint8 = theBuilder.getConstantUint32(8);
  const auto uint16 = theBuilder.getConstantUint32(16);
  const auto uint24 = theBuilder.getConstantUint32(24);

  // Step 1.
  const uint32_t v1x = theBuilder.createCompositeExtract(uintType, source, {0});
  // r0.x = v1xS8 = v1.x shifted by 8 bits
  uint32_t v1xS8 = theBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                             uintType, v1x, uint8);
  // r0.y = v1xS16 = v1.x shifted by 16 bits
  uint32_t v1xS16 = theBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                              uintType, v1x, uint16);
  // r0.z = v1xS24 = v1.x shifted by 24 bits
  uint32_t v1xS24 = theBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                              uintType, v1x, uint24);

  // Step 2.
  // Do bfi 3 times. DXIL bfi is equivalent to SPIR-V OpBitFieldInsert.
  const uint32_t v1y = theBuilder.createCompositeExtract(uintType, source, {1});
  // Note that t0.x = v1.x, nothing we need to do for that.
  const uint32_t t0y =
      theBuilder.createBitFieldInsert(uintType, /*base*/ v1xS8, /*insert*/ v1y,
                                      /*offset*/ uint24,
                                      /*width*/ uint8);
  const uint32_t t0z =
      theBuilder.createBitFieldInsert(uintType, /*base*/ v1xS16, /*insert*/ v1y,
                                      /*offset*/ uint16,
                                      /*width*/ uint16);
  const uint32_t t0w =
      theBuilder.createBitFieldInsert(uintType, /*base*/ v1xS24, /*insert*/ v1y,
                                      /*offset*/ uint8,
                                      /*width*/ uint24);

  // Step 3. MSAD (Masked Sum of Absolute Differences)

  // Now perform MSAD four times.
  // Need to mimic this algorithm in SPIR-V!
  //
  // UINT msad( UINT ref, UINT src, UINT accum )
  // {
  //     for (UINT i = 0; i < 4; i++)
  //     {
  //         BYTE refByte, srcByte, absDiff;
  //
  //         refByte = (BYTE)(ref >> (i * 8));
  //         if (!refByte)
  //         {
  //             continue;
  //         }
  //
  //         srcByte = (BYTE)(src >> (i * 8));
  //         if (refByte >= srcByte)
  //         {
  //             absDiff = refByte - srcByte;
  //         }
  //         else
  //         {
  //             absDiff = srcByte - refByte;
  //         }
  //
  //         // The recommended overflow behavior for MSAD is
  //         // to do a 32-bit saturate. This is not
  //         // required, however, and wrapping is allowed.
  //         // So from an application point of view,
  //         // overflow behavior is undefined.
  //         if (UINT_MAX - accum < absDiff)
  //         {
  //             accum = UINT_MAX;
  //             break;
  //         }
  //         accum += absDiff;
  //     }
  //
  //     return accum;
  // }

  llvm::SmallVector<uint32_t, 4> result;
  const uint32_t accum0 =
      theBuilder.createCompositeExtract(uintType, accum, {0});
  const uint32_t accum1 =
      theBuilder.createCompositeExtract(uintType, accum, {1});
  const uint32_t accum2 =
      theBuilder.createCompositeExtract(uintType, accum, {2});
  const uint32_t accum3 =
      theBuilder.createCompositeExtract(uintType, accum, {3});
  const llvm::SmallVector<uint32_t, 4> sources = {v1x, t0y, t0z, t0w};
  llvm::SmallVector<uint32_t, 4> accums = {accum0, accum1, accum2, accum3};
  llvm::SmallVector<uint32_t, 4> refBytes;
  llvm::SmallVector<uint32_t, 4> signedRefBytes;
  llvm::SmallVector<uint32_t, 4> isRefByteZero;
  for (uint32_t i = 0; i < 4; ++i) {
    refBytes.push_back(theBuilder.createBitFieldExtract(
        uintType, reference, /*offset*/ theBuilder.getConstantUint32(i * 8),
        /*count*/ uint8, /*isSigned*/ false));
    signedRefBytes.push_back(
        theBuilder.createUnaryOp(spv::Op::OpBitcast, intType, refBytes.back()));
    isRefByteZero.push_back(theBuilder.createBinaryOp(
        spv::Op::OpIEqual, boolType, refBytes.back(), uint0));
  }

  for (uint32_t msadNum = 0; msadNum < 4; ++msadNum) {
    for (uint32_t byteCount = 0; byteCount < 4; ++byteCount) {
      // 'count' is always 8 because we are extracting 8 bits out of 32.
      const uint32_t srcByte = theBuilder.createBitFieldExtract(
          uintType, sources[msadNum],
          /*offset*/ theBuilder.getConstantUint32(8 * byteCount),
          /*count*/ uint8, /*isSigned*/ false);
      const uint32_t signedSrcByte =
          theBuilder.createUnaryOp(spv::Op::OpBitcast, intType, srcByte);
      const uint32_t sub = theBuilder.createBinaryOp(
          spv::Op::OpISub, intType, signedRefBytes[byteCount], signedSrcByte);
      const uint32_t absSub = theBuilder.createExtInst(
          intType, glsl, GLSLstd450::GLSLstd450SAbs, {sub});
      const uint32_t diff = theBuilder.createSelect(
          uintType, isRefByteZero[byteCount], uint0,
          theBuilder.createUnaryOp(spv::Op::OpBitcast, uintType, absSub));

      // As pointed out by the DXIL reference above, it is *not* required to
      // saturate the output to UINT_MAX in case of overflow. Wrapping around is
      // also allowed. For simplicity, we will wrap around at this point.
      accums[msadNum] = theBuilder.createBinaryOp(spv::Op::OpIAdd, uintType,
                                                  accums[msadNum], diff);
    }
  }
  return theBuilder.createCompositeConstruct(uint4Type, accums);
}

uint32_t SPIRVEmitter::processWaveQuery(const CallExpr *callExpr,
                                        spv::Op opcode) {
  // Signatures:
  // bool WaveIsFirstLane()
  // uint WaveGetLaneCount()
  // uint WaveGetLaneIndex()
  assert(callExpr->getNumArgs() == 0);
  featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "Wave Operation",
                                  callExpr->getExprLoc());
  theBuilder.requireCapability(getCapabilityForGroupNonUniform(opcode));
  const uint32_t subgroupScope = theBuilder.getConstantInt32(3);
  const uint32_t retType =
      typeTranslator.translateType(callExpr->getCallReturnType(astContext));
  return theBuilder.createGroupNonUniformOp(opcode, retType, subgroupScope);
}

uint32_t SPIRVEmitter::processWaveVote(const CallExpr *callExpr,
                                       spv::Op opcode) {
  // Signatures:
  // bool WaveActiveAnyTrue( bool expr )
  // bool WaveActiveAllTrue( bool expr )
  // bool uint4 WaveActiveBallot( bool expr )
  assert(callExpr->getNumArgs() == 1);
  featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "Wave Operation",
                                  callExpr->getExprLoc());
  theBuilder.requireCapability(getCapabilityForGroupNonUniform(opcode));
  const uint32_t predicate = doExpr(callExpr->getArg(0));
  const uint32_t subgroupScope = theBuilder.getConstantInt32(3);
  const uint32_t retType =
      typeTranslator.translateType(callExpr->getCallReturnType(astContext));
  return theBuilder.createGroupNonUniformUnaryOp(opcode, retType, subgroupScope,
                                                 predicate);
}

spv::Op SPIRVEmitter::translateWaveOp(hlsl::IntrinsicOp op, QualType type,
                                      SourceLocation srcLoc) {
  const bool isSintType = isSintOrVecMatOfSintType(type);
  const bool isUintType = isUintOrVecMatOfUintType(type);
  const bool isFloatType = isFloatOrVecMatOfFloatType(type);

#define WAVE_OP_CASE_INT(kind, intWaveOp)                                      \
                                                                               \
  case hlsl::IntrinsicOp::IOP_Wave##kind: {                                    \
    if (isSintType || isUintType) {                                            \
      return spv::Op::OpGroupNonUniform##intWaveOp;                            \
    }                                                                          \
  } break

#define WAVE_OP_CASE_INT_FLOAT(kind, intWaveOp, floatWaveOp)                   \
                                                                               \
  case hlsl::IntrinsicOp::IOP_Wave##kind: {                                    \
    if (isSintType || isUintType) {                                            \
      return spv::Op::OpGroupNonUniform##intWaveOp;                            \
    }                                                                          \
    if (isFloatType) {                                                         \
      return spv::Op::OpGroupNonUniform##floatWaveOp;                          \
    }                                                                          \
  } break

#define WAVE_OP_CASE_SINT_UINT_FLOAT(kind, sintWaveOp, uintWaveOp,             \
                                     floatWaveOp)                              \
                                                                               \
  case hlsl::IntrinsicOp::IOP_Wave##kind: {                                    \
    if (isSintType) {                                                          \
      return spv::Op::OpGroupNonUniform##sintWaveOp;                           \
    }                                                                          \
    if (isUintType) {                                                          \
      return spv::Op::OpGroupNonUniform##uintWaveOp;                           \
    }                                                                          \
    if (isFloatType) {                                                         \
      return spv::Op::OpGroupNonUniform##floatWaveOp;                          \
    }                                                                          \
  } break

  switch (op) {
    WAVE_OP_CASE_INT_FLOAT(ActiveUSum, IAdd, FAdd);
    WAVE_OP_CASE_INT_FLOAT(ActiveSum, IAdd, FAdd);
    WAVE_OP_CASE_INT_FLOAT(ActiveUProduct, IMul, FMul);
    WAVE_OP_CASE_INT_FLOAT(ActiveProduct, IMul, FMul);
    WAVE_OP_CASE_INT_FLOAT(PrefixUSum, IAdd, FAdd);
    WAVE_OP_CASE_INT_FLOAT(PrefixSum, IAdd, FAdd);
    WAVE_OP_CASE_INT_FLOAT(PrefixUProduct, IMul, FMul);
    WAVE_OP_CASE_INT_FLOAT(PrefixProduct, IMul, FMul);
    WAVE_OP_CASE_INT(ActiveBitAnd, BitwiseAnd);
    WAVE_OP_CASE_INT(ActiveBitOr, BitwiseOr);
    WAVE_OP_CASE_INT(ActiveBitXor, BitwiseXor);
    WAVE_OP_CASE_SINT_UINT_FLOAT(ActiveUMax, SMax, UMax, FMax);
    WAVE_OP_CASE_SINT_UINT_FLOAT(ActiveMax, SMax, UMax, FMax);
    WAVE_OP_CASE_SINT_UINT_FLOAT(ActiveUMin, SMin, UMin, FMin);
    WAVE_OP_CASE_SINT_UINT_FLOAT(ActiveMin, SMin, UMin, FMin);
  default:
    // Only Simple Wave Ops are handled here.
    break;
  }
#undef WAVE_OP_CASE_INT_FLOAT
#undef WAVE_OP_CASE_INT
#undef WAVE_OP_CASE_SINT_UINT_FLOAT

  emitError("translating wave operator '%0' unimplemented", srcLoc)
      << static_cast<uint32_t>(op);
  return spv::Op::OpNop;
}

uint32_t SPIRVEmitter::processWaveReductionOrPrefix(
    const CallExpr *callExpr, spv::Op opcode, spv::GroupOperation groupOp) {
  // Signatures:
  // bool WaveActiveAllEqual( <type> expr )
  // uint WaveActiveCountBits( bool bBit )
  // <type> WaveActiveSum( <type> expr )
  // <type> WaveActiveProduct( <type> expr )
  // <int_type> WaveActiveBitAnd( <int_type> expr )
  // <int_type> WaveActiveBitOr( <int_type> expr )
  // <int_type> WaveActiveBitXor( <int_type> expr )
  // <type> WaveActiveMin( <type> expr)
  // <type> WaveActiveMax( <type> expr)
  //
  // uint WavePrefixCountBits(Bool bBit)
  // <type> WavePrefixProduct(<type> value)
  // <type> WavePrefixSum(<type> value)
  assert(callExpr->getNumArgs() == 1);
  featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "Wave Operation",
                                  callExpr->getExprLoc());
  theBuilder.requireCapability(getCapabilityForGroupNonUniform(opcode));
  const uint32_t predicate = doExpr(callExpr->getArg(0));
  const uint32_t subgroupScope = theBuilder.getConstantInt32(3);
  const uint32_t retType =
      typeTranslator.translateType(callExpr->getCallReturnType(astContext));
  return theBuilder.createGroupNonUniformUnaryOp(
      opcode, retType, subgroupScope, predicate,
      llvm::Optional<spv::GroupOperation>(groupOp));
}

uint32_t SPIRVEmitter::processWaveBroadcast(const CallExpr *callExpr) {
  // Signatures:
  // <type> WaveReadLaneFirst(<type> expr)
  // <type> WaveReadLaneAt(<type> expr, uint laneIndex)
  const auto numArgs = callExpr->getNumArgs();
  assert(numArgs == 1 || numArgs == 2);
  featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "Wave Operation",
                                  callExpr->getExprLoc());
  theBuilder.requireCapability(spv::Capability::GroupNonUniformBallot);
  const uint32_t value = doExpr(callExpr->getArg(0));
  const uint32_t subgroupScope = theBuilder.getConstantInt32(3);
  const uint32_t retType =
      typeTranslator.translateType(callExpr->getCallReturnType(astContext));
  if (numArgs == 2)
    return theBuilder.createGroupNonUniformBinaryOp(
        spv::Op::OpGroupNonUniformBroadcast, retType, subgroupScope, value,
        doExpr(callExpr->getArg(1)));
  else
    return theBuilder.createGroupNonUniformUnaryOp(
        spv::Op::OpGroupNonUniformBroadcastFirst, retType, subgroupScope,
        value);
}

uint32_t SPIRVEmitter::processWaveQuadWideShuffle(const CallExpr *callExpr,
                                                  hlsl::IntrinsicOp op) {
  // Signatures:
  // <type> QuadReadAcrossX(<type> localValue)
  // <type> QuadReadAcrossY(<type> localValue)
  // <type> QuadReadAcrossDiagonal(<type> localValue)
  // <type> QuadReadLaneAt(<type> sourceValue, uint quadLaneID)
  assert(callExpr->getNumArgs() == 1 || callExpr->getNumArgs() == 2);
  featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "Wave Operation",
                                  callExpr->getExprLoc());
  theBuilder.requireCapability(spv::Capability::GroupNonUniformQuad);

  const uint32_t value = doExpr(callExpr->getArg(0));
  const uint32_t subgroupScope = theBuilder.getConstantInt32(3);
  const uint32_t retType =
      typeTranslator.translateType(callExpr->getCallReturnType(astContext));

  uint32_t target = 0;
  spv::Op opcode = spv::Op::OpGroupNonUniformQuadSwap;
  switch (op) {
  case hlsl::IntrinsicOp::IOP_QuadReadAcrossX:
    target = theBuilder.getConstantUint32(0);
    break;
  case hlsl::IntrinsicOp::IOP_QuadReadAcrossY:
    target = theBuilder.getConstantUint32(1);
    break;
  case hlsl::IntrinsicOp::IOP_QuadReadAcrossDiagonal:
    target = theBuilder.getConstantUint32(2);
    break;
  case hlsl::IntrinsicOp::IOP_QuadReadLaneAt:
    target = doExpr(callExpr->getArg(1));
    opcode = spv::Op::OpGroupNonUniformQuadBroadcast;
    break;
  default:
    llvm_unreachable("case should not appear here");
  }

  return theBuilder.createGroupNonUniformBinaryOp(opcode, retType,
                                                  subgroupScope, value, target);
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
  const uint32_t argId = doExpr(arg);
  const uint32_t ipId = doExpr(ipArg);

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
      ip = castToInt(ip, argType, ipType, arg->getExprLoc());
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

      uint32_t ip = theBuilder.createCompositeConstruct(
          typeTranslator.translateType(argType), ips);
      // If the 'ip' is not a float type, the AST will not contain a CastExpr
      // because this is internal to the intrinsic function. So, in such a
      // case we need to cast manually.
      if (!hlsl::GetHLSLMatElementType(ipType)->isFloatingType())
        ip = castToInt(ip, argType, ipType, ipArg->getExprLoc());
      theBuilder.createStore(ipId, ip);
      return theBuilder.createCompositeConstruct(returnTypeId, fracs);
    }
  }

  emitError("invalid argument type passed to Modf intrinsic function",
            callExpr->getExprLoc());
  return 0;
}

uint32_t SPIRVEmitter::processIntrinsicLit(const CallExpr *callExpr) {
  // Signature is: float4 lit(float n_dot_l, float n_dot_h, float m)
  //
  // This function returns a lighting coefficient vector
  // (ambient, diffuse, specular, 1) where:
  // ambient  = 1.
  // diffuse  = (n_dot_l < 0) ? 0 : n_dot_l
  // specular = (n_dot_l < 0 || n_dot_h < 0) ? 0 : ((n_dot_h) * m)
  const uint32_t glslInstSetId = theBuilder.getGLSLExtInstSet();
  const uint32_t nDotL = doExpr(callExpr->getArg(0));
  const uint32_t nDotH = doExpr(callExpr->getArg(1));
  const uint32_t m = doExpr(callExpr->getArg(2));
  const uint32_t floatType = theBuilder.getFloat32Type();
  const uint32_t boolType = theBuilder.getBoolType();
  const uint32_t floatZero = theBuilder.getConstantFloat32(0);
  const uint32_t floatOne = theBuilder.getConstantFloat32(1);
  const uint32_t retType = typeTranslator.translateType(callExpr->getType());
  const uint32_t diffuse = theBuilder.createExtInst(
      floatType, glslInstSetId, GLSLstd450::GLSLstd450FMax, {floatZero, nDotL});
  const uint32_t min = theBuilder.createExtInst(
      floatType, glslInstSetId, GLSLstd450::GLSLstd450FMin, {nDotL, nDotH});
  const uint32_t isNeg = theBuilder.createBinaryOp(spv::Op::OpFOrdLessThan,
                                                   boolType, min, floatZero);
  const uint32_t mul =
      theBuilder.createBinaryOp(spv::Op::OpFMul, floatType, nDotH, m);
  const uint32_t specular =
      theBuilder.createSelect(floatType, isNeg, floatZero, mul);
  return theBuilder.createCompositeConstruct(
      retType, {floatOne, diffuse, specular, floatOne});
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

  emitError("invalid argument type passed to Frexp intrinsic function",
            callExpr->getExprLoc());
  return 0;
}

uint32_t SPIRVEmitter::processIntrinsicLdexp(const CallExpr *callExpr) {
  // Signature: ret ldexp(x, exp)
  // This function uses the following formula: x * 2^exp.
  // Note that we cannot use GLSL extended instruction Ldexp since it requires
  // the exponent to be an integer (vector) but HLSL takes an float (vector)
  // exponent. So we must calculate the result manually.
  const uint32_t glsl = theBuilder.getGLSLExtInstSet();
  const Expr *x = callExpr->getArg(0);
  const auto paramType = x->getType();
  const uint32_t xId = doExpr(x);
  const uint32_t expId = doExpr(callExpr->getArg(1));

  // For scalar and vector argument types.
  if (TypeTranslator::isScalarType(paramType) ||
      TypeTranslator::isVectorType(paramType)) {
    const auto paramTypeId = typeTranslator.translateType(paramType);
    const auto twoExp = theBuilder.createExtInst(
        paramTypeId, glsl, GLSLstd450::GLSLstd450Exp2, {expId});
    return theBuilder.createBinaryOp(spv::Op::OpFMul, paramTypeId, xId, twoExp);
  }

  // For matrix argument types.
  {
    uint32_t rowCount = 0, colCount = 0;
    if (TypeTranslator::isMxNMatrix(paramType, nullptr, &rowCount, &colCount)) {
      const auto actOnEachVec = [this, glsl, expId](uint32_t index,
                                                    uint32_t vecType,
                                                    uint32_t xRowId) {
        const auto expRowId =
            theBuilder.createCompositeExtract(vecType, expId, {index});
        const auto twoExp = theBuilder.createExtInst(
            vecType, glsl, GLSLstd450::GLSLstd450Exp2, {expRowId});
        return theBuilder.createBinaryOp(spv::Op::OpFMul, vecType, xRowId,
                                         twoExp);
      };
      return processEachVectorInMatrix(x, xId, actOnEachVec);
    }
  }

  emitError("invalid argument type passed to ldexp intrinsic function",
            callExpr->getExprLoc());
  return 0;
}

uint32_t SPIRVEmitter::processIntrinsicDst(const CallExpr *callExpr) {
  // Signature is float4 dst(float4 src0, float4 src1)
  // result.x = 1;
  // result.y = src0.y * src1.y;
  // result.z = src0.z;
  // result.w = src1.w;
  const auto floatId = theBuilder.getFloat32Type();
  const auto arg0Id = doExpr(callExpr->getArg(0));
  const auto arg1Id = doExpr(callExpr->getArg(1));
  const auto arg0y = theBuilder.createCompositeExtract(floatId, arg0Id, {1});
  const auto arg1y = theBuilder.createCompositeExtract(floatId, arg1Id, {1});
  const auto arg0z = theBuilder.createCompositeExtract(floatId, arg0Id, {2});
  const auto arg1w = theBuilder.createCompositeExtract(floatId, arg1Id, {3});
  const auto arg0yMularg1y =
      theBuilder.createBinaryOp(spv::Op::OpFMul, floatId, arg0y, arg1y);
  return theBuilder.createCompositeConstruct(
      typeTranslator.translateType(callExpr->getType()),
      {theBuilder.getConstantFloat32(1.0), arg0yMularg1y, arg0z, arg1w});
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
    emitError("invalid argument type passed to clip intrinsic function",
              callExpr->getExprLoc());
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
  if (TypeTranslator::isMxNMatrix(argX->getType())) {
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

uint32_t SPIRVEmitter::processIntrinsicMemoryBarrier(const CallExpr *callExpr,
                                                     bool isDevice,
                                                     bool groupSync,
                                                     bool isAllBarrier) {
  // * DeviceMemoryBarrier =
  // OpMemoryBarrier (memScope=Device,
  //                  sem=Image|Uniform|AcquireRelease)
  //
  // * DeviceMemoryBarrierWithGroupSync =
  // OpControlBarrier(execScope = Workgroup,
  //                  memScope=Device,
  //                  sem=Image|Uniform|AcquireRelease)
  const spv::MemorySemanticsMask deviceMemoryBarrierSema =
      spv::MemorySemanticsMask::ImageMemory |
      spv::MemorySemanticsMask::UniformMemory |
      spv::MemorySemanticsMask::AcquireRelease;

  // * GroupMemoryBarrier =
  // OpMemoryBarrier (memScope=Workgroup,
  //                  sem = Workgroup|AcquireRelease)
  //
  // * GroupMemoryBarrierWithGroupSync =
  // OpControlBarrier (execScope = Workgroup,
  //                   memScope = Workgroup,
  //                   sem = Workgroup|AcquireRelease)
  const spv::MemorySemanticsMask groupMemoryBarrierSema =
      spv::MemorySemanticsMask::WorkgroupMemory |
      spv::MemorySemanticsMask::AcquireRelease;

  // * AllMemoryBarrier =
  // OpMemoryBarrier(memScope = Device,
  //                 sem = Image|Uniform|Workgroup|AcquireRelease)
  //
  // * AllMemoryBarrierWithGroupSync =
  // OpControlBarrier(execScope = Workgroup,
  //                  memScope = Device,
  //                  sem = Image|Uniform|Workgroup|AcquireRelease)
  const spv::MemorySemanticsMask allMemoryBarrierSema =
      spv::MemorySemanticsMask::ImageMemory |
      spv::MemorySemanticsMask::UniformMemory |
      spv::MemorySemanticsMask::WorkgroupMemory |
      spv::MemorySemanticsMask::AtomicCounterMemory |
      spv::MemorySemanticsMask::AcquireRelease;

  // Get <result-id> for execution scope.
  // If present, execution scope is always Workgroup!
  const uint32_t execScopeId =
      groupSync ? theBuilder.getConstantUint32(
                      static_cast<uint32_t>(spv::Scope::Workgroup))
                : 0;

  // Get <result-id> for memory scope
  const spv::Scope memScope =
      (isDevice || isAllBarrier) ? spv::Scope::Device : spv::Scope::Workgroup;
  const uint32_t memScopeId =
      theBuilder.getConstantUint32(static_cast<uint32_t>(memScope));

  // Get <result-id> for memory semantics
  const auto memSemaMask = isAllBarrier ? allMemoryBarrierSema
                                        : isDevice ? deviceMemoryBarrierSema
                                                   : groupMemoryBarrierSema;
  const uint32_t memSema =
      theBuilder.getConstantUint32(static_cast<uint32_t>(memSemaMask));
  theBuilder.createBarrier(execScopeId, memScopeId, memSema);
  return 0;
}

uint32_t SPIRVEmitter::processNonFpMatrixTranspose(QualType matType,
                                                   uint32_t matId) {
  // Simplest way is to flatten the matrix construct a new matrix from the
  // flattened elements. (for a mat4x4).
  QualType elemType = {};
  uint32_t numRows = 0, numCols = 0;
  const bool isMat =
      TypeTranslator::isMxNMatrix(matType, &elemType, &numRows, &numCols);
  assert(isMat && !elemType->isFloatingType());
  (void)isMat;

  const auto colQualType = astContext.getExtVectorType(elemType, numRows);
  const uint32_t colTypeId = typeTranslator.translateType(colQualType);
  const uint32_t elemTypeId = typeTranslator.translateType(elemType);

  // You cannot perform a composite construct of an array using a few vectors.
  // The number of constutients passed to OpCompositeConstruct must be equal to
  // the number of array elements.
  llvm::SmallVector<uint32_t, 4> elems;
  for (uint32_t i = 0; i < numRows; ++i)
    for (uint32_t j = 0; j < numCols; ++j)
      elems.push_back(
          theBuilder.createCompositeExtract(elemTypeId, matId, {i, j}));

  llvm::SmallVector<uint32_t, 4> cols;
  for (uint32_t i = 0; i < numCols; ++i) {
    // The elements in the ith vector of the "transposed" array are at offset i,
    // i + <original-vector-size>, ...
    llvm::SmallVector<uint32_t, 4> indexes;
    for (uint32_t j = 0; j < numRows; ++j)
      indexes.push_back(elems[i + (j * numCols)]);

    cols.push_back(theBuilder.createCompositeConstruct(colTypeId, indexes));
  }

  const auto transposeTypeId =
      theBuilder.getArrayType(colTypeId, theBuilder.getConstantUint32(numCols));
  return theBuilder.createCompositeConstruct(transposeTypeId, cols);
}

uint32_t SPIRVEmitter::processNonFpDot(uint32_t vec1Id, uint32_t vec2Id,
                                       uint32_t vecSize, QualType elemType) {
  const auto elemTypeId = typeTranslator.translateType(elemType);
  llvm::SmallVector<uint32_t, 4> muls;
  for (uint32_t i = 0; i < vecSize; ++i) {
    const auto elem1 =
        theBuilder.createCompositeExtract(elemTypeId, vec1Id, {i});
    const auto elem2 =
        theBuilder.createCompositeExtract(elemTypeId, vec2Id, {i});
    muls.push_back(theBuilder.createBinaryOp(translateOp(BO_Mul, elemType),
                                             elemTypeId, elem1, elem2));
  }
  uint32_t sum = muls[0];
  for (uint32_t i = 1; i < vecSize; ++i) {
    sum = theBuilder.createBinaryOp(translateOp(BO_Add, elemType), elemTypeId,
                                    sum, muls[i]);
  }
  return sum;
}

uint32_t SPIRVEmitter::processNonFpScalarTimesMatrix(QualType scalarType,
                                                     uint32_t scalarId,
                                                     QualType matrixType,
                                                     uint32_t matrixId) {
  assert(TypeTranslator::isScalarType(scalarType));
  QualType elemType = {};
  uint32_t numRows = 0, numCols = 0;
  const bool isMat =
      TypeTranslator::isMxNMatrix(matrixType, &elemType, &numRows, &numCols);
  assert(isMat);
  assert(typeTranslator.isSameType(scalarType, elemType));
  (void)isMat;

  // We need to multiply the scalar by each vector of the matrix.
  // The front-end guarantees that the scalar and matrix element type are
  // the same. For example, if the scalar is a float, the matrix is casted
  // to a float matrix before being passed to mul(). It is also guaranteed
  // that types such as bool are casted to float or int before being
  // passed to mul().
  const auto rowType = astContext.getExtVectorType(elemType, numCols);
  const auto rowTypeId = typeTranslator.translateType(rowType);
  llvm::SmallVector<uint32_t, 4> splat(size_t(numCols), scalarId);
  const auto scalarSplat =
      theBuilder.createCompositeConstruct(rowTypeId, splat);
  llvm::SmallVector<uint32_t, 4> mulRows;
  for (uint32_t row = 0; row < numRows; ++row) {
    const auto rowId =
        theBuilder.createCompositeExtract(rowTypeId, matrixId, {row});
    mulRows.push_back(theBuilder.createBinaryOp(translateOp(BO_Mul, scalarType),
                                                rowTypeId, rowId, scalarSplat));
  }
  return theBuilder.createCompositeConstruct(
      typeTranslator.translateType(matrixType), mulRows);
}

uint32_t SPIRVEmitter::processNonFpVectorTimesMatrix(QualType vecType,
                                                     uint32_t vecId,
                                                     QualType matType,
                                                     uint32_t matId,
                                                     uint32_t matTransposeId) {
  // This function assumes that the vector element type and matrix elemet type
  // are the same.
  QualType vecElemType = {}, matElemType = {};
  uint32_t vecSize = 0, numRows = 0, numCols = 0;
  const bool isVec =
      TypeTranslator::isVectorType(vecType, &vecElemType, &vecSize);
  const bool isMat =
      TypeTranslator::isMxNMatrix(matType, &matElemType, &numRows, &numCols);
  assert(typeTranslator.isSameType(vecElemType, matElemType));
  assert(isVec);
  assert(isMat);
  assert(vecSize == numRows);
  (void)isVec;
  (void)isMat;

  // When processing vector times matrix, the vector is a row vector, and it
  // should be multiplied by the matrix *columns*. The most efficient way to
  // handle this in SPIR-V would be to first transpose the matrix, and then use
  // OpAccessChain.
  if (!matTransposeId)
    matTransposeId = processNonFpMatrixTranspose(matType, matId);

  const auto vecTypeId = typeTranslator.translateType(vecType);
  llvm::SmallVector<uint32_t, 4> resultElems;
  for (uint32_t col = 0; col < numCols; ++col) {
    const auto colId =
        theBuilder.createCompositeExtract(vecTypeId, matTransposeId, {col});
    resultElems.push_back(processNonFpDot(vecId, colId, vecSize, vecElemType));
  }
  return theBuilder.createCompositeConstruct(
      typeTranslator.translateType(
          astContext.getExtVectorType(vecElemType, numCols)),
      resultElems);
}

uint32_t SPIRVEmitter::processNonFpMatrixTimesVector(QualType matType,
                                                     uint32_t matId,
                                                     QualType vecType,
                                                     uint32_t vecId) {
  // This function assumes that the vector element type and matrix elemet type
  // are the same.
  QualType vecElemType = {}, matElemType = {};
  uint32_t vecSize = 0, numRows = 0, numCols = 0;
  const bool isVec =
      TypeTranslator::isVectorType(vecType, &vecElemType, &vecSize);
  const bool isMat =
      TypeTranslator::isMxNMatrix(matType, &matElemType, &numRows, &numCols);
  assert(typeTranslator.isSameType(vecElemType, matElemType));
  assert(isVec);
  assert(isMat);
  assert(vecSize == numCols);
  (void)isVec;
  (void)isMat;

  // When processing matrix times vector, the vector is a column vector. So we
  // simply get each row of the matrix and perform a dot product with the
  // vector.
  const auto vecTypeId = typeTranslator.translateType(vecType);
  llvm::SmallVector<uint32_t, 4> resultElems;
  for (uint32_t row = 0; row < numRows; ++row) {
    const auto rowId =
        theBuilder.createCompositeExtract(vecTypeId, matId, {row});
    resultElems.push_back(processNonFpDot(rowId, vecId, vecSize, vecElemType));
  }
  return theBuilder.createCompositeConstruct(
      typeTranslator.translateType(
          astContext.getExtVectorType(vecElemType, numRows)),
      resultElems);
}

uint32_t SPIRVEmitter::processNonFpMatrixTimesMatrix(QualType lhsType,
                                                     uint32_t lhsId,
                                                     QualType rhsType,
                                                     uint32_t rhsId) {
  // This function assumes that the vector element type and matrix elemet type
  // are the same.
  QualType lhsElemType = {}, rhsElemType = {};
  uint32_t lhsNumRows = 0, lhsNumCols = 0;
  uint32_t rhsNumRows = 0, rhsNumCols = 0;
  const bool lhsIsMat = TypeTranslator::isMxNMatrix(lhsType, &lhsElemType,
                                                    &lhsNumRows, &lhsNumCols);
  const bool rhsIsMat = TypeTranslator::isMxNMatrix(rhsType, &rhsElemType,
                                                    &rhsNumRows, &rhsNumCols);
  assert(typeTranslator.isSameType(lhsElemType, rhsElemType));
  assert(lhsIsMat && rhsIsMat);
  assert(lhsNumCols == rhsNumRows);
  (void)rhsIsMat;
  (void)lhsIsMat;

  const uint32_t rhsTranspose = processNonFpMatrixTranspose(rhsType, rhsId);

  const auto vecType = astContext.getExtVectorType(lhsElemType, lhsNumCols);
  const auto vecTypeId = typeTranslator.translateType(vecType);
  llvm::SmallVector<uint32_t, 4> resultRows;
  for (uint32_t row = 0; row < lhsNumRows; ++row) {
    const auto rowId =
        theBuilder.createCompositeExtract(vecTypeId, lhsId, {row});
    resultRows.push_back(processNonFpVectorTimesMatrix(vecType, rowId, rhsType,
                                                       rhsId, rhsTranspose));
  }

  // The resulting matrix will have 'lhsNumRows' rows and 'rhsNumCols' columns.
  const auto elemTypeId = typeTranslator.translateType(lhsElemType);
  const auto resultNumRows = theBuilder.getConstantUint32(lhsNumRows);
  const auto resultColType = theBuilder.getVecType(elemTypeId, rhsNumCols);
  const auto resultType = theBuilder.getArrayType(resultColType, resultNumRows);
  return theBuilder.createCompositeConstruct(resultType, resultRows);
}

uint32_t SPIRVEmitter::processIntrinsicMul(const CallExpr *callExpr) {
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
  {
    QualType elemType = {};
    if (TypeTranslator::isScalarType(arg0Type) &&
        TypeTranslator::isMxNMatrix(arg1Type, &elemType)) {
      // OpMatrixTimesScalar can only be used if *both* the matrix element type
      // and the scalar type are float.
      if (arg0Type->isFloatingType() && elemType->isFloatingType())
        return theBuilder.createBinaryOp(spv::Op::OpMatrixTimesScalar,
                                         returnTypeId, arg1Id, arg0Id);
      else
        return processNonFpScalarTimesMatrix(arg0Type, arg0Id, arg1Type,
                                             arg1Id);
    }
  }

  // mul(matrix, scalar)
  {
    QualType elemType = {};
    if (TypeTranslator::isScalarType(arg1Type) &&
        TypeTranslator::isMxNMatrix(arg0Type, &elemType)) {
      // OpMatrixTimesScalar can only be used if *both* the matrix element type
      // and the scalar type are float.
      if (arg1Type->isFloatingType() && elemType->isFloatingType())
        return theBuilder.createBinaryOp(spv::Op::OpMatrixTimesScalar,
                                         returnTypeId, arg0Id, arg1Id);
      else
        return processNonFpScalarTimesMatrix(arg1Type, arg1Id, arg0Type,
                                             arg0Id);
    }
  }

  // mul(vector, matrix)
  {
    QualType vecElemType = {}, matElemType = {};
    uint32_t elemCount = 0, numRows = 0;
    if (TypeTranslator::isVectorType(arg0Type, &vecElemType, &elemCount) &&
        TypeTranslator::isMxNMatrix(arg1Type, &matElemType, &numRows)) {
      assert(elemCount == numRows);

      if (vecElemType->isFloatingType() && matElemType->isFloatingType())
        return theBuilder.createBinaryOp(spv::Op::OpMatrixTimesVector,
                                         returnTypeId, arg1Id, arg0Id);
      else
        return processNonFpVectorTimesMatrix(arg0Type, arg0Id, arg1Type,
                                             arg1Id);
    }
  }

  // mul(matrix, vector)
  {
    QualType vecElemType = {}, matElemType = {};
    uint32_t elemCount = 0, numCols = 0;
    if (TypeTranslator::isMxNMatrix(arg0Type, &matElemType, nullptr,
                                    &numCols) &&
        TypeTranslator::isVectorType(arg1Type, &vecElemType, &elemCount)) {
      assert(elemCount == numCols);
      if (vecElemType->isFloatingType() && matElemType->isFloatingType())
        return theBuilder.createBinaryOp(spv::Op::OpVectorTimesMatrix,
                                         returnTypeId, arg1Id, arg0Id);
      else
        return processNonFpMatrixTimesVector(arg0Type, arg0Id, arg1Type,
                                             arg1Id);
    }
  }

  // mul(matrix, matrix)
  {
    // The front-end ensures that the two matrix element types match.
    QualType elemType = {};
    uint32_t lhsCols = 0, rhsRows = 0;
    if (TypeTranslator::isMxNMatrix(arg0Type, &elemType, nullptr, &lhsCols) &&
        TypeTranslator::isMxNMatrix(arg1Type, nullptr, &rhsRows, nullptr)) {
      assert(lhsCols == rhsRows);
      if (elemType->isFloatingType())
        return theBuilder.createBinaryOp(spv::Op::OpMatrixTimesMatrix,
                                         returnTypeId, arg1Id, arg0Id);
      else
        return processNonFpMatrixTimesMatrix(arg0Type, arg0Id, arg1Type,
                                             arg1Id);
    }
  }

  emitError("invalid argument type passed to mul intrinsic function",
            callExpr->getExprLoc());
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
  (void)vec0ComponentType;
  (void)vec1ComponentType;
  (void)vec1Size;

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
    const auto actOnEachVec = [this, vecOne](uint32_t /*index*/,
                                             uint32_t vecType,
                                             uint32_t curRowId) {
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
  // This function handles 'asint', 'asuint', 'asfloat', and 'asdouble'.

  // Method 1: ret asint(arg)
  //    arg component type = {float, uint}
  //    arg template  type = {scalar, vector, matrix}
  //    ret template  type = same as arg template type.
  //    ret component type = int

  // Method 2: ret asuint(arg)
  //    arg component type = {float, int}
  //    arg template  type = {scalar, vector, matrix}
  //    ret template  type = same as arg template type.
  //    ret component type = uint

  // Method 3: ret asfloat(arg)
  //    arg component type = {float, uint, int}
  //    arg template  type = {scalar, vector, matrix}
  //    ret template  type = same as arg template type.
  //    ret component type = float

  // Method 4: double  asdouble(uint lowbits, uint highbits)
  // Method 5: double2 asdouble(uint2 lowbits, uint2 highbits)
  // Method 6:
  //           void asuint(
  //           in  double value,
  //           out uint lowbits,
  //           out uint highbits
  //           );

  const QualType returnType = callExpr->getType();
  const uint32_t numArgs = callExpr->getNumArgs();
  const uint32_t returnTypeId = typeTranslator.translateType(returnType);
  const Expr *arg0 = callExpr->getArg(0);
  const QualType argType = arg0->getType();

  // Method 3 return type may be the same as arg type, so it would be a no-op.
  if (typeTranslator.isSameType(returnType, argType))
    return doExpr(arg0);

  switch (numArgs) {
  case 1: {
    // Handling Method 1, 2, and 3.
    const auto argId = doExpr(arg0);
    QualType fromElemType = {};
    uint32_t numRows = 0, numCols = 0;
    // For non-matrix arguments (scalar or vector), just do an OpBitCast.
    if (!TypeTranslator::isMxNMatrix(argType, &fromElemType, &numRows,
                                     &numCols)) {
      return theBuilder.createUnaryOp(spv::Op::OpBitcast, returnTypeId, argId);
    }

    // Input or output type is a matrix.
    const QualType toElemType = hlsl::GetHLSLMatElementType(returnType);
    llvm::SmallVector<uint32_t, 4> castedRows;
    const auto fromVecQualType =
        astContext.getExtVectorType(fromElemType, numCols);
    const auto toVecQualType = astContext.getExtVectorType(toElemType, numCols);
    const auto fromVecTypeId = typeTranslator.translateType(fromVecQualType);
    const auto toVecTypeId = typeTranslator.translateType(toVecQualType);
    for (uint32_t row = 0; row < numRows; ++row) {
      const auto rowId =
          theBuilder.createCompositeExtract(fromVecTypeId, argId, {row});
      castedRows.push_back(
          theBuilder.createUnaryOp(spv::Op::OpBitcast, toVecTypeId, rowId));
    }
    return theBuilder.createCompositeConstruct(returnTypeId, castedRows);
  }
  case 2: {
    const uint32_t lowbits = doExpr(arg0);
    const uint32_t highbits = doExpr(callExpr->getArg(1));
    const uint32_t uintType = theBuilder.getUint32Type();
    const uint32_t doubleType = theBuilder.getFloat64Type();
    // Handling Method 4
    if (argType->isUnsignedIntegerType()) {
      const uint32_t uintVec2Type = theBuilder.getVecType(uintType, 2);
      const uint32_t operand = theBuilder.createCompositeConstruct(
          uintVec2Type, {lowbits, highbits});
      return theBuilder.createUnaryOp(spv::Op::OpBitcast, doubleType, operand);
    }
    // Handling Method 5
    else {
      const uint32_t uintVec4Type = theBuilder.getVecType(uintType, 4);
      const uint32_t doubleVec2Type = theBuilder.getVecType(doubleType, 2);
      const uint32_t operand = theBuilder.createVectorShuffle(
          uintVec4Type, lowbits, highbits, {0, 2, 1, 3});
      return theBuilder.createUnaryOp(spv::Op::OpBitcast, doubleVec2Type,
                                      operand);
    }
  }
  case 3: {
    // Handling Method 6.
    const uint32_t value = doExpr(arg0);
    const uint32_t lowbits = doExpr(callExpr->getArg(1));
    const uint32_t highbits = doExpr(callExpr->getArg(2));
    const uint32_t uintType = theBuilder.getUint32Type();
    const uint32_t uintVec2Type = theBuilder.getVecType(uintType, 2);
    const uint32_t vecResult =
        theBuilder.createUnaryOp(spv::Op::OpBitcast, uintVec2Type, value);
    theBuilder.createStore(
        lowbits, theBuilder.createCompositeExtract(uintType, vecResult, {0}));
    theBuilder.createStore(
        highbits, theBuilder.createCompositeExtract(uintType, vecResult, {1}));
    return 0;
  }
  default:
    emitError("unrecognized signature for %0 intrinsic function",
              callExpr->getExprLoc())
        << callExpr->getDirectCallee()->getName();
    return 0;
  }
}

uint32_t SPIRVEmitter::processD3DCOLORtoUBYTE4(const CallExpr *callExpr) {
  // Should take a float4 and return an int4 by doing:
  // int4 result = input.zyxw * 255.001953;
  // Maximum float precision makes the scaling factor 255.002.
  const auto arg = callExpr->getArg(0);
  const auto argId = doExpr(arg);
  const auto argTypeId = typeTranslator.translateType(arg->getType());
  const auto swizzle =
      theBuilder.createVectorShuffle(argTypeId, argId, argId, {2, 1, 0, 3});
  const auto scaled = theBuilder.createBinaryOp(
      spv::Op::OpVectorTimesScalar, argTypeId, swizzle,
      theBuilder.getConstantFloat32(255.002f));
  return castToInt(scaled, arg->getType(), callExpr->getType(),
                   callExpr->getExprLoc());
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
                                  uint32_t /*index*/, uint32_t vecType,
                                  uint32_t curRowId) {
      return theBuilder.createExtInst(vecType, glslInstSetId,
                                      GLSLstd450::GLSLstd450FClamp,
                                      {curRowId, vecZero, vecOne});
    };
    return processEachVectorInMatrix(arg, argId, actOnEachVec);
  }

  emitError("invalid argument type passed to saturate intrinsic function",
            callExpr->getExprLoc());
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
  if (TypeTranslator::isMxNMatrix(argType)) {
    const auto actOnEachVec = [this, glslInstSetId](uint32_t /*index*/,
                                                    uint32_t vecType,
                                                    uint32_t curRowId) {
      return theBuilder.createExtInst(vecType, glslInstSetId,
                                      GLSLstd450::GLSLstd450FSign, {curRowId});
    };
    floatSignResultId = processEachVectorInMatrix(arg, argId, actOnEachVec);
  } else {
    floatSignResultId = theBuilder.createExtInst(
        argTypeId, glslInstSetId, GLSLstd450::GLSLstd450FSign, {argId});
  }

  return castToInt(floatSignResultId, arg->getType(), returnType,
                   arg->getExprLoc());
}

uint32_t SPIRVEmitter::processIntrinsicF16ToF32(const CallExpr *callExpr) {
  // f16tof32() takes in (vector of) uint and returns (vector of) float.
  // The frontend should guarantee that by inserting implicit casts.
  const uint32_t glsl = theBuilder.getGLSLExtInstSet();
  const uint32_t f32TypeId = theBuilder.getFloat32Type();
  const uint32_t u32TypeId = theBuilder.getUint32Type();
  const uint32_t v2f32TypeId = theBuilder.getVecType(f32TypeId, 2);

  const auto *arg = callExpr->getArg(0);
  const uint32_t argId = doExpr(arg);

  uint32_t elemCount = {};

  if (TypeTranslator::isVectorType(arg->getType(), nullptr, &elemCount)) {
    // The input is a vector. We need to handle each element separately.
    llvm::SmallVector<uint32_t, 4> elements;

    for (uint32_t i = 0; i < elemCount; ++i) {
      const uint32_t srcElem =
          theBuilder.createCompositeExtract(u32TypeId, argId, {i});
      const uint32_t convert = theBuilder.createExtInst(
          v2f32TypeId, glsl, GLSLstd450::GLSLstd450UnpackHalf2x16, srcElem);
      elements.push_back(
          theBuilder.createCompositeExtract(f32TypeId, convert, {0}));
    }
    return theBuilder.createCompositeConstruct(
        theBuilder.getVecType(f32TypeId, elemCount), elements);
  }

  const uint32_t convert = theBuilder.createExtInst(
      v2f32TypeId, glsl, GLSLstd450::GLSLstd450UnpackHalf2x16, argId);
  // f16tof32() converts the float16 stored in the low-half of the uint to
  // a float. So just need to return the first component.
  return theBuilder.createCompositeExtract(f32TypeId, convert, {0});
}

uint32_t SPIRVEmitter::processIntrinsicF32ToF16(const CallExpr *callExpr) {
  // f32tof16() takes in (vector of) float and returns (vector of) uint.
  // The frontend should guarantee that by inserting implicit casts.
  const uint32_t glsl = theBuilder.getGLSLExtInstSet();
  const uint32_t f32TypeId = theBuilder.getFloat32Type();
  const uint32_t u32TypeId = theBuilder.getUint32Type();
  const uint32_t v2f32TypeId = theBuilder.getVecType(f32TypeId, 2);
  const uint32_t zero = theBuilder.getConstantFloat32(0);

  const auto *arg = callExpr->getArg(0);
  const uint32_t argId = doExpr(arg);
  uint32_t elemCount = {};

  if (TypeTranslator::isVectorType(arg->getType(), nullptr, &elemCount)) {
    // The input is a vector. We need to handle each element separately.
    llvm::SmallVector<uint32_t, 4> elements;

    for (uint32_t i = 0; i < elemCount; ++i) {
      const uint32_t srcElem =
          theBuilder.createCompositeExtract(f32TypeId, argId, {i});
      const uint32_t srcVec =
          theBuilder.createCompositeConstruct(v2f32TypeId, {srcElem, zero});

      elements.push_back(theBuilder.createExtInst(
          u32TypeId, glsl, GLSLstd450::GLSLstd450PackHalf2x16, srcVec));
    }
    return theBuilder.createCompositeConstruct(
        theBuilder.getVecType(u32TypeId, elemCount), elements);
  }

  // f16tof32() stores the float into the low-half of the uint. So we need
  // to supply another zero to take the other half.
  const uint32_t srcVec =
      theBuilder.createCompositeConstruct(v2f32TypeId, {argId, zero});
  return theBuilder.createExtInst(u32TypeId, glsl,
                                  GLSLstd450::GLSLstd450PackHalf2x16, srcVec);
}

uint32_t SPIRVEmitter::processIntrinsicUsingSpirvInst(
    const CallExpr *callExpr, spv::Op opcode, bool actPerRowForMatrices) {
  // Certain opcodes are only allowed in pixel shader
  if (!shaderModel.IsPS())
    switch (opcode) {
    case spv::Op::OpDPdx:
    case spv::Op::OpDPdy:
    case spv::Op::OpDPdxFine:
    case spv::Op::OpDPdyFine:
    case spv::Op::OpDPdxCoarse:
    case spv::Op::OpDPdyCoarse:
    case spv::Op::OpFwidth:
    case spv::Op::OpFwidthFine:
    case spv::Op::OpFwidthCoarse:
      needsLegalization = true;
      break;
    default:
      // Only the given opcodes need legalization. Anything else should preserve
      // previous.
      break;
    }

  const uint32_t returnType = typeTranslator.translateType(callExpr->getType());
  if (callExpr->getNumArgs() == 1u) {
    const Expr *arg = callExpr->getArg(0);
    const uint32_t argId = doExpr(arg);

    // If the instruction does not operate on matrices, we can perform the
    // instruction on each vector of the matrix.
    if (actPerRowForMatrices && TypeTranslator::isMxNMatrix(arg->getType())) {
      const auto actOnEachVec = [this, opcode](uint32_t /*index*/,
                                               uint32_t vecType,
                                               uint32_t curRowId) {
        return theBuilder.createUnaryOp(opcode, vecType, curRowId);
      };
      return processEachVectorInMatrix(arg, argId, actOnEachVec);
    }
    return theBuilder.createUnaryOp(opcode, returnType, argId);
  } else if (callExpr->getNumArgs() == 2u) {
    const Expr *arg0 = callExpr->getArg(0);
    const uint32_t arg0Id = doExpr(arg0);
    const uint32_t arg1Id = doExpr(callExpr->getArg(1));
    // If the instruction does not operate on matrices, we can perform the
    // instruction on each vector of the matrix.
    if (actPerRowForMatrices && TypeTranslator::isMxNMatrix(arg0->getType())) {
      const auto actOnEachVec = [this, opcode, arg1Id](uint32_t index,
                                                       uint32_t vecType,
                                                       uint32_t arg0RowId) {
        const uint32_t arg1RowId =
            theBuilder.createCompositeExtract(vecType, arg1Id, {index});
        return theBuilder.createBinaryOp(opcode, vecType, arg0RowId, arg1RowId);
      };
      return processEachVectorInMatrix(arg0, arg0Id, actOnEachVec);
    }
    return theBuilder.createBinaryOp(opcode, returnType, arg0Id, arg1Id);
  }

  emitError("unsupported %0 intrinsic function", callExpr->getExprLoc())
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
    if (actPerRowForMatrices && TypeTranslator::isMxNMatrix(arg->getType())) {
      const auto actOnEachVec = [this, glslInstSetId,
                                 opcode](uint32_t /*index*/, uint32_t vecType,
                                         uint32_t curRowId) {
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
    if (actPerRowForMatrices && TypeTranslator::isMxNMatrix(arg0->getType())) {
      const auto actOnEachVec = [this, glslInstSetId, opcode,
                                 arg1Id](uint32_t index, uint32_t vecType,
                                         uint32_t arg0RowId) {
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
    if (actPerRowForMatrices && TypeTranslator::isMxNMatrix(arg0->getType())) {
      const auto actOnEachVec = [this, glslInstSetId, opcode, arg1Id,
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

  emitError("unsupported %0 intrinsic function", callExpr->getExprLoc())
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

  {
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (TypeTranslator::isMxNMatrix(type, &elemType, &rowCount, &colCount)) {
      const auto row = getVecValueZero(elemType, colCount);
      llvm::SmallVector<uint32_t, 4> rows((size_t)rowCount, row);
      return theBuilder.createCompositeConstruct(
          typeTranslator.translateType(type), rows);
    }
  }

  emitError("getting value 0 for type %0 unimplemented", {})
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

      if (scalarType->isSpecificBuiltinType(BuiltinType::LitFloat))
        scalarType = typeTranslator.getIntendedLiteralType(scalarType);
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

  emitError("getting value 1 for type %0 unimplemented", {}) << type;
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

uint32_t SPIRVEmitter::getMaskForBitwidthValue(QualType type) {
  QualType elemType = {};
  uint32_t count = 1;

  if (TypeTranslator::isScalarType(type, &elemType) ||
      TypeTranslator::isVectorType(type, &elemType, &count)) {
    const auto bitwidth = typeTranslator.getElementSpirvBitwidth(elemType);
    uint32_t mask = 0;
    uint32_t elemTypeId = 0;
    switch (bitwidth) {
    case 16:
      mask = theBuilder.getConstantUint16(bitwidth - 1);
      elemTypeId = theBuilder.getUint16Type();
      break;
    case 32:
      mask = theBuilder.getConstantUint32(bitwidth - 1);
      elemTypeId = theBuilder.getUint32Type();
      break;
    case 64:
      mask = theBuilder.getConstantUint64(bitwidth - 1);
      elemTypeId = theBuilder.getUint64Type();
      break;
    default:
      assert(false && "this method only supports 16-, 32-, and 64-bit types");
    }

    if (count == 1)
      return mask;

    const uint32_t typeId = theBuilder.getVecType(elemTypeId, count);
    llvm::SmallVector<uint32_t, 4> elements(size_t(count), mask);
    return theBuilder.getConstantComposite(typeId, elements);
  }

  assert(false && "this method only supports scalars and vectors");
  return 0;
}

uint32_t SPIRVEmitter::translateAPValue(const APValue &value,
                                        const QualType targetType) {
  uint32_t result = 0;

  // Provide a hint to the typeTranslator that if a literal is discovered, its
  // intended usage is targetType.
  TypeTranslator::LiteralTypeHint hint(typeTranslator, targetType);

  if (targetType->isBooleanType()) {
    result = theBuilder.getConstantBool(value.getInt().getBoolValue(),
                                        isSpecConstantMode);
  } else if (targetType->isIntegerType()) {
    result = translateAPInt(value.getInt(), targetType);
  } else if (targetType->isFloatingType()) {
    result = translateAPFloat(value.getFloat(), targetType);
  } else if (hlsl::IsHLSLVecType(targetType)) {
    const uint32_t vecType = typeTranslator.translateType(targetType);
    const QualType elemType = hlsl::GetHLSLVecElementType(targetType);
    const auto numElements = value.getVectorLength();
    // Special case for vectors of size 1. SPIR-V doesn't support this vector
    // size so we need to translate it to scalar values.
    if (numElements == 1) {
      result = translateAPValue(value.getVectorElt(0), elemType);
    } else {
      llvm::SmallVector<uint32_t, 4> elements;
      for (uint32_t i = 0; i < numElements; ++i) {
        elements.push_back(translateAPValue(value.getVectorElt(i), elemType));
      }
      result = theBuilder.getConstantComposite(vecType, elements);
    }
  }

  if (result)
    return result;

  emitError("APValue of type %0 unimplemented", {}) << value.getKind();
  value.dump();
  return 0;
}

uint32_t SPIRVEmitter::translateAPInt(const llvm::APInt &intValue,
                                      QualType targetType) {
  targetType = typeTranslator.getIntendedLiteralType(targetType);
  const auto targetTypeBitWidth = astContext.getTypeSize(targetType);
  const bool isSigned = targetType->isSignedIntegerType();
  switch (targetTypeBitWidth) {
  case 16: {
    if (spirvOptions.enable16BitTypes) {
      if (isSigned) {
        return theBuilder.getConstantInt16(
            static_cast<int16_t>(intValue.getSExtValue()));
      } else {
        return theBuilder.getConstantUint16(
            static_cast<uint16_t>(intValue.getZExtValue()));
      }
    } else {
      // If enable16BitTypes option is not true, treat as 32-bit integer.
      if (isSigned)
        return theBuilder.getConstantInt32(
            static_cast<int32_t>(intValue.getSExtValue()), isSpecConstantMode);
      else
        return theBuilder.getConstantUint32(
            static_cast<uint32_t>(intValue.getZExtValue()), isSpecConstantMode);
    }
  }
  case 32: {
    if (isSigned) {
      if (!intValue.isSignedIntN(32)) {
        emitError("evaluating integer literal %0 as a 32-bit integer loses "
                  "inforamtion",
                  {})
            << std::to_string(intValue.getSExtValue());
        return 0;
      }
      return theBuilder.getConstantInt32(
          static_cast<int32_t>(intValue.getSExtValue()), isSpecConstantMode);
    } else {
      if (!intValue.isIntN(32)) {
        emitError("evaluating integer literal %0 as a 32-bit integer loses "
                  "inforamtion",
                  {})
            << std::to_string(intValue.getZExtValue());
        return 0;
      }
      return theBuilder.getConstantUint32(
          static_cast<uint32_t>(intValue.getZExtValue()), isSpecConstantMode);
    }
  }
  case 64: {
    if (isSigned)
      return theBuilder.getConstantInt64(intValue.getSExtValue());
    else
      return theBuilder.getConstantUint64(intValue.getZExtValue());
  }
  }

  emitError("APInt for target bitwidth %0 unimplemented", {})
      << astContext.getIntWidth(targetType);

  return 0;
}

bool SPIRVEmitter::isLiteralLargerThan32Bits(const Expr *expr) {
  if (const auto *intLiteral = dyn_cast<IntegerLiteral>(expr)) {
    const bool isSigned = expr->getType()->isSignedIntegerType();
    const llvm::APInt &value = intLiteral->getValue();
    return (isSigned && !value.isSignedIntN(32)) ||
           (!isSigned && !value.isIntN(32));
  }

  if (const auto *floatLiteral = dyn_cast<FloatingLiteral>(expr)) {
    llvm::APFloat value = floatLiteral->getValue();
    const auto &semantics = value.getSemantics();
    // regular 'half' and 'float' can be represented in 32 bits.
    if (&semantics == &llvm::APFloat::IEEEsingle ||
        &semantics == &llvm::APFloat::IEEEhalf)
      return true;

    // See if 'double' value can be represented in 32 bits without losing info.
    bool losesInfo = false;
    const auto convertStatus =
        value.convert(llvm::APFloat::IEEEsingle,
                      llvm::APFloat::rmNearestTiesToEven, &losesInfo);
    if (convertStatus != llvm::APFloat::opOK &&
        convertStatus != llvm::APFloat::opInexact)
      return true;
  }

  return false;
}

uint32_t SPIRVEmitter::tryToEvaluateAsInt32(const llvm::APInt &intValue,
                                            bool isSigned) {
  if (isSigned && intValue.isSignedIntN(32)) {
    return theBuilder.getConstantInt32(
        static_cast<int32_t>(intValue.getSExtValue()));
  }
  if (!isSigned && intValue.isIntN(32)) {
    return theBuilder.getConstantUint32(
        static_cast<uint32_t>(intValue.getZExtValue()));
  }

  // Couldn't evaluate as a 32-bit int without losing information.
  return 0;
}

uint32_t SPIRVEmitter::tryToEvaluateAsFloat32(const llvm::APFloat &floatValue) {
  const auto &semantics = floatValue.getSemantics();
  // If the given value is already a 32-bit float, there is no need to convert.
  if (&semantics == &llvm::APFloat::IEEEsingle) {
    return theBuilder.getConstantFloat32(floatValue.convertToFloat(),
                                         isSpecConstantMode);
  }

  // Try to see if this literal float can be represented in 32-bit.
  // Since the convert function below may modify the fp value, we call it on a
  // temporary copy.
  llvm::APFloat eval = floatValue;
  bool losesInfo = false;
  const auto convertStatus =
      eval.convert(llvm::APFloat::IEEEsingle,
                   llvm::APFloat::rmNearestTiesToEven, &losesInfo);
  if (convertStatus == llvm::APFloat::opOK && !losesInfo)
    return theBuilder.getConstantFloat32(eval.convertToFloat());

  // Couldn't evaluate as a 32-bit float without losing information.
  return 0;
}

uint32_t SPIRVEmitter::translateAPFloat(llvm::APFloat floatValue,
                                        QualType targetType) {
  using llvm::APFloat;
  const auto originalValue = floatValue;
  const auto valueBitwidth = APFloat::getSizeInBits(floatValue.getSemantics());

  // Find out the target bitwidth.
  targetType = typeTranslator.getIntendedLiteralType(targetType);
  auto targetBitwidth =
      APFloat::getSizeInBits(astContext.getFloatTypeSemantics(targetType));
  // If 16-bit types are not enabled, treat them as 32-bit float.
  if (targetBitwidth == 16 && !spirvOptions.enable16BitTypes)
    targetBitwidth = 32;

  if (targetBitwidth != valueBitwidth) {
    bool losesInfo = false;
    const llvm::fltSemantics &targetSemantics =
        targetBitwidth == 16
            ? APFloat::IEEEhalf
            : targetBitwidth == 32 ? APFloat::IEEEsingle : APFloat::IEEEdouble;
    const auto status = floatValue.convert(
        targetSemantics, APFloat::roundingMode::rmTowardZero, &losesInfo);
    if (status != APFloat::opStatus::opOK &&
        status != APFloat::opStatus::opInexact) {
      emitError(
          "evaluating float literal %0 at a lower bitwidth loses information",
          {})
          // Converting from 16bit to 32/64-bit won't lose information.
          // So only 32/64-bit values can reach here.
          << std::to_string(valueBitwidth == 32
                                ? originalValue.convertToFloat()
                                : originalValue.convertToDouble());
      return 0;
    }
  }

  switch (targetBitwidth) {
  case 16:
    return theBuilder.getConstantFloat16(
        static_cast<uint16_t>(floatValue.bitcastToAPInt().getZExtValue()));
  case 32:
    return theBuilder.getConstantFloat32(floatValue.convertToFloat(),
                                         isSpecConstantMode);
  case 64:
    return theBuilder.getConstantFloat64(floatValue.convertToDouble());
  default:
    break;
  }
  emitError("APFloat for target bitwidth %0 unimplemented", {})
      << targetBitwidth;
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

bool SPIRVEmitter::processGeometryShaderAttributes(const FunctionDecl *decl,
                                                   uint32_t *arraySize) {
  bool success = true;
  assert(shaderModel.IsGS());
  if (auto *vcAttr = decl->getAttr<HLSLMaxVertexCountAttr>()) {
    theBuilder.addExecutionMode(entryFunctionId,
                                spv::ExecutionMode::OutputVertices,
                                {static_cast<uint32_t>(vcAttr->getCount())});
  }

  uint32_t invocations = 1;
  if (auto *instanceAttr = decl->getAttr<HLSLInstanceAttr>()) {
    invocations = static_cast<uint32_t>(instanceAttr->getCount());
  }
  theBuilder.addExecutionMode(entryFunctionId, spv::ExecutionMode::Invocations,
                              {invocations});

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
    if (param->hasAttr<HLSLPointAttr>() && !inPoint) {
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::InputPoints, {});
      *arraySize = 1;
      inPoint = true;
    } else if (param->hasAttr<HLSLLineAttr>() && !inLine) {
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::InputLines, {});
      *arraySize = 2;
      inLine = true;
    } else if (param->hasAttr<HLSLTriangleAttr>() && !inTriangle) {
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::Triangles, {});
      *arraySize = 3;
      inTriangle = true;
    } else if (param->hasAttr<HLSLLineAdjAttr>() && !inLineAdj) {
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::InputLinesAdjacency, {});
      *arraySize = 4;
      inLineAdj = true;
    } else if (param->hasAttr<HLSLTriangleAdjAttr>() && !inTriangleAdj) {
      theBuilder.addExecutionMode(
          entryFunctionId, spv::ExecutionMode::InputTrianglesAdjacency, {});
      *arraySize = 6;
      inTriangleAdj = true;
    }
  }
  if (inPoint + inLine + inLineAdj + inTriangle + inTriangleAdj > 1) {
    emitError("only one input primitive type can be specified in the geometry "
              "shader",
              {});
    success = false;
  }
  if (outPoint + outTriangle + outLine > 1) {
    emitError("only one output primitive type can be specified in the geometry "
              "shader",
              {});
    success = false;
  }

  return success;
}

void SPIRVEmitter::processPixelShaderAttributes(const FunctionDecl *decl) {
  theBuilder.addExecutionMode(entryFunctionId,
                              spv::ExecutionMode::OriginUpperLeft, {});
  if (decl->getAttr<HLSLEarlyDepthStencilAttr>()) {
    theBuilder.addExecutionMode(entryFunctionId,
                                spv::ExecutionMode::EarlyFragmentTests, {});
  }
}

void SPIRVEmitter::processComputeShaderAttributes(const FunctionDecl *decl) {
  // If not explicitly specified, x, y, and z should be defaulted to 1.
  uint32_t x = 1, y = 1, z = 1;

  if (auto *numThreadsAttr = decl->getAttr<HLSLNumThreadsAttr>()) {
    x = static_cast<uint32_t>(numThreadsAttr->getX());
    y = static_cast<uint32_t>(numThreadsAttr->getY());
    z = static_cast<uint32_t>(numThreadsAttr->getZ());
  }

  theBuilder.addExecutionMode(entryFunctionId, spv::ExecutionMode::LocalSize,
                              {x, y, z});
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
                domain->getLocation());
      return false;
    }
    theBuilder.addExecutionMode(entryFunctionId, hsExecMode, {});
  }

  // Early return for domain shaders as domain shaders only takes the 'domain'
  // attribute.
  if (shaderModel.IsDS())
    return true;

  if (auto *partitioning = decl->getAttr<HLSLPartitioningAttr>()) {
    const auto scheme = partitioning->getScheme().lower();
    if (scheme == "pow2") {
      emitError("pow2 partitioning scheme is not supported since there is no "
                "equivalent in Vulkan",
                partitioning->getLocation());
      return false;
    }
    const ExecutionMode hsExecMode =
        llvm::StringSwitch<ExecutionMode>(scheme)
            .Case("fractional_even", ExecutionMode::SpacingFractionalEven)
            .Case("fractional_odd", ExecutionMode::SpacingFractionalOdd)
            .Case("integer", ExecutionMode::SpacingEqual)
            .Default(ExecutionMode::Max);
    if (hsExecMode == ExecutionMode::Max) {
      emitError("unknown partitioning scheme in hull shader",
                partitioning->getLocation());
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
                  outputTopology->getLocation());
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
  if (auto *pcf = decl->getAttr<HLSLPatchConstantFuncAttr>()) {
    llvm::StringRef pcf_name = pcf->getFunctionName();
    for (auto *decl : astContext.getTranslationUnitDecl()->decls())
      if (auto *funcDecl = dyn_cast<FunctionDecl>(decl))
        if (astContext.IsPatchConstantFunctionDecl(funcDecl) &&
            funcDecl->getName() == pcf_name)
          patchConstFunc = funcDecl;
  }

  return true;
}

bool SPIRVEmitter::emitEntryFunctionWrapper(const FunctionDecl *decl,
                                            const uint32_t entryFuncId) {
  // HS specific attributes
  uint32_t numOutputControlPoints = 0;
  uint32_t outputControlPointIdVal = 0; // SV_OutputControlPointID value
  uint32_t primitiveIdVar = 0;          // SV_PrimitiveID variable
  uint32_t viewIdVar = 0;               // SV_ViewID variable
  uint32_t hullMainInputPatchParam = 0; // Temporary parameter for InputPatch<>

  // The array size of per-vertex input/output variables
  // Used by HS/DS/GS for the additional arrayness, zero means not an array.
  uint32_t inputArraySize = 0;
  uint32_t outputArraySize = 0;

  // Construct the wrapper function signature.
  const uint32_t voidType = theBuilder.getVoidType();
  const uint32_t funcType = theBuilder.getFunctionType(voidType, {});

  // The wrapper entry function surely does not have pre-assigned <result-id>
  // for it like other functions that got added to the work queue following
  // function calls. And the wrapper is the entry function.
  entryFunctionId =
      theBuilder.beginFunction(funcType, voidType, decl->getName());
  // Note this should happen before using declIdMapper for other tasks.
  declIdMapper.setEntryFunctionId(entryFunctionId);

  // Handle attributes specific to each shader stage
  if (shaderModel.IsPS()) {
    processPixelShaderAttributes(decl);
  } else if (shaderModel.IsCS()) {
    processComputeShaderAttributes(decl);
  } else if (shaderModel.IsHS()) {
    if (!processTessellationShaderAttributes(decl, &numOutputControlPoints))
      return false;

    // The input array size for HS is specified in the InputPatch parameter.
    for (const auto *param : decl->params())
      if (hlsl::IsHLSLInputPatchType(param->getType())) {
        inputArraySize = hlsl::GetHLSLInputPatchCount(param->getType());
        break;
      }

    outputArraySize = numOutputControlPoints;
  } else if (shaderModel.IsDS()) {
    if (!processTessellationShaderAttributes(decl, &numOutputControlPoints))
      return false;

    // The input array size for HS is specified in the OutputPatch parameter.
    for (const auto *param : decl->params())
      if (hlsl::IsHLSLOutputPatchType(param->getType())) {
        inputArraySize = hlsl::GetHLSLOutputPatchCount(param->getType());
        break;
      }
    // The per-vertex output of DS is not an array.
  } else if (shaderModel.IsGS()) {
    if (!processGeometryShaderAttributes(decl, &inputArraySize))
      return false;
    // The per-vertex output of GS is not an array.
  }

  // Go through all parameters and record the declaration of SV_ClipDistance
  // and SV_CullDistance. We need to do this extra step because in HLSL we
  // can declare multiple SV_ClipDistance/SV_CullDistance variables of float
  // or vector of float types, but we can only have one single float array
  // for the ClipDistance/CullDistance builtin. So we need to group all
  // SV_ClipDistance/SV_CullDistance variables into one float array, thus we
  // need to calculate the total size of the array and the offset of each
  // variable within that array.
  // Also go through all parameters to record the semantic strings provided for
  // the builtins in gl_PerVertex.
  for (const auto *param : decl->params()) {
    if (canActAsInParmVar(param))
      if (!declIdMapper.glPerVertex.recordGlPerVertexDeclFacts(param, true))
        return false;
    if (canActAsOutParmVar(param))
      if (!declIdMapper.glPerVertex.recordGlPerVertexDeclFacts(param, false))
        return false;
  }
  // Also consider the SV_ClipDistance/SV_CullDistance in the return type
  if (!declIdMapper.glPerVertex.recordGlPerVertexDeclFacts(decl, false))
    return false;

  // Calculate the total size of the ClipDistance/CullDistance array and the
  // offset of SV_ClipDistance/SV_CullDistance variables within the array.
  declIdMapper.glPerVertex.calculateClipCullDistanceArraySize();

  if (!shaderModel.IsCS()) {
    // Generate stand-alone builtins of Position, ClipDistance, and
    // CullDistance, which belongs to gl_PerVertex.
    declIdMapper.glPerVertex.generateVars(inputArraySize, outputArraySize);
  }

  // Require the ClipDistance/CullDistance capability if necessary.
  // It is legal to just use the ClipDistance/CullDistance builtin without
  // requiring the ClipDistance/CullDistance capability, as long as we don't
  // read or write the builtin variable.
  // For our CodeGen, that corresponds to not seeing SV_ClipDistance or
  // SV_CullDistance at all. If we see them, we will generate code to read
  // them to initialize temporary variable for calling the source code entry
  // function or write to them after calling the source code entry function.
  declIdMapper.glPerVertex.requireCapabilityIfNecessary();

  // The entry basic block.
  const uint32_t entryLabel = theBuilder.createBasicBlock();
  theBuilder.setInsertPoint(entryLabel);

  // Initialize all global variables at the beginning of the wrapper
  for (const VarDecl *varDecl : toInitGloalVars) {
    const auto varInfo = declIdMapper.getDeclEvalInfo(varDecl);
    if (const auto *init = varDecl->getInit()) {
      storeValue(varInfo, doExpr(init), varDecl->getType());

      // Update counter variable associated with global variables
      tryToAssignCounterVar(varDecl, init);
    }
    // If not explicitly initialized, initialize with their zero values if not
    // resource objects
    else if (!hlsl::IsHLSLResourceType(varDecl->getType())) {
      const auto typeId = typeTranslator.translateType(varDecl->getType());
      theBuilder.createStore(varInfo, theBuilder.getConstantNull(typeId));
    }
  }

  // Create temporary variables for holding function call arguments
  llvm::SmallVector<uint32_t, 4> params;
  for (const auto *param : decl->params()) {
    const auto paramType = param->getType();
    const uint32_t typeId = typeTranslator.translateType(paramType);
    std::string tempVarName = "param.var." + param->getNameAsString();
    const uint32_t tempVar = theBuilder.addFnVar(typeId, tempVarName);

    params.push_back(tempVar);

    // Create the stage input variable for parameter not marked as pure out and
    // initialize the corresponding temporary variable
    // Also do not create input variables for output stream objects of geometry
    // shaders (e.g. TriangleStream) which are required to be marked as 'inout'.
    if (canActAsInParmVar(param)) {
      if (shaderModel.IsHS() && hlsl::IsHLSLInputPatchType(paramType)) {
        // Record the temporary variable holding InputPatch. It may be used
        // later in the patch constant function.
        hullMainInputPatchParam = tempVar;
      }

      uint32_t loadedValue = 0;

      if (!declIdMapper.createStageInputVar(param, &loadedValue, false))
        return false;

      theBuilder.createStore(tempVar, loadedValue);

      // Record the temporary variable holding SV_OutputControlPointID,
      // SV_PrimitiveID, and SV_ViewID. It may be used later in the patch
      // constant function.
      if (hasSemantic(param, hlsl::DXIL::SemanticKind::OutputControlPointID))
        outputControlPointIdVal = loadedValue;
      else if (hasSemantic(param, hlsl::DXIL::SemanticKind::PrimitiveID))
        primitiveIdVar = tempVar;
      else if (hasSemantic(param, hlsl::DXIL::SemanticKind::ViewID))
        viewIdVar = tempVar;
    }
  }

  // Call the original entry function
  const uint32_t retType = typeTranslator.translateType(decl->getReturnType());
  const uint32_t retVal =
      theBuilder.createFunctionCall(retType, entryFuncId, params);

  // Create and write stage output variables for return value. Special case for
  // Hull shaders since they operate differently in 2 ways:
  // 1- Their return value is in fact an array and each invocation should write
  //    to the proper offset in the array.
  // 2- The patch constant function must be called *once* after all invocations
  //    of the main entry point function is done.
  if (shaderModel.IsHS()) {
    // Create stage output variables out of the return type.
    if (!declIdMapper.createStageOutputVar(decl, numOutputControlPoints,
                                           outputControlPointIdVal, retVal))
      return false;
    if (!processHSEntryPointOutputAndPCF(
            decl, retType, retVal, numOutputControlPoints,
            outputControlPointIdVal, primitiveIdVar, viewIdVar,
            hullMainInputPatchParam))
      return false;
  } else {
    if (!declIdMapper.createStageOutputVar(decl, retVal, /*forPCF*/ false))
      return false;
  }

  // Create and write stage output variables for parameters marked as
  // out/inout
  for (uint32_t i = 0; i < decl->getNumParams(); ++i) {
    const auto *param = decl->getParamDecl(i);
    if (canActAsOutParmVar(param)) {
      // Load the value from the parameter after function call
      const uint32_t typeId = typeTranslator.translateType(param->getType());
      uint32_t loadedParam = 0;

      // Write back of stage output variables in GS is manually controlled by
      // .Append() intrinsic method. No need to load the parameter since we
      // won't need to write back here.
      if (!shaderModel.IsGS())
        loadedParam = theBuilder.createLoad(typeId, params[i]);

      if (!declIdMapper.createStageOutputVar(param, loadedParam, false))
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

bool SPIRVEmitter::processHSEntryPointOutputAndPCF(
    const FunctionDecl *hullMainFuncDecl, uint32_t retType, uint32_t retVal,
    uint32_t numOutputControlPoints, uint32_t outputControlPointId,
    uint32_t primitiveId, uint32_t viewId, uint32_t hullMainInputPatch) {
  // This method may only be called for Hull shaders.
  assert(shaderModel.IsHS());

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
        "SV_OutputControlPointID semantic must be provided in hull shader",
        hullMainFuncDecl->getLocation());
    return false;
  }
  if (!patchConstFunc) {
    emitError("patch constant function not defined in hull shader",
              hullMainFuncDecl->getLocation());
    return false;
  }

  uint32_t hullMainOutputPatch = 0;
  // If the patch constant function (PCF) takes the result of the Hull main
  // entry point, create a temporary function-scope variable and write the
  // results to it, so it can be passed to the PCF.
  if (patchConstFuncTakesHullOutputPatch(patchConstFunc)) {
    const uint32_t hullMainRetType = theBuilder.getArrayType(
        retType, theBuilder.getConstantUint32(numOutputControlPoints));
    hullMainOutputPatch =
        theBuilder.addFnVar(hullMainRetType, "temp.var.hullMainRetVal");
    const auto tempLocation = theBuilder.createAccessChain(
        theBuilder.getPointerType(retType, spv::StorageClass::Function),
        hullMainOutputPatch, {outputControlPointId});
    theBuilder.createStore(tempLocation, retVal);
  }

  // Now create a barrier before calling the Patch Constant Function (PCF).
  // Flags are:
  // Execution Barrier scope = Workgroup (2)
  // Memory Barrier scope = Invocation (4)
  // Memory Semantics Barrier scope = None (0)
  const auto constZero = theBuilder.getConstantUint32(0);
  const auto constFour = theBuilder.getConstantUint32(4);
  const auto constTwo = theBuilder.getConstantUint32(2);
  theBuilder.createBarrier(constTwo, constFour, constZero);

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

  // A lambda for creating a stage input variable and its associated temporary
  // variable for function call. Also initializes the temporary variable using
  // the contents loaded from the stage input variable. Returns the <result-id>
  // of the temporary variable.
  const auto createParmVarAndInitFromStageInputVar =
      [this](const ParmVarDecl *param) {
        const uint32_t typeId = typeTranslator.translateType(param->getType());
        std::string tempVarName = "param.var." + param->getNameAsString();
        const uint32_t tempVar = theBuilder.addFnVar(typeId, tempVarName);
        uint32_t loadedValue = 0;
        declIdMapper.createStageInputVar(param, &loadedValue, /*forPCF*/ true);
        theBuilder.createStore(tempVar, loadedValue);
        return tempVar;
      };

  for (const auto *param : patchConstFunc->parameters()) {
    // Note: According to the HLSL reference, the PCF takes an InputPatch of
    // ControlPoints as well as the PatchID (PrimitiveID). This does not
    // necessarily mean that they are present. There is also no requirement
    // for the order of parameters passed to PCF.
    if (hlsl::IsHLSLInputPatchType(param->getType())) {
      pcfParams.push_back(hullMainInputPatch);
    } else if (hlsl::IsHLSLOutputPatchType(param->getType())) {
      pcfParams.push_back(hullMainOutputPatch);
    } else if (hasSemantic(param, hlsl::DXIL::SemanticKind::PrimitiveID)) {
      if (!primitiveId) {
        primitiveId = createParmVarAndInitFromStageInputVar(param);
      }
      pcfParams.push_back(primitiveId);
    } else if (hasSemantic(param, hlsl::DXIL::SemanticKind::ViewID)) {
      if (!viewId) {
        viewId = createParmVarAndInitFromStageInputVar(param);
      }
      pcfParams.push_back(viewId);
    } else {
      emitError("patch constant function parameter '%0' unknown",
                param->getLocation())
          << param->getName();
    }
  }
  const uint32_t pcfResultId =
      theBuilder.createFunctionCall(pcfRetType, pcfId, {pcfParams});
  if (!declIdMapper.createStageOutputVar(patchConstFunc, pcfResultId,
                                         /*forPCF*/ true))
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
      emitError(
          "non-32bit integer case value in switch statement unimplemented",
          caseExpr->getExprLoc());
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
    for (unsigned i = curCaseIndex + 1;
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

uint32_t SPIRVEmitter::extractVecFromVec4(uint32_t fromId,
                                          uint32_t targetVecSize,
                                          uint32_t targetElemTypeId) {
  assert(targetVecSize > 0 && targetVecSize < 5);
  const uint32_t retType =
      targetVecSize == 1
          ? targetElemTypeId
          : theBuilder.getVecType(targetElemTypeId, targetVecSize);
  switch (targetVecSize) {
  case 1:
    return theBuilder.createCompositeExtract(retType, fromId, {0});
    break;
  case 2:
    return theBuilder.createVectorShuffle(retType, fromId, fromId, {0, 1});
    break;
  case 3:
    return theBuilder.createVectorShuffle(retType, fromId, fromId, {0, 1, 2});
    break;
  case 4:
    return fromId;
  default:
    llvm_unreachable("vector element count must be 1, 2, 3, or 4");
  }
}

} // end namespace spirv
} // end namespace clang
