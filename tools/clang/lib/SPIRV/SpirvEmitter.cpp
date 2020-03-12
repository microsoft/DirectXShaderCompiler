//===------- SpirvEmitter.cpp - SPIR-V Binary Code Emitter ------*- C++ -*-===//
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

#include "SpirvEmitter.h"

#include "AlignmentSizeCalculator.h"
#include "RawBufferMethods.h"
#include "dxc/HlslIntrinsicOp.h"
#include "spirv-tools/optimizer.hpp"
#include "clang/SPIRV/AstTypeProbe.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/StringExtras.h"

#include "InitListHandler.h"

#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
#include "clang/Basic/Version.h"
#else
namespace clang {
uint32_t getGitCommitCount() { return 0; }
const char *getGitCommitHash() { return "<unknown-hash>"; }
} // namespace clang
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO

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
        if (isStructuredBuffer(object->getType())) {
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
      if (isConstantTextureBuffer(varDecl))
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
      if (isAKindOfStructuredOrByteBuffer(varDecl->getType()))
        return isExternalVar(varDecl);
  } else if (const auto *callExpr = dyn_cast<CallExpr>(expr)) {
    if (isAKindOfStructuredOrByteBuffer(callExpr->getType()))
      return true;
  } else if (const auto *arrSubExpr = dyn_cast<ArraySubscriptExpr>(expr)) {
    return isReferencingNonAliasStructuredOrByteBuffer(arrSubExpr->getBase());
  }
  return false;
}

bool spirvToolsLegalize(spv_target_env env, std::vector<uint32_t> *mod,
                        std::string *messages) {
  spvtools::Optimizer optimizer(env);

  optimizer.SetMessageConsumer(
      [messages](spv_message_level_t /*level*/, const char * /*source*/,
                 const spv_position_t & /*position*/,
                 const char *message) { *messages += message; });

  spvtools::OptimizerOptions options;
  options.set_run_validator(false);

  optimizer.RegisterLegalizationPasses();

  optimizer.RegisterPass(spvtools::CreateReplaceInvalidOpcodePass());

  optimizer.RegisterPass(spvtools::CreateCompactIdsPass());

  return optimizer.Run(mod->data(), mod->size(), mod, options);
}

bool spirvToolsOptimize(spv_target_env env, std::vector<uint32_t> *mod,
                        clang::spirv::SpirvCodeGenOptions &spirvOptions,
                        std::string *messages) {
  spvtools::Optimizer optimizer(env);

  optimizer.SetMessageConsumer(
      [messages](spv_message_level_t /*level*/, const char * /*source*/,
                 const spv_position_t & /*position*/,
                 const char *message) { *messages += message; });

  spvtools::OptimizerOptions options;
  options.set_run_validator(false);

  if (spirvOptions.optConfig.empty()) {
    optimizer.RegisterPerformancePasses();
    if (spirvOptions.flattenResourceArrays)
      optimizer.RegisterPass(spvtools::CreateDescriptorScalarReplacementPass());
    optimizer.RegisterPass(spvtools::CreateCompactIdsPass());
  } else {
    // Command line options use llvm::SmallVector and llvm::StringRef, whereas
    // SPIR-V optimizer uses std::vector and std::string.
    std::vector<std::string> stdFlags;
    for (const auto &f : spirvOptions.optConfig)
      stdFlags.push_back(f.str());
    if (!optimizer.RegisterPassesFromFlags(stdFlags))
      return false;
  }

  return optimizer.Run(mod->data(), mod->size(), mod, options);
}

bool spirvToolsValidate(spv_target_env env, const SpirvCodeGenOptions &opts,
                        bool beforeHlslLegalization, std::vector<uint32_t> *mod,
                        std::string *messages) {
  spvtools::SpirvTools tools(env);

  tools.SetMessageConsumer(
      [messages](spv_message_level_t /*level*/, const char * /*source*/,
                 const spv_position_t & /*position*/,
                 const char *message) { *messages += message; });

  spvtools::ValidatorOptions options;
  options.SetBeforeHlslLegalization(beforeHlslLegalization);
  // GL: strict block layout rules
  // VK: relaxed block layout rules
  // DX: Skip block layout rules
  if (opts.useScalarLayout || opts.useDxLayout) {
    options.SetScalarBlockLayout(true);
  } else if (opts.useGlLayout) {
    // spirv-val by default checks this.
  } else {
    options.SetRelaxBlockLayout(true);
  }

  return tools.Validate(mod->data(), mod->size(), options);
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

} // namespace

SpirvEmitter::SpirvEmitter(CompilerInstance &ci)
    : theCompilerInstance(ci), astContext(ci.getASTContext()),
      diags(ci.getDiagnostics()),
      spirvOptions(ci.getCodeGenOpts().SpirvOptions),
      entryFunctionName(ci.getCodeGenOpts().HLSLEntryFunction), spvContext(),
      featureManager(diags, spirvOptions),
      spvBuilder(astContext, spvContext, spirvOptions),
      declIdMapper(astContext, spvContext, spvBuilder, *this, featureManager,
                   spirvOptions),
      entryFunction(nullptr), curFunction(nullptr), curThis(nullptr),
      seenPushConstantAt(), isSpecConstantMode(false), needsLegalization(false),
      beforeHlslLegalization(false), mainSourceFile(nullptr) {

  // Get ShaderModel from command line hlsl profile option.
  const hlsl::ShaderModel *shaderModel =
      hlsl::ShaderModel::GetByName(ci.getCodeGenOpts().HLSLProfile.c_str());
  if (shaderModel->GetKind() == hlsl::ShaderModel::Kind::Invalid)
    emitError("unknown shader module: %0", {}) << shaderModel->GetName();

  if (spirvOptions.invertY && !shaderModel->IsVS() && !shaderModel->IsDS() &&
      !shaderModel->IsGS())
    emitError("-fvk-invert-y can only be used in VS/DS/GS", {});

  if (spirvOptions.useGlLayout && spirvOptions.useDxLayout)
    emitError("cannot specify both -fvk-use-dx-layout and -fvk-use-gl-layout",
              {});

  // Set shader model kind and hlsl major/minor version.
  spvContext.setCurrentShaderModelKind(shaderModel->GetKind());
  spvContext.setMajorVersion(shaderModel->GetMajor());
  spvContext.setMinorVersion(shaderModel->GetMinor());

  if (spirvOptions.useDxLayout) {
    spirvOptions.cBufferLayoutRule = SpirvLayoutRule::FxcCTBuffer;
    spirvOptions.tBufferLayoutRule = SpirvLayoutRule::FxcCTBuffer;
    spirvOptions.sBufferLayoutRule = SpirvLayoutRule::FxcSBuffer;
    spirvOptions.ampPayloadLayoutRule = SpirvLayoutRule::FxcSBuffer;
  } else if (spirvOptions.useGlLayout) {
    spirvOptions.cBufferLayoutRule = SpirvLayoutRule::GLSLStd140;
    spirvOptions.tBufferLayoutRule = SpirvLayoutRule::GLSLStd430;
    spirvOptions.sBufferLayoutRule = SpirvLayoutRule::GLSLStd430;
    spirvOptions.ampPayloadLayoutRule = SpirvLayoutRule::GLSLStd430;
  } else if (spirvOptions.useScalarLayout) {
    spirvOptions.cBufferLayoutRule = SpirvLayoutRule::Scalar;
    spirvOptions.tBufferLayoutRule = SpirvLayoutRule::Scalar;
    spirvOptions.sBufferLayoutRule = SpirvLayoutRule::Scalar;
    spirvOptions.ampPayloadLayoutRule = SpirvLayoutRule::Scalar;
  } else {
    spirvOptions.cBufferLayoutRule = SpirvLayoutRule::RelaxedGLSLStd140;
    spirvOptions.tBufferLayoutRule = SpirvLayoutRule::RelaxedGLSLStd430;
    spirvOptions.sBufferLayoutRule = SpirvLayoutRule::RelaxedGLSLStd430;
    spirvOptions.ampPayloadLayoutRule = SpirvLayoutRule::RelaxedGLSLStd430;
  }

  // Set shader module version, source file name, and source file content (if
  // needed).
  llvm::StringRef source = "";
  std::vector<llvm::StringRef> fileNames;
  const auto &inputFiles = ci.getFrontendOpts().Inputs;
  // File name
  if (spirvOptions.debugInfoFile && !inputFiles.empty()) {
    for (const auto &inputFile : inputFiles) {
      fileNames.push_back(inputFile.getFile());
    }
  }
  // Source code
  if (spirvOptions.debugInfoSource) {
    const auto &sm = ci.getSourceManager();
    const llvm::MemoryBuffer *mainFile =
        sm.getBuffer(sm.getMainFileID(), SourceLocation());
    source = StringRef(mainFile->getBufferStart(), mainFile->getBufferSize());
  }
  mainSourceFile = spvBuilder.setDebugSource(spvContext.getMajorVersion(),
                                             spvContext.getMinorVersion(),
                                             fileNames, source);

  if (spirvOptions.debugInfoTool &&
      spirvOptions.targetEnv.compare("vulkan1.1") >= 0) {
    // Emit OpModuleProcessed to indicate the commit information.
    std::string commitHash =
        std::string("dxc-commit-hash: ") + clang::getGitCommitHash();
    spvBuilder.addModuleProcessed(commitHash);

    // Emit OpModuleProcessed to indicate the command line options that were
    // used to generate this module.
    if (!spirvOptions.clOptions.empty()) {
      // Using this format: "dxc-cl-option: XXXXXX"
      std::string clOptionStr = "dxc-cl-option:" + spirvOptions.clOptions;
      spvBuilder.addModuleProcessed(clOptionStr);
    }
  }
}

void SpirvEmitter::HandleTranslationUnit(ASTContext &context) {
  // Stop translating if there are errors in previous compilation stages.
  if (context.getDiagnostics().hasErrorOccurred())
    return;

  TranslationUnitDecl *tu = context.getTranslationUnitDecl();
  uint32_t numEntryPoints = 0;

  // The entry function is the seed of the queue.
  for (auto *decl : tu->decls()) {
    if (auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
      if (spvContext.isLib()) {
        if (const auto *shaderAttr = funcDecl->getAttr<HLSLShaderAttr>()) {
          // If we are compiling as a library then add everything that has a
          // ShaderAttr.
          addFunctionToWorkQueue(getShaderModelKind(shaderAttr->getStage()),
                                 funcDecl, /*isEntryFunction*/ true);
          numEntryPoints++;
        }
      } else {
        if (funcDecl->getName() == entryFunctionName) {
          addFunctionToWorkQueue(spvContext.getCurrentShaderModelKind(),
                                 funcDecl, /*isEntryFunction*/ true);
          numEntryPoints++;
        }
      }
    } else {
      doDecl(decl);
    }
  }

  // Translate all functions reachable from the entry function.
  // The queue can grow in the meanwhile; so need to keep evaluating
  // workQueue.size().
  for (uint32_t i = 0; i < workQueue.size(); ++i) {
    const FunctionInfo *curEntryOrCallee = workQueue[i];
    spvContext.setCurrentShaderModelKind(curEntryOrCallee->shaderModelKind);
    doDecl(curEntryOrCallee->funcDecl);
  }

  if (context.getDiagnostics().hasErrorOccurred())
    return;

  const spv_target_env targetEnv = featureManager.getTargetEnv();

  // Addressing and memory model are required in a valid SPIR-V module.
  spvBuilder.setMemoryModel(spv::AddressingModel::Logical,
                            spv::MemoryModel::GLSL450);

  // Even though the 'workQueue' grows due to the above loop, the first
  // 'numEntryPoints' entries in the 'workQueue' are the ones with the HLSL
  // 'shader' attribute, and must therefore be entry functions.
  assert(numEntryPoints <= workQueue.size());

  for (uint32_t i = 0; i < numEntryPoints; ++i) {
    // TODO: assign specific StageVars w.r.t. to entry point
    const FunctionInfo *entryInfo = workQueue[i];
    assert(entryInfo->isEntryFunction);
    spvBuilder.addEntryPoint(
        getSpirvShaderStage(entryInfo->shaderModelKind),
        entryInfo->entryFunction, entryInfo->funcDecl->getName(),
        targetEnv == SPV_ENV_VULKAN_1_2
            ? spvBuilder.getModule()->getVariables()
            : llvm::ArrayRef<SpirvVariable *>(declIdMapper.collectStageVars()));
  }

  // Add Location decorations to stage input/output variables.
  if (!declIdMapper.decorateStageIOLocations())
    return;

  // Add descriptor set and binding decorations to resource variables.
  if (!declIdMapper.decorateResourceBindings())
    return;

  // Output the constructed module.
  std::vector<uint32_t> m = spvBuilder.takeModule();

  if (!spirvOptions.codeGenHighLevel) {
    // In order to flatten resource arrays, we must also unroll loops. Therefore
    // we should run legalization before optimization.
    needsLegalization = needsLegalization || spirvOptions.flattenResourceArrays;

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
      if (!spirvToolsOptimize(targetEnv, &m, spirvOptions, &messages)) {
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
    if (!spirvToolsValidate(targetEnv, spirvOptions,
                            needsLegalization ||
                                declIdMapper.requiresLegalization(),
                            &m, &messages)) {
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

void SpirvEmitter::doDecl(const Decl *decl) {
  if (isa<EmptyDecl>(decl) || isa<TypedefDecl>(decl))
    return;

  if (decl->isImplicit()) {
    doImplicitDecl(decl);
    return;
  }

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
  } else if (const auto *enumDecl = dyn_cast<EnumDecl>(decl)) {
    doEnumDecl(enumDecl);
  } else {
    emitError("decl type %0 unimplemented", decl->getLocation())
        << decl->getDeclKindName();
  }
}

void SpirvEmitter::doStmt(const Stmt *stmt,
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

SpirvInstruction *SpirvEmitter::doExpr(const Expr *expr) {
  SpirvInstruction *result = nullptr;
  expr = expr->IgnoreParens();

  if (const auto *declRefExpr = dyn_cast<DeclRefExpr>(expr)) {
    result = declIdMapper.getDeclEvalInfo(declRefExpr->getDecl(),
                                          expr->getLocStart());
  } else if (const auto *memberExpr = dyn_cast<MemberExpr>(expr)) {
    result = doMemberExpr(memberExpr);
  } else if (const auto *castExpr = dyn_cast<CastExpr>(expr)) {
    result = doCastExpr(castExpr);
  } else if (const auto *initListExpr = dyn_cast<InitListExpr>(expr)) {
    result = doInitListExpr(initListExpr);
  } else if (const auto *boolLiteral = dyn_cast<CXXBoolLiteralExpr>(expr)) {
    result =
        spvBuilder.getConstantBool(boolLiteral->getValue(), isSpecConstantMode);
    result->setRValue();
  } else if (const auto *intLiteral = dyn_cast<IntegerLiteral>(expr)) {
    result = translateAPInt(intLiteral->getValue(), expr->getType());
    result->setRValue();
  } else if (const auto *floatLiteral = dyn_cast<FloatingLiteral>(expr)) {
    result = translateAPFloat(floatLiteral->getValue(), expr->getType());
    result->setRValue();
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

SpirvInstruction *SpirvEmitter::loadIfGLValue(const Expr *expr) {
  // We are trying to load the value here, which is what an LValueToRValue
  // implicit cast is intended to do. We can ignore the cast if exists.
  expr = expr->IgnoreParenLValueCasts();

  return loadIfGLValue(expr, doExpr(expr));
}

SpirvInstruction *SpirvEmitter::loadIfGLValue(const Expr *expr,
                                              SpirvInstruction *info) {
  const auto exprType = expr->getType();

  // Do nothing if this is already rvalue
  if (!info || info->isRValue())
    return info;

  // Check whether we are trying to load an array of opaque objects as a whole.
  // If true, we are likely to copy it as a whole. To assist per-element
  // copying, avoid the load here and return the pointer directly.
  // TODO: consider moving this hack into SPIRV-Tools as a transformation.
  if (isOpaqueArrayType(exprType))
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
    return info;
  }

  if (loadIfAliasVarRef(expr, &info)) {
    // We are loading an alias variable as a whole here. This is likely for
    // wholesale assignments or function returns. Need to load the pointer.
    //
    // Note: legalization specific code
    return info;
  }

  SpirvInstruction *loadedInstr = nullptr;
  // TODO: Ouch. Very hacky. We need special path to get the value type if
  // we are loading a whole ConstantBuffer/TextureBuffer since the normal
  // type translation path won't work.
  if (const auto *declContext = isConstantTextureBufferDeclRef(expr)) {
    loadedInstr = spvBuilder.createLoad(
        declIdMapper.getCTBufferPushConstantType(declContext), info,
        expr->getExprLoc());
  } else {
    loadedInstr = spvBuilder.createLoad(exprType, info, expr->getExprLoc());
  }
  assert(loadedInstr);

  // Special-case: According to the SPIR-V Spec: There is no physical size or
  // bit pattern defined for boolean type. Therefore an unsigned integer is used
  // to represent booleans when layout is required. In such cases, after loading
  // the uint, we should perform a comparison.
  {
    uint32_t vecSize = 1, numRows = 0, numCols = 0;
    if (info->getLayoutRule() != SpirvLayoutRule::Void &&
        isBoolOrVecMatOfBoolType(exprType)) {
      QualType uintType = astContext.UnsignedIntTy;
      if (isScalarType(exprType) || isVectorType(exprType, nullptr, &vecSize)) {
        const auto fromType =
            vecSize == 1 ? uintType
                         : astContext.getExtVectorType(uintType, vecSize);
        loadedInstr =
            castToBool(loadedInstr, fromType, exprType, expr->getLocStart());
      } else {
        const bool isMat = isMxNMatrix(exprType, nullptr, &numRows, &numCols);
        assert(isMat);
        (void)isMat;
        const clang::Type *type = exprType.getCanonicalType().getTypePtr();
        const RecordType *RT = cast<RecordType>(type);
        const ClassTemplateSpecializationDecl *templateSpecDecl =
            cast<ClassTemplateSpecializationDecl>(RT->getDecl());
        ClassTemplateDecl *templateDecl =
            templateSpecDecl->getSpecializedTemplate();
        const auto fromType = getHLSLMatrixType(
            astContext, theCompilerInstance.getSema(), templateDecl,
            astContext.UnsignedIntTy, numRows, numCols);
        loadedInstr =
            castToBool(loadedInstr, fromType, exprType, expr->getLocStart());
      }
      // Now that it is converted to Bool, it has no layout rule.
      // This result-id should be evaluated as bool from here on out.
      loadedInstr->setLayoutRule(SpirvLayoutRule::Void);
    }
  }

  loadedInstr->setRValue();
  return loadedInstr;
}

SpirvInstruction *SpirvEmitter::loadIfAliasVarRef(const Expr *expr) {
  auto *instr = doExpr(expr);
  loadIfAliasVarRef(expr, &instr);
  return instr;
}

bool SpirvEmitter::loadIfAliasVarRef(const Expr *varExpr,
                                     SpirvInstruction **instr) {
  assert(instr);
  if ((*instr) && (*instr)->containsAliasComponent() &&
      isAKindOfStructuredOrByteBuffer(varExpr->getType())) {
    // Load the pointer of the aliased-to-variable if the expression has a
    // pointer to pointer type.
    if (varExpr->isGLValue()) {
      *instr = spvBuilder.createLoad(varExpr->getType(), *instr,
                                     varExpr->getExprLoc());
    }
    return true;
  }
  return false;
}

SpirvInstruction *SpirvEmitter::castToType(SpirvInstruction *value,
                                           QualType fromType, QualType toType,
                                           SourceLocation srcLoc) {
  if (isFloatOrVecMatOfFloatType(toType))
    return castToFloat(value, fromType, toType, srcLoc);

  // Order matters here. Bool (vector) values will also be considered as uint
  // (vector) values. So given a bool (vector) argument, isUintOrVecOfUintType()
  // will also return true. We need to check bool before uint. The opposite is
  // not true.
  if (isBoolOrVecMatOfBoolType(toType))
    return castToBool(value, fromType, toType, srcLoc);

  if (isSintOrVecMatOfSintType(toType) || isUintOrVecMatOfUintType(toType))
    return castToInt(value, fromType, toType, srcLoc);

  emitError("casting to type %0 unimplemented", {}) << toType;
  return nullptr;
}

void SpirvEmitter::doFunctionDecl(const FunctionDecl *decl) {
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
  breakStack = std::stack<SpirvBasicBlock *>();
  continueStack = std::stack<SpirvBasicBlock *>();

  // This will allow the entry-point name to be something like
  // myNamespace::myEntrypointFunc.
  std::string funcName = getFnName(decl);

  SpirvFunction *func = declIdMapper.getOrRegisterFn(decl);

  const auto iter = functionInfoMap.find(decl);
  if (iter != functionInfoMap.end()) {
    const auto &entryInfo = iter->second;
    if (entryInfo->isEntryFunction) {
      funcName = "src." + funcName;
      // Create wrapper for the entry function
      if (!emitEntryFunctionWrapper(decl, func))
        return;
    }
  }

  const QualType retType =
      declIdMapper.getTypeAndCreateCounterForPotentialAliasVar(decl);

  // Construct the function signature.
  llvm::SmallVector<QualType, 4> paramTypes;

  bool isNonStaticMemberFn = false;
  if (const auto *memberFn = dyn_cast<CXXMethodDecl>(decl)) {
    isNonStaticMemberFn = !memberFn->isStatic();

    if (isNonStaticMemberFn) {
      // For non-static member function, the first parameter should be the
      // object on which we are invoking this method.
      const QualType valueType =
          memberFn->getThisType(astContext)->getPointeeType();
      paramTypes.push_back(valueType);
    }
  }

  for (const auto *param : decl->params()) {
    const QualType valueType =
        declIdMapper.getTypeAndCreateCounterForPotentialAliasVar(param);
    paramTypes.push_back(valueType);
  }

  spvBuilder.beginFunction(retType, paramTypes, decl->getLocStart(), funcName,
                           decl->hasAttr<HLSLPreciseAttr>(), func);

  if (isNonStaticMemberFn) {
    // Remember the parameter for the 'this' object so later we can handle
    // CXXThisExpr correctly.
    curThis = spvBuilder.addFnParam(paramTypes[0], /*isPrecise*/ false,
                                    decl->getLocStart(), "param.this");
    if (isOrContainsAKindOfStructuredOrByteBuffer(paramTypes[0])) {
      curThis->setContainsAliasComponent(true);
      needsLegalization = true;
    }
  }

  // Create all parameters.
  for (uint32_t i = 0; i < decl->getNumParams(); ++i) {
    const ParmVarDecl *paramDecl = decl->getParamDecl(i);
    (void)declIdMapper.createFnParam(paramDecl);
  }

  if (decl->hasBody()) {
    // The entry basic block.
    auto *entryLabel = spvBuilder.createBasicBlock("bb.entry");
    spvBuilder.setInsertPoint(entryLabel);

    // Process all statments in the body.
    doStmt(decl->getBody());

    // We have processed all Stmts in this function and now in the last
    // basic block. Make sure we have a termination instruction.
    if (!spvBuilder.isCurrentBasicBlockTerminated()) {
      const auto retType = decl->getReturnType();
      const auto returnLoc = decl->getBody()->getLocEnd();

      if (retType->isVoidType()) {
        spvBuilder.createReturn(returnLoc);
      } else {
        // If the source code does not provide a proper return value for some
        // control flow path, it's undefined behavior. We just return null
        // value here.
        spvBuilder.createReturnValue(spvBuilder.getConstantNull(retType),
                                     returnLoc);
      }
    }
  }

  spvBuilder.endFunction();
}

bool SpirvEmitter::validateVKAttributes(const NamedDecl *decl) {
  bool success = true;

  if (const auto *varDecl = dyn_cast<VarDecl>(decl)) {
    const auto varType = varDecl->getType();
    if ((isSubpassInput(varType) || isSubpassInputMS(varType)) &&
        !varDecl->hasAttr<VKInputAttachmentIndexAttr>()) {
      emitError("missing vk::input_attachment_index attribute",
                varDecl->getLocation());
      success = false;
    }
  }

  if (decl->getAttr<VKInputAttachmentIndexAttr>()) {
    if (!spvContext.isPS()) {
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

    if (!isScalarType(elementType) && !isVectorType(elementType)) {
      emitError(
          "only scalar/vector types allowed as SubpassInput(MS) parameter type",
          decl->getLocation());
      // Return directly to avoid further type processing, which will hit
      // asserts when lowering the type.
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

  // vk::shader_record_nv is supported only on cbuffer/ConstantBuffer
  if (const auto *srbAttr = decl->getAttr<VKShaderRecordNVAttr>()) {
    const auto loc = srbAttr->getLocation();
    const HLSLBufferDecl *bufDecl = nullptr;
    bool isValidType = false;
    if (bufDecl = dyn_cast<HLSLBufferDecl>(decl))
      isValidType = bufDecl->isCBuffer();
    else if (bufDecl = dyn_cast<HLSLBufferDecl>(decl->getDeclContext()))
      isValidType = bufDecl->isCBuffer();

    if (!isValidType) {
      emitError(
          "vk::shader_record_nv can be applied only to cbuffer/ConstantBuffer",
          loc);
      success = false;
    }
    if (decl->hasAttr<VKBindingAttr>()) {
      emitError("vk::shader_record_nv attribute cannot be used together with "
                "vk::binding attribute",
                loc);
      success = false;
    }
  }

  return success;
}

void SpirvEmitter::doHLSLBufferDecl(const HLSLBufferDecl *bufferDecl) {
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
      if (isOrContainsNonFpColMajorMatrix(astContext, spirvOptions,
                                          varMember->getType(), varMember)) {
        emitError("externally initialized non-floating-point column-major "
                  "matrices not supported yet",
                  varMember->getLocation());
      }
    }
  }
  if (!validateVKAttributes(bufferDecl))
    return;
  if (bufferDecl->hasAttr<VKShaderRecordNVAttr>()) {
    (void)declIdMapper.createShaderRecordBufferNV(bufferDecl);
  } else {
    (void)declIdMapper.createCTBuffer(bufferDecl);
  }
}

void SpirvEmitter::doImplicitDecl(const Decl *decl) {
  // We only handle specific implicit declaration for raytracing
  // which are RayFlag/HitKind constant unsigned integers
  // Ignore others
  if (spvContext.isLib() || spvContext.isRay()) {
    const VarDecl *implDecl = dyn_cast<VarDecl>(decl);
    if (implDecl && (implDecl->getName().startswith(StringRef("RAY_FLAG")) ||
                     implDecl->getName().startswith(StringRef("HIT_KIND")))) {
      (void)declIdMapper.createRayTracingNVImplicitVar(implDecl);
    }
  }
}

void SpirvEmitter::doRecordDecl(const RecordDecl *recordDecl) {
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

void SpirvEmitter::doEnumDecl(const EnumDecl *decl) {
  for (auto it = decl->enumerator_begin(); it != decl->enumerator_end(); ++it)
    declIdMapper.createEnumConstant(*it);
}

void SpirvEmitter::doVarDecl(const VarDecl *decl) {
  if (!validateVKAttributes(decl))
    return;

  // We cannot handle external initialization of column-major matrices now.
  if (isExternalVar(decl) &&
      isOrContainsNonFpColMajorMatrix(astContext, spirvOptions, decl->getType(),
                                      decl)) {
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

    if (isRWAppendConsumeSBuffer(type)) {
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

  if (decl->hasAttr<VKShaderRecordNVAttr>()) {
    (void)declIdMapper.createShaderRecordBufferNV(decl);
    return;
  }

  if (isa<HLSLBufferDecl>(decl->getDeclContext())) {
    // This is a VarDecl of a ConstantBuffer/TextureBuffer type.
    (void)declIdMapper.createCTBuffer(decl);
    return;
  }

  SpirvVariable *var = nullptr;

  // The contents in externally visible variables can be updated via the
  // pipeline. They should be handled differently from file and function scope
  // variables.
  // File scope variables (static "global" and "local" variables) belongs to
  // the Private storage class, while function scope variables (normal "local"
  // variables) belongs to the Function storage class.
  if (isExternalVar(decl)) {
    var = declIdMapper.createExternVar(decl);
  } else {
    // We already know the variable is not externally visible here. If it does
    // not have local storage, it should be file scope variable.
    const bool isFileScopeVar = !decl->hasLocalStorage();

    if (isFileScopeVar)
      var = declIdMapper.createFileVar(decl, llvm::None);
    else
      var = declIdMapper.createFnVar(decl, llvm::None);

    // Emit OpStore to initialize the variable
    // TODO: revert back to use OpVariable initializer

    // We should only evaluate the initializer once for a static variable.
    if (isFileScopeVar) {
      if (decl->isStaticLocal()) {
        initOnce(decl->getType(), decl->getName(), var, decl->getInit());
      } else {
        // Defer to initialize these global variables at the beginning of the
        // entry function.
        toInitGloalVars.push_back(decl);
      }

    }
    // Function local variables. Just emit OpStore at the current insert point.
    else if (const Expr *init = decl->getInit()) {
      if (auto *constInit = tryToEvaluateAsConst(init)) {
        spvBuilder.createStore(var, constInit, decl->getLocation());
      } else {
        storeValue(var, loadIfGLValue(init), decl->getType(),
                   decl->getLocation());
      }

      // Update counter variable associated with local variables
      tryToAssignCounterVar(decl, init);
    }

    // Variables that are not externally visible and of opaque types should
    // request legalization.
    if (!needsLegalization && isOpaqueType(decl->getType()))
      needsLegalization = true;
  }

  // All variables that are of opaque struct types should request legalization.
  if (!needsLegalization && isOpaqueStructType(decl->getType()))
    needsLegalization = true;
}

spv::LoopControlMask SpirvEmitter::translateLoopAttribute(const Stmt *stmt,
                                                          const Attr &attr) {
  switch (attr.getKind()) {
  case attr::HLSLLoop:
  case attr::HLSLFastOpt:
    return spv::LoopControlMask::DontUnroll;
  case attr::HLSLUnroll:
    return spv::LoopControlMask::Unroll;
  case attr::HLSLAllowUAVCondition:
    if (!spirvOptions.noWarnIgnoredFeatures) {
      emitWarning("unsupported allow_uav_condition attribute ignored",
                  stmt->getLocStart());
    }
    break;
  default:
    llvm_unreachable("found unknown loop attribute");
  }
  return spv::LoopControlMask::MaskNone;
}

void SpirvEmitter::doDiscardStmt(const DiscardStmt *discardStmt) {
  assert(!spvBuilder.isCurrentBasicBlockTerminated());
  spvBuilder.createKill(discardStmt->getLoc());
  // Some statements that alter the control flow (break, continue, return, and
  // discard), require creation of a new basic block to hold any statement that
  // may follow them.
  auto *newBB = spvBuilder.createBasicBlock();
  spvBuilder.setInsertPoint(newBB);
}

void SpirvEmitter::doDoStmt(const DoStmt *theDoStmt,
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
  auto *headerBB = spvBuilder.createBasicBlock("do_while.header");
  auto *bodyBB = spvBuilder.createBasicBlock("do_while.body");
  auto *continueBB = spvBuilder.createBasicBlock("do_while.continue");
  auto *mergeBB = spvBuilder.createBasicBlock("do_while.merge");

  // Make sure any continue statements branch to the continue block, and any
  // break statements branch to the merge block.
  continueStack.push(continueBB);
  breakStack.push(mergeBB);

  // Branch from the current insert point to the header block.
  spvBuilder.createBranch(headerBB, theDoStmt->getLocStart());
  spvBuilder.addSuccessor(headerBB);

  // Process the <header> block
  // The header block must always branch to the body.
  spvBuilder.setInsertPoint(headerBB);
  const Stmt *body = theDoStmt->getBody();
  spvBuilder.createBranch(bodyBB,
                          body ? body->getLocStart() : theDoStmt->getLocStart(),
                          mergeBB, continueBB, loopControl);
  spvBuilder.addSuccessor(bodyBB);
  // The current basic block has OpLoopMerge instruction. We need to set its
  // continue and merge target.
  spvBuilder.setContinueTarget(continueBB);
  spvBuilder.setMergeTarget(mergeBB);

  // Process the <body> block
  spvBuilder.setInsertPoint(bodyBB);
  if (body) {
    doStmt(body);
  }
  if (!spvBuilder.isCurrentBasicBlockTerminated()) {
    spvBuilder.createBranch(continueBB, body ? body->getLocEnd()
                                             : theDoStmt->getLocStart());
  }
  spvBuilder.addSuccessor(continueBB);

  // Process the <continue> block. The check for whether the loop should
  // continue lies in the continue block.
  // *NOTE*: There's a SPIR-V rule that when a conditional branch is to occur in
  // a continue block of a loop, there should be no OpSelectionMerge. Only an
  // OpBranchConditional must be specified.
  spvBuilder.setInsertPoint(continueBB);
  SpirvInstruction *condition = nullptr;
  if (const Expr *check = theDoStmt->getCond()) {
    condition = doExpr(check);
  } else {
    condition = spvBuilder.getConstantBool(true);
  }
  spvBuilder.createConditionalBranch(condition, headerBB, mergeBB,
                                     theDoStmt->getLocEnd());
  spvBuilder.addSuccessor(headerBB);
  spvBuilder.addSuccessor(mergeBB);

  // Set insertion point to the <merge> block for subsequent statements
  spvBuilder.setInsertPoint(mergeBB);

  // Done with the current scope's continue block and merge block.
  continueStack.pop();
  breakStack.pop();
}

void SpirvEmitter::doContinueStmt(const ContinueStmt *continueStmt) {
  assert(!spvBuilder.isCurrentBasicBlockTerminated());
  auto *continueTargetBB = continueStack.top();
  spvBuilder.createBranch(continueTargetBB, continueStmt->getLocStart());
  spvBuilder.addSuccessor(continueTargetBB);

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
  auto *newBB = spvBuilder.createBasicBlock();
  spvBuilder.setInsertPoint(newBB);
}

void SpirvEmitter::doWhileStmt(const WhileStmt *whileStmt,
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
  auto *checkBB = spvBuilder.createBasicBlock("while.check");
  auto *bodyBB = spvBuilder.createBasicBlock("while.body");
  auto *continueBB = spvBuilder.createBasicBlock("while.continue");
  auto *mergeBB = spvBuilder.createBasicBlock("while.merge");

  // Make sure any continue statements branch to the continue block, and any
  // break statements branch to the merge block.
  continueStack.push(continueBB);
  breakStack.push(mergeBB);

  // Process the <check> block
  spvBuilder.createBranch(checkBB, whileStmt->getLocStart());
  spvBuilder.addSuccessor(checkBB);
  spvBuilder.setInsertPoint(checkBB);

  // If we have:
  // while (int a = foo()) {...}
  // we should evaluate 'a' by calling 'foo()' every single time the check has
  // to occur.
  if (const auto *condVarDecl = whileStmt->getConditionVariableDeclStmt())
    doStmt(condVarDecl);

  SpirvInstruction *condition = nullptr;
  const Expr *check = whileStmt->getCond();
  if (check) {
    condition = doExpr(check);
  } else {
    condition = spvBuilder.getConstantBool(true);
  }
  spvBuilder.createConditionalBranch(
      condition, bodyBB,
      /*false branch*/ mergeBB, whileStmt->getLocStart(),
      /*merge*/ mergeBB, continueBB, spv::SelectionControlMask::MaskNone,
      loopControl);
  spvBuilder.addSuccessor(bodyBB);
  spvBuilder.addSuccessor(mergeBB);
  // The current basic block has OpLoopMerge instruction. We need to set its
  // continue and merge target.
  spvBuilder.setContinueTarget(continueBB);
  spvBuilder.setMergeTarget(mergeBB);

  // Process the <body> block
  spvBuilder.setInsertPoint(bodyBB);
  const Stmt *body = whileStmt->getBody();
  if (body) {
    doStmt(body);
  }
  if (!spvBuilder.isCurrentBasicBlockTerminated())
    spvBuilder.createBranch(continueBB, whileStmt->getLocEnd());
  spvBuilder.addSuccessor(continueBB);

  // Process the <continue> block. While loops do not have an explicit
  // continue block. The continue block just branches to the <check> block.
  spvBuilder.setInsertPoint(continueBB);
  spvBuilder.createBranch(checkBB, whileStmt->getLocEnd());
  spvBuilder.addSuccessor(checkBB);

  // Set insertion point to the <merge> block for subsequent statements
  spvBuilder.setInsertPoint(mergeBB);

  // Done with the current scope's continue and merge blocks.
  continueStack.pop();
  breakStack.pop();
}

void SpirvEmitter::doForStmt(const ForStmt *forStmt,
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
  auto *checkBB = spvBuilder.createBasicBlock("for.check");
  auto *bodyBB = spvBuilder.createBasicBlock("for.body");
  auto *continueBB = spvBuilder.createBasicBlock("for.continue");
  auto *mergeBB = spvBuilder.createBasicBlock("for.merge");

  // Make sure any continue statements branch to the continue block, and any
  // break statements branch to the merge block.
  continueStack.push(continueBB);
  breakStack.push(mergeBB);

  // Process the <init> block
  if (const Stmt *initStmt = forStmt->getInit()) {
    doStmt(initStmt);
  }
  const Expr *check = forStmt->getCond();
  spvBuilder.createBranch(checkBB, check ? check->getLocStart()
                                         : forStmt->getLocStart());
  spvBuilder.addSuccessor(checkBB);

  // Process the <check> block
  spvBuilder.setInsertPoint(checkBB);
  SpirvInstruction *condition = nullptr;
  if (check) {
    condition = doExpr(check);
  } else {
    condition = spvBuilder.getConstantBool(true);
  }
  const Stmt *body = forStmt->getBody();
  spvBuilder.createConditionalBranch(
      condition, bodyBB,
      /*false branch*/ mergeBB,
      check ? check->getLocEnd()
            : (body ? body->getLocStart() : forStmt->getLocStart()),
      /*merge*/ mergeBB, continueBB, spv::SelectionControlMask::MaskNone,
      loopControl);
  spvBuilder.addSuccessor(bodyBB);
  spvBuilder.addSuccessor(mergeBB);
  // The current basic block has OpLoopMerge instruction. We need to set its
  // continue and merge target.
  spvBuilder.setContinueTarget(continueBB);
  spvBuilder.setMergeTarget(mergeBB);

  // Process the <body> block
  spvBuilder.setInsertPoint(bodyBB);
  if (body) {
    doStmt(body);
  }
  if (!spvBuilder.isCurrentBasicBlockTerminated())
    spvBuilder.createBranch(continueBB, forStmt->getLocEnd());
  spvBuilder.addSuccessor(continueBB);

  // Process the <continue> block
  spvBuilder.setInsertPoint(continueBB);
  if (const Expr *cont = forStmt->getInc()) {
    doExpr(cont);
  }
  // <continue> should jump back to header
  spvBuilder.createBranch(checkBB, forStmt->getLocEnd());
  spvBuilder.addSuccessor(checkBB);

  // Set insertion point to the <merge> block for subsequent statements
  spvBuilder.setInsertPoint(mergeBB);

  // Done with the current scope's continue block and merge block.
  continueStack.pop();
  breakStack.pop();
}

void SpirvEmitter::doIfStmt(const IfStmt *ifStmt,
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
      if (!spirvOptions.noWarnIgnoredFeatures) {
        emitWarning("unknown if statement attribute '%0' ignored",
                    attribute->getLocation())
            << attribute->getSpelling();
      }
      break;
    }
  }

  if (const auto *declStmt = ifStmt->getConditionVariableDeclStmt())
    doDeclStmt(declStmt);

  // First emit the instruction for evaluating the condition.
  auto *condition = doExpr(ifStmt->getCond());

  // Then we need to emit the instruction for the conditional branch.
  // We'll need the <label-id> for the then/else/merge block to do so.
  const bool hasElse = ifStmt->getElse() != nullptr;
  auto *thenBB = spvBuilder.createBasicBlock("if.true");
  auto *mergeBB = spvBuilder.createBasicBlock("if.merge");
  auto *elseBB = hasElse ? spvBuilder.createBasicBlock("if.false") : mergeBB;

  // Create the branch instruction. This will end the current basic block.
  const auto *then = ifStmt->getThen();
  spvBuilder.createConditionalBranch(condition, thenBB, elseBB,
                                     then->getLocStart(), mergeBB,
                                     /*continue*/ 0, selectionControl);
  spvBuilder.addSuccessor(thenBB);
  spvBuilder.addSuccessor(elseBB);
  // The current basic block has the OpSelectionMerge instruction. We need
  // to record its merge target.
  spvBuilder.setMergeTarget(mergeBB);

  // Handle the then branch
  spvBuilder.setInsertPoint(thenBB);
  doStmt(then);
  if (!spvBuilder.isCurrentBasicBlockTerminated())
    spvBuilder.createBranch(mergeBB, ifStmt->getLocEnd());
  spvBuilder.addSuccessor(mergeBB);

  // Handle the else branch (if exists)
  if (hasElse) {
    spvBuilder.setInsertPoint(elseBB);
    const auto *elseStmt = ifStmt->getElse();
    doStmt(elseStmt);
    if (!spvBuilder.isCurrentBasicBlockTerminated())
      spvBuilder.createBranch(mergeBB, elseStmt->getLocEnd());
    spvBuilder.addSuccessor(mergeBB);
  }

  // From now on, we'll emit instructions into the merge block.
  spvBuilder.setInsertPoint(mergeBB);
}

void SpirvEmitter::doReturnStmt(const ReturnStmt *stmt) {
  if (const auto *retVal = stmt->getRetValue()) {
    // Update counter variable associated with function returns
    tryToAssignCounterVar(curFunction, retVal);

    auto *retInfo = loadIfGLValue(retVal);
    if (!retInfo)
      return;

    auto retType = retVal->getType();
    if (retInfo->getStorageClass() != spv::StorageClass::Function &&
        retType->isStructureType()) {
      // We are returning some value from a non-Function storage class. Need to
      // create a temporary variable to "convert" the value to Function storage
      // class and then return.
      auto *tempVar =
          spvBuilder.addFnVar(retType, retVal->getLocEnd(), "temp.var.ret");
      storeValue(tempVar, retInfo, retType, retVal->getLocEnd());
      spvBuilder.createReturnValue(
          spvBuilder.createLoad(retType, tempVar, retVal->getLocEnd()),
          stmt->getReturnLoc());
    } else {
      spvBuilder.createReturnValue(retInfo, stmt->getReturnLoc());
    }
  } else {
    spvBuilder.createReturn(stmt->getReturnLoc());
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
  auto *newBB = spvBuilder.createBasicBlock();
  spvBuilder.setInsertPoint(newBB);
}

void SpirvEmitter::doBreakStmt(const BreakStmt *breakStmt) {
  assert(!spvBuilder.isCurrentBasicBlockTerminated());
  auto *breakTargetBB = breakStack.top();
  spvBuilder.addSuccessor(breakTargetBB);
  spvBuilder.createBranch(breakTargetBB, breakStmt->getLocStart());

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
  auto *newBB = spvBuilder.createBasicBlock();
  spvBuilder.setInsertPoint(newBB);
}

void SpirvEmitter::doSwitchStmt(const SwitchStmt *switchStmt,
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

  if (isAttrForceCase && !canUseSpirvOpSwitch &&
      !spirvOptions.noWarnIgnoredFeatures) {
    emitWarning("ignored 'forcecase' attribute for the switch statement "
                "since one or more case values are not integer literals",
                switchStmt->getLocStart());
  }

  if (canUseSpirvOpSwitch)
    processSwitchStmtUsingSpirvOpSwitch(switchStmt);
  else
    processSwitchStmtUsingIfStmts(switchStmt);
}

SpirvInstruction *
SpirvEmitter::doArraySubscriptExpr(const ArraySubscriptExpr *expr) {
  llvm::SmallVector<SpirvInstruction *, 4> indices;
  const auto *base = collectArrayStructIndices(
      expr, /*rawIndex*/ false, /*rawIndices*/ nullptr, &indices);
  auto *info = loadIfAliasVarRef(base);

  if (!indices.empty()) {
    info = turnIntoElementPtr(base->getType(), info, expr->getType(), indices,
                              base->getExprLoc());
  }

  return info;
}

SpirvInstruction *SpirvEmitter::doBinaryOperator(const BinaryOperator *expr) {
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
    if (auto *result = tryToGenFloatMatrixScale(expr))
      return result;
    if (auto *result = tryToGenFloatVectorScale(expr))
      return result;
  }

  return processBinaryOp(expr->getLHS(), expr->getRHS(), opcode,
                         expr->getLHS()->getType(), expr->getType(),
                         expr->getSourceRange(), expr->getOperatorLoc());
}

SpirvInstruction *SpirvEmitter::doCallExpr(const CallExpr *callExpr) {
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

SpirvInstruction *SpirvEmitter::processCall(const CallExpr *callExpr) {
  const FunctionDecl *callee = getCalleeDefinition(callExpr);

  // Note that we always want the defintion because Stmts/Exprs in the
  // function body references the parameters in the definition.
  if (!callee) {
    emitError("found undefined function", callExpr->getExprLoc());
    return nullptr;
  }

  const auto paramTypeMatchesArgType = [](QualType paramType,
                                          QualType argType) {
    if (argType == paramType)
      return true;

    if (const auto *refType = paramType->getAs<ReferenceType>())
      paramType = refType->getPointeeType();
    auto argUnqualifiedType = argType->getUnqualifiedDesugaredType();
    auto paramUnqualifiedType = paramType->getUnqualifiedDesugaredType();
    if (argUnqualifiedType == paramUnqualifiedType)
      return true;

    return false;
  };

  const auto numParams = callee->getNumParams();

  bool isNonStaticMemberCall = false;
  QualType objectType = {};             // Type of the object (if exists)
  SpirvInstruction *objInstr = nullptr; // EvalInfo for the object (if exists)

  llvm::SmallVector<SpirvInstruction *, 4> vars; // Variables for function call
  llvm::SmallVector<bool, 4> isTempVar;          // Temporary variable or not
  llvm::SmallVector<SpirvInstruction *, 4> args; // Evaluated arguments

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
      objInstr = doExpr(object);

      // If not already a variable, we need to create a temporary variable and
      // pass the object pointer to the function. Example:
      // getObject().objectMethod();
      // Also, any parameter passed to the member function must be of Function
      // storage class.
      if (objInstr->isRValue()) {
        args.push_back(createTemporaryVar(
            objectType, getAstTypeName(objectType),
            // May need to load to use as initializer
            loadIfGLValue(object, objInstr), object->getLocStart()));
      } else {
        // Based on SPIR-V spec, function parameter must always be in Function
        // scope. If we pass a non-function scope argument, we need
        // the legalization.
        if (objInstr->getStorageClass() != spv::StorageClass::Function)
          beforeHlslLegalization = true;

        args.push_back(objInstr);
      }

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
    // for it if it can act as out parameter.
    SpirvInstruction *argInfo = nullptr;
    if (const auto *declRefExpr = dyn_cast<DeclRefExpr>(arg)) {
      argInfo = declIdMapper.getDeclEvalInfo(declRefExpr->getDecl(),
                                             arg->getLocStart());
    }

    auto *argInst = doExpr(arg);
    auto argType = arg->getType();

    // If argInfo is nullptr and argInst is a rvalue, we do not have a proper
    // pointer to pass to the function. we need a temporary variable in that
    // case.
    if ((argInfo || (argInst && !argInst->isRValue())) &&
        canActAsOutParmVar(param) &&
        paramTypeMatchesArgType(param->getType(), arg->getType())) {
      // Based on SPIR-V spec, function parameter must be always Function
      // scope. In addition, we must pass memory object declaration argument
      // to function. If we pass an argument that is not function scope
      // or not memory object declaration, we need the legalization.
      if (!argInfo || argInfo->getStorageClass() != spv::StorageClass::Function)
        beforeHlslLegalization = true;

      isTempVar.push_back(false);
      args.push_back(argInst);
      vars.push_back(argInfo ? argInfo : argInst);
    } else {
      // We need to create variables for holding the values to be used as
      // arguments. The variables themselves are of pointer types.
      const QualType varType =
          declIdMapper.getTypeAndCreateCounterForPotentialAliasVar(param);
      const std::string varName = "param.var." + param->getNameAsString();
      // Temporary "param.var.*" variables are used for OpFunctionCall purposes.
      // 'precise' attribute on function parameters only affect computations
      // inside the function, not the variables at the call sites. Therefore, we
      // do not need to mark the "param.var.*" variables as precise.
      const bool isPrecise = false;

      auto *tempVar =
          spvBuilder.addFnVar(varType, arg->getLocStart(), varName, isPrecise);

      vars.push_back(tempVar);
      isTempVar.push_back(true);
      args.push_back(argInst);

      // Update counter variable associated with function parameters
      tryToAssignCounterVar(param, arg);

      // Manually load the argument here
      auto *rhsVal = loadIfGLValue(arg, args.back());

      // The AST does not include cast nodes to and from the function parameter
      // type for 'out' and 'inout' cases. Example:
      //
      // void foo(out half3 param) {...}
      // void main() { float3 arg; foo(arg); }
      //
      // In such cases, we first do a manual cast before passing the argument to
      // the function. And we will cast back the results once the function call
      // has returned.
      if (canActAsOutParmVar(param) &&
          !paramTypeMatchesArgType(param->getType(), arg->getType())) {
        auto paramType = param->getType();
        if (const auto *refType = paramType->getAs<ReferenceType>())
          paramType = refType->getPointeeType();
        rhsVal =
            castToType(rhsVal, arg->getType(), paramType, arg->getLocStart());
      }

      // Initialize the temporary variables using the contents of the arguments
      storeValue(tempVar, rhsVal, param->getType(), arg->getLocStart());
    }
  }

  if (beforeHlslLegalization)
    needsLegalization = true;

  assert(vars.size() == isTempVar.size());
  assert(vars.size() == args.size());

  // Push the callee into the work queue if it is not there.
  addFunctionToWorkQueue(spvContext.getCurrentShaderModelKind(), callee,
                         /*isEntryFunction*/ false);

  const QualType retType =
      declIdMapper.getTypeAndCreateCounterForPotentialAliasVar(callee);
  // Get or forward declare the function <result-id>
  SpirvFunction *func = declIdMapper.getOrRegisterFn(callee);

  auto *retVal = spvBuilder.createFunctionCall(
      retType, func, vars, callExpr->getCallee()->getExprLoc());

  // Go through all parameters and write those marked as out/inout
  for (uint32_t i = 0; i < numParams; ++i) {
    const auto *param = callee->getParamDecl(i);
    // If it calls a non-static member function, the object itself is argument
    // 0, and therefore all other argument positions are shifted by 1.
    const uint32_t index = i + isNonStaticMemberCall;
    if (isTempVar[index] && canActAsOutParmVar(param)) {
      const auto *arg = callExpr->getArg(i);
      SpirvInstruction *value = spvBuilder.createLoad(
          param->getType(), vars[index], arg->getLocStart());

      // Now we want to assign 'value' to arg. But first, in rare cases when
      // using 'out' or 'inout' where the parameter and argument have a type
      // mismatch, we need to first cast 'value' to the type of 'arg' because
      // the AST will not include a cast node.
      if (!paramTypeMatchesArgType(param->getType(), arg->getType())) {
        auto paramType = param->getType();
        if (const auto *refType = paramType->getAs<ReferenceType>())
          paramType = refType->getPointeeType();
        value =
            castToType(value, paramType, arg->getType(), arg->getLocStart());
      }

      processAssignment(arg, value, false, args[index]);
    }
  }

  return retVal;
}

SpirvInstruction *SpirvEmitter::doCastExpr(const CastExpr *expr) {
  const Expr *subExpr = expr->getSubExpr();
  const QualType subExprType = subExpr->getType();
  const QualType toType = expr->getType();
  const auto srcLoc = expr->getExprLoc();

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
    if (auto *value = tryToEvaluateAsConst(expr)) {
      value->setRValue();
      return value;
    }

    auto *value = castToInt(loadIfGLValue(subExpr), subExprType, toType,
                            subExpr->getLocStart());
    value->setRValue();
    return value;
  }
  case CastKind::CK_FloatingCast:
  case CastKind::CK_IntegralToFloating:
  case CastKind::CK_HLSLCC_FloatingCast:
  case CastKind::CK_HLSLCC_IntegralToFloating: {
    // First try to see if we can do constant folding for floating point
    // numbers like what we are doing for integers in the above.
    if (auto *value = tryToEvaluateAsConst(expr)) {
      value->setRValue();
      return value;
    }

    auto *value = castToFloat(loadIfGLValue(subExpr), subExprType, toType,
                              subExpr->getLocStart());
    value->setRValue();
    return value;
  }
  case CastKind::CK_IntegralToBoolean:
  case CastKind::CK_FloatingToBoolean:
  case CastKind::CK_HLSLCC_IntegralToBoolean:
  case CastKind::CK_HLSLCC_FloatingToBoolean: {
    // First try to see if we can do constant folding.
    if (auto *value = tryToEvaluateAsConst(expr)) {
      value->setRValue();
      return value;
    }

    auto *value = castToBool(loadIfGLValue(subExpr), subExprType, toType,
                             subExpr->getLocStart());
    value->setRValue();
    return value;
  }
  case CastKind::CK_HLSLVectorSplat: {
    const size_t size = hlsl::GetHLSLVecSize(expr->getType());
    return createVectorSplat(subExpr, size);
  }
  case CastKind::CK_HLSLVectorTruncationCast: {
    const QualType toVecType = toType;
    const QualType elemType = hlsl::GetHLSLVecElementType(toType);
    const auto toSize = hlsl::GetHLSLVecSize(toType);
    auto *composite = doExpr(subExpr);
    llvm::SmallVector<SpirvInstruction *, 4> elements;

    for (uint32_t i = 0; i < toSize; ++i) {
      elements.push_back(spvBuilder.createCompositeExtract(
          elemType, composite, {i}, expr->getExprLoc()));
    }

    auto *value = elements.front();
    if (toSize > 1) {
      value = spvBuilder.createCompositeConstruct(toVecType, elements,
                                                  expr->getExprLoc());
    }

    value->setRValue();
    return value;
  }
  case CastKind::CK_HLSLVectorToScalarCast: {
    // The underlying should already be a vector of size 1.
    assert(hlsl::GetHLSLVecSize(subExprType) == 1);
    return doExpr(subExpr);
  }
  case CastKind::CK_HLSLVectorToMatrixCast: {
    // If target type is already an 1xN matrix type, we just return the
    // underlying vector.
    if (is1xNMatrix(toType))
      return doExpr(subExpr);

    // A vector can have no more than 4 elements. The only remaining case
    // is casting from size-4 vector to size-2-by-2 matrix.

    auto *vec = loadIfGLValue(subExpr);
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    const bool isMat = isMxNMatrix(toType, &elemType, &rowCount, &colCount);
    assert(isMat && rowCount == 2 && colCount == 2);
    (void)isMat;
    QualType vec2Type = astContext.getExtVectorType(elemType, 2);
    auto *subVec1 = spvBuilder.createVectorShuffle(vec2Type, vec, vec, {0, 1},
                                                   expr->getLocStart());
    auto *subVec2 = spvBuilder.createVectorShuffle(vec2Type, vec, vec, {2, 3},
                                                   expr->getLocStart());
    auto *mat = spvBuilder.createCompositeConstruct(toType, {subVec1, subVec2},
                                                    expr->getLocStart());
    mat->setRValue();
    return mat;
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

    if (isa<SpirvConstant>(vecSplat)) {
      llvm::SmallVector<SpirvConstant *, 4> vectors(
          size_t(rowCount), cast<SpirvConstant>(vecSplat));
      auto *value = spvBuilder.getConstantComposite(toType, vectors);
      value->setRValue();
      return value;
    } else {
      llvm::SmallVector<SpirvInstruction *, 4> vectors(size_t(rowCount),
                                                       vecSplat);
      auto *value = spvBuilder.createCompositeConstruct(toType, vectors,
                                                        expr->getLocEnd());
      value->setRValue();
      return value;
    }
  }
  case CastKind::CK_HLSLMatrixTruncationCast: {
    const QualType srcType = subExprType;
    auto *src = doExpr(subExpr);
    const QualType elemType = hlsl::GetHLSLMatElementType(srcType);
    llvm::SmallVector<uint32_t, 4> indexes;

    // It is possible that the source matrix is in fact a vector.
    // For example: Truncate float1x3 --> float1x2.
    // The front-end disallows float1x3 --> float2x1.
    {
      uint32_t srcVecSize = 0, dstVecSize = 0;
      if (isVectorType(srcType, nullptr, &srcVecSize) &&
          isVectorType(toType, nullptr, &dstVecSize)) {
        for (uint32_t i = 0; i < dstVecSize; ++i)
          indexes.push_back(i);
        auto *val = spvBuilder.createVectorShuffle(toType, src, src, indexes,
                                                   expr->getLocStart());
        val->setRValue();
        return val;
      }
    }

    uint32_t srcRows = 0, srcCols = 0, dstRows = 0, dstCols = 0;
    hlsl::GetHLSLMatRowColCount(srcType, srcRows, srcCols);
    hlsl::GetHLSLMatRowColCount(toType, dstRows, dstCols);
    const QualType srcRowType = astContext.getExtVectorType(elemType, srcCols);
    const QualType dstRowType = astContext.getExtVectorType(elemType, dstCols);

    // Indexes to pass to OpVectorShuffle
    for (uint32_t i = 0; i < dstCols; ++i)
      indexes.push_back(i);

    llvm::SmallVector<SpirvInstruction *, 4> extractedVecs;
    for (uint32_t row = 0; row < dstRows; ++row) {
      // Extract a row
      SpirvInstruction *rowInstr = spvBuilder.createCompositeExtract(
          srcRowType, src, {row}, expr->getExprLoc());
      // Extract the necessary columns from that row.
      // The front-end ensures dstCols <= srcCols.
      // If dstCols equals srcCols, we can use the whole row directly.
      if (dstCols == 1) {
        rowInstr = spvBuilder.createCompositeExtract(elemType, rowInstr, {0},
                                                     expr->getLocStart());
      } else if (dstCols < srcCols) {
        rowInstr = spvBuilder.createVectorShuffle(
            dstRowType, rowInstr, rowInstr, indexes, expr->getLocStart());
      }
      extractedVecs.push_back(rowInstr);
    }

    auto *val = extractedVecs.front();
    if (extractedVecs.size() > 1) {
      val = spvBuilder.createCompositeConstruct(toType, extractedVecs,
                                                expr->getExprLoc());
    }
    val->setRValue();
    return val;
  }
  case CastKind::CK_HLSLMatrixToScalarCast: {
    // The underlying should already be a matrix of 1x1.
    assert(is1x1Matrix(subExprType));
    return doExpr(subExpr);
  }
  case CastKind::CK_HLSLMatrixToVectorCast: {
    // If the underlying matrix is Mx1 or 1xM for M in {1, 2,3,4}, we can return
    // the underlying matrix because it'll be evaluated as a vector by default.
    if (is1x1Matrix(subExprType) || is1xNMatrix(subExprType) ||
        isMx1Matrix(subExprType))
      return doExpr(subExpr);

    // A vector can have no more than 4 elements. The only remaining case
    // is casting from a 2x2 matrix to a vector of size 4.

    auto *mat = loadIfGLValue(subExpr);
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0, elemCount = 0;
    const bool isMat =
        isMxNMatrix(subExprType, &elemType, &rowCount, &colCount);
    const bool isVec = isVectorType(toType, nullptr, &elemCount);
    assert(isMat && rowCount == 2 && colCount == 2);
    assert(isVec && elemCount == 4);
    (void)isMat;
    (void)isVec;
    QualType vec2Type = astContext.getExtVectorType(elemType, 2);
    auto *row0 = spvBuilder.createCompositeExtract(vec2Type, mat, {0}, srcLoc);
    auto *row1 = spvBuilder.createCompositeExtract(vec2Type, mat, {1}, srcLoc);
    auto *vec = spvBuilder.createVectorShuffle(toType, row0, row1, {0, 1, 2, 3},
                                               srcLoc);
    vec->setRValue();
    return vec;
  }
  case CastKind::CK_FunctionToPointerDecay:
    // Just need to return the function id
    return doExpr(subExpr);
  case CastKind::CK_FlatConversion: {
    SpirvInstruction *subExprInstr = nullptr;
    QualType evalType = subExprType;

    // Optimization: we can use OpConstantNull for cases where we want to
    // initialize an entire data structure to zeros.
    if (evaluatesToConstZero(subExpr, astContext)) {
      subExprInstr = spvBuilder.getConstantNull(toType);
      subExprInstr->setRValue();
      return subExprInstr;
    }

    // Try to evaluate float literals as float rather than double.
    if (const auto *floatLiteral = dyn_cast<FloatingLiteral>(subExpr)) {
      subExprInstr = tryToEvaluateAsFloat32(floatLiteral->getValue());
      if (subExprInstr)
        evalType = astContext.FloatTy;
    }
    // Evaluate 'literal float' initializer type as float rather than double.
    // TODO: This could result in rounding error if the initializer is a
    // non-literal expression that requires larger than 32 bits and has the
    // 'literal float' type.
    else if (subExprType->isSpecificBuiltinType(BuiltinType::LitFloat)) {
      evalType = astContext.FloatTy;
    }
    // Try to evaluate integer literals as 32-bit int rather than 64-bit int.
    else if (const auto *intLiteral = dyn_cast<IntegerLiteral>(subExpr)) {
      const bool isSigned = subExprType->isSignedIntegerType();
      subExprInstr = tryToEvaluateAsInt32(intLiteral->getValue(), isSigned);
      if (subExprInstr)
        evalType = isSigned ? astContext.IntTy : astContext.UnsignedIntTy;
    }
    // For assigning one array instance to another one with the same array type
    // (regardless of constness and literalness), the rhs will be wrapped in a
    // FlatConversion. Similarly for assigning a struct to another struct with
    // identical members.
    //  |- <lhs>
    //  `- ImplicitCastExpr <FlatConversion>
    //     `- ImplicitCastExpr <LValueToRValue>
    //        `- <rhs>
    else if (isSameType(astContext, toType, evalType) ||
             // We can have casts changing the shape but without affecting
             // memory order, e.g., `float4 a[2]; float b[8] = (float[8])a;`.
             // This is also represented as FlatConversion. For such cases, we
             // can rely on the InitListHandler, which can decompse
             // vectors/matrices.
             subExprType->isArrayType()) {
      auto *valInstr =
          InitListHandler(astContext, *this).processCast(toType, subExpr);
      if (valInstr)
        valInstr->setRValue();
      return valInstr;
    }
    // We can have casts changing the shape but without affecting memory order,
    // e.g., `float4 a[2]; float b[8] = (float[8])a;`. This is also represented
    // as FlatConversion. For such cases, we can rely on the InitListHandler,
    // which can decompse vectors/matrices.
    else if (subExprType->isArrayType()) {
      auto *valInstr = InitListHandler(astContext, *this)
                           .processCast(expr->getType(), subExpr);
      if (valInstr)
        valInstr->setRValue();
      return valInstr;
    }

    if (!subExprInstr)
      subExprInstr = doExpr(subExpr);

    auto *val = processFlatConversion(toType, evalType, subExprInstr,
                                      expr->getExprLoc());
    val->setRValue();
    return val;
  }
  case CastKind::CK_UncheckedDerivedToBase:
  case CastKind::CK_HLSLDerivedToBase: {
    // Find the index sequence of the base to which we are casting
    llvm::SmallVector<uint32_t, 4> baseIndices;
    getBaseClassIndices(expr, &baseIndices);

    // Turn them in to SPIR-V constants
    llvm::SmallVector<SpirvInstruction *, 4> baseIndexInstructions(
        baseIndices.size(), nullptr);
    for (uint32_t i = 0; i < baseIndices.size(); ++i)
      baseIndexInstructions[i] = spvBuilder.getConstantInt(
          astContext.UnsignedIntTy, llvm::APInt(32, baseIndices[i]));

    auto *derivedInfo = doExpr(subExpr);
    return turnIntoElementPtr(subExpr->getType(), derivedInfo, expr->getType(),
                              baseIndexInstructions, subExpr->getExprLoc());
  }
  default:
    emitError("implicit cast kind '%0' unimplemented", expr->getExprLoc())
        << expr->getCastKindName() << expr->getSourceRange();
    expr->dump();
    return 0;
  }
}

SpirvInstruction *SpirvEmitter::processFlatConversion(
    const QualType type, const QualType initType, SpirvInstruction *initInstr,
    SourceLocation srcLoc) {
  // Try to translate the canonical type first
  const auto canonicalType = type.getCanonicalType();
  if (canonicalType != type)
    return processFlatConversion(canonicalType, initType, initInstr, srcLoc);

  // Primitive types
  {
    QualType ty = {};
    if (isScalarType(type, &ty)) {
      if (const auto *builtinType = ty->getAs<BuiltinType>()) {
        switch (builtinType->getKind()) {
        case BuiltinType::Void: {
          emitError("cannot create a constant of void type", srcLoc);
          return 0;
        }
        case BuiltinType::Bool:
          return castToBool(initInstr, initType, ty, srcLoc);
        // Target type is an integer variant.
        case BuiltinType::Int:
        case BuiltinType::Short:
        case BuiltinType::Min12Int:
        case BuiltinType::Min16Int:
        case BuiltinType::Min16UInt:
        case BuiltinType::UShort:
        case BuiltinType::UInt:
        case BuiltinType::Long:
        case BuiltinType::LongLong:
        case BuiltinType::ULong:
        case BuiltinType::ULongLong:
          return castToInt(initInstr, initType, ty, srcLoc);
        // Target type is a float variant.
        case BuiltinType::Double:
        case BuiltinType::Float:
        case BuiltinType::Half:
        case BuiltinType::HalfFloat:
        case BuiltinType::Min10Float:
        case BuiltinType::Min16Float:
          return castToFloat(initInstr, initType, ty, srcLoc);
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
    if (isVectorType(type, &elemType, &elemCount)) {
      auto *elem = processFlatConversion(elemType, initType, initInstr, srcLoc);
      llvm::SmallVector<SpirvInstruction *, 4> constituents(size_t(elemCount),
                                                            elem);
      return spvBuilder.createCompositeConstruct(type, constituents, srcLoc);
    }
  }

  // Matrix types
  {
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount)) {
      // By default HLSL matrices are row major, while SPIR-V matrices are
      // column major. We are mapping what HLSL semantically mean a row into a
      // column here.
      const QualType vecType = astContext.getExtVectorType(elemType, colCount);
      auto *elem = processFlatConversion(elemType, initType, initInstr, srcLoc);
      const llvm::SmallVector<SpirvInstruction *, 4> constituents(
          size_t(colCount), elem);
      auto *col =
          spvBuilder.createCompositeConstruct(vecType, constituents, srcLoc);
      const llvm::SmallVector<SpirvInstruction *, 4> rows(size_t(rowCount),
                                                          col);
      return spvBuilder.createCompositeConstruct(type, rows, srcLoc);
    }
  }

  // Struct type
  if (const auto *structType = type->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();
    llvm::SmallVector<SpirvInstruction *, 4> fields;

    for (const auto *field : decl->fields()) {
      // There is a special case for FlatConversion. If T is a struct with only
      // one member, S, then (T)<an-instance-of-S> is allowed, which essentially
      // constructs a new T instance using the instance of S as its only member.
      // Check whether we are handling that case here first.
      if (field->getType().getCanonicalType() == initType.getCanonicalType()) {
        fields.push_back(initInstr);
      } else {
        fields.push_back(processFlatConversion(field->getType(), initType,
                                               initInstr, srcLoc));
      }
    }

    return spvBuilder.createCompositeConstruct(type, fields, srcLoc);
  }

  // Array type
  if (const auto *arrayType = astContext.getAsConstantArrayType(type)) {
    const auto size =
        static_cast<uint32_t>(arrayType->getSize().getZExtValue());
    auto *elem = processFlatConversion(arrayType->getElementType(), initType,
                                       initInstr, srcLoc);
    llvm::SmallVector<SpirvInstruction *, 4> constituents(size_t(size), elem);
    return spvBuilder.createCompositeConstruct(type, constituents, srcLoc);
  }

  emitError("flat conversion of type %0 unimplemented", {})
      << type->getTypeClassName();
  type->dump();
  return 0;
}

SpirvInstruction *
SpirvEmitter::doCompoundAssignOperator(const CompoundAssignOperator *expr) {
  const auto opcode = expr->getOpcode();

  // Try to optimize floatMxN *= float and floatN *= float case
  if (opcode == BO_MulAssign) {
    if (auto *result = tryToGenFloatMatrixScale(expr))
      return result;
    if (auto *result = tryToGenFloatVectorScale(expr))
      return result;
  }

  const auto *rhs = expr->getRHS();
  const auto *lhs = expr->getLHS();

  SpirvInstruction *lhsPtr = nullptr;
  auto *result = processBinaryOp(
      lhs, rhs, opcode, expr->getComputationLHSType(), expr->getType(),
      expr->getSourceRange(), expr->getOperatorLoc(), &lhsPtr);
  return processAssignment(lhs, result, true, lhsPtr);
}

SpirvInstruction *
SpirvEmitter::doConditionalOperator(const ConditionalOperator *expr) {
  const auto type = expr->getType();
  const SourceLocation loc = expr->getExprLoc();

  // According to HLSL doc, all sides of the ?: expression are always evaluated.

  // If we are selecting between two SampleState objects, none of the three
  // operands has a LValueToRValue implicit cast.
  auto *condition = loadIfGLValue(expr->getCond());
  auto *trueBranch = loadIfGLValue(expr->getTrueExpr());
  auto *falseBranch = loadIfGLValue(expr->getFalseExpr());

  // For cases where the return type is a scalar or a vector, we can use
  // OpSelect to choose between the two. OpSelect's return type must be either
  // scalar or vector.
  if (isScalarType(type) || isVectorType(type)) {
    // The SPIR-V OpSelect instruction must have a selection argument that is
    // the same size as the return type. If the return type is a vector, the
    // selection must be a vector of booleans (one per output component).
    uint32_t count = 0;
    if (isVectorType(expr->getType(), nullptr, &count) &&
        !isVectorType(expr->getCond()->getType())) {
      const llvm::SmallVector<SpirvInstruction *, 4> components(size_t(count),
                                                                condition);
      condition = spvBuilder.createCompositeConstruct(
          astContext.getExtVectorType(astContext.BoolTy, count), components,
          expr->getCond()->getLocEnd());
    }

    auto *value =
        spvBuilder.createSelect(type, condition, trueBranch, falseBranch, loc);
    value->setRValue();
    return value;
  }

  // If we can't use OpSelect, we need to create if-else control flow.
  auto *tempVar = spvBuilder.addFnVar(type, loc, "temp.var.ternary");
  auto *thenBB = spvBuilder.createBasicBlock("if.true");
  auto *mergeBB = spvBuilder.createBasicBlock("if.merge");
  auto *elseBB = spvBuilder.createBasicBlock("if.false");

  // Create the branch instruction. This will end the current basic block.
  spvBuilder.createConditionalBranch(condition, thenBB, elseBB,
                                     expr->getCond()->getLocEnd(), mergeBB);
  spvBuilder.addSuccessor(thenBB);
  spvBuilder.addSuccessor(elseBB);
  spvBuilder.setMergeTarget(mergeBB);
  // Handle the then branch
  spvBuilder.setInsertPoint(thenBB);
  spvBuilder.createStore(tempVar, trueBranch,
                         expr->getTrueExpr()->getLocStart());
  spvBuilder.createBranch(mergeBB, expr->getTrueExpr()->getLocEnd());
  spvBuilder.addSuccessor(mergeBB);
  // Handle the else branch
  spvBuilder.setInsertPoint(elseBB);
  spvBuilder.createStore(tempVar, falseBranch,
                         expr->getFalseExpr()->getLocStart());
  spvBuilder.createBranch(mergeBB, expr->getFalseExpr()->getLocEnd());
  spvBuilder.addSuccessor(mergeBB);
  // From now on, emit instructions into the merge block.
  spvBuilder.setInsertPoint(mergeBB);
  auto *result = spvBuilder.createLoad(type, tempVar, expr->getLocEnd());
  result->setRValue();
  return result;
}

SpirvInstruction *
SpirvEmitter::processByteAddressBufferStructuredBufferGetDimensions(
    const CXXMemberCallExpr *expr) {
  const auto *object = expr->getImplicitObjectArgument();
  auto *objectInstr = loadIfAliasVarRef(object);
  const auto type = object->getType();
  const bool isBABuf = isByteAddressBuffer(type) || isRWByteAddressBuffer(type);
  const bool isStructuredBuf = isStructuredBuffer(type) ||
                               isAppendStructuredBuffer(type) ||
                               isConsumeStructuredBuffer(type);
  assert(isBABuf || isStructuredBuf);

  // (RW)ByteAddressBuffers/(RW)StructuredBuffers are represented as a structure
  // with only one member that is a runtime array. We need to perform
  // OpArrayLength on member 0.
  SpirvInstruction *length = spvBuilder.createArrayLength(
      astContext.UnsignedIntTy, expr->getExprLoc(), objectInstr, 0);
  // For (RW)ByteAddressBuffers, GetDimensions() must return the array length
  // in bytes, but OpArrayLength returns the number of uints in the runtime
  // array. Therefore we must multiply the results by 4.
  if (isBABuf) {
    length = spvBuilder.createBinaryOp(
        spv::Op::OpIMul, astContext.UnsignedIntTy, length,
        // TODO(jaebaek): What line info we should emit for constants?
        spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                  llvm::APInt(32, 4u)),
        expr->getExprLoc());
  }
  spvBuilder.createStore(doExpr(expr->getArg(0)), length,
                         expr->getArg(0)->getLocStart());

  if (isStructuredBuf) {
    // For (RW)StructuredBuffer, the stride of the runtime array (which is the
    // size of the struct) must also be written to the second argument.
    AlignmentSizeCalculator alignmentCalc(astContext, spirvOptions);
    uint32_t size = 0, stride = 0;
    std::tie(std::ignore, size) =
        alignmentCalc.getAlignmentAndSize(type, spirvOptions.sBufferLayoutRule,
                                          /*isRowMajor*/ llvm::None, &stride);
    auto *sizeInstr = spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                                llvm::APInt(32, size));
    spvBuilder.createStore(doExpr(expr->getArg(1)), sizeInstr,
                           expr->getArg(1)->getLocStart());
  }

  return nullptr;
}

SpirvInstruction *SpirvEmitter::processRWByteAddressBufferAtomicMethods(
    hlsl::IntrinsicOp opcode, const CXXMemberCallExpr *expr) {
  // The signature of RWByteAddressBuffer atomic methods are largely:
  // void Interlocked*(in UINT dest, in UINT value);
  // void Interlocked*(in UINT dest, in UINT value, out UINT original_value);

  const auto *object = expr->getImplicitObjectArgument();
  auto *objectInfo = loadIfAliasVarRef(object);

  auto *zero =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *offset = doExpr(expr->getArg(0));

  // Right shift by 2 to convert the byte offset to uint32_t offset
  auto *address = spvBuilder.createBinaryOp(
      spv::Op::OpShiftRightLogical, astContext.UnsignedIntTy, offset,
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 2)),
      expr->getExprLoc());
  auto *ptr =
      spvBuilder.createAccessChain(astContext.UnsignedIntTy, objectInfo,
                                   {zero, address}, object->getLocStart());

  const bool isCompareExchange =
      opcode == hlsl::IntrinsicOp::MOP_InterlockedCompareExchange;
  const bool isCompareStore =
      opcode == hlsl::IntrinsicOp::MOP_InterlockedCompareStore;

  if (isCompareExchange || isCompareStore) {
    auto *comparator = doExpr(expr->getArg(1));
    auto *originalVal = spvBuilder.createAtomicCompareExchange(
        astContext.UnsignedIntTy, ptr, spv::Scope::Device,
        spv::MemorySemanticsMask::MaskNone, spv::MemorySemanticsMask::MaskNone,
        doExpr(expr->getArg(2)), comparator, expr->getCallee()->getExprLoc());
    if (isCompareExchange)
      spvBuilder.createStore(doExpr(expr->getArg(3)), originalVal,
                             expr->getArg(3)->getLocStart());
  } else {
    auto *value = doExpr(expr->getArg(1));
    SpirvInstruction *originalVal = spvBuilder.createAtomicOp(
        translateAtomicHlslOpcodeToSpirvOpcode(opcode),
        astContext.UnsignedIntTy, ptr, spv::Scope::Device,
        spv::MemorySemanticsMask::MaskNone, value,
        expr->getCallee()->getExprLoc());
    if (expr->getNumArgs() > 2) {
      originalVal = castToType(originalVal, astContext.UnsignedIntTy,
                               expr->getArg(2)->getType(),
                               expr->getArg(2)->getLocStart());
      spvBuilder.createStore(doExpr(expr->getArg(2)), originalVal,
                             expr->getArg(2)->getLocStart());
    }
  }

  return nullptr;
}

SpirvInstruction *
SpirvEmitter::processGetSamplePosition(const CXXMemberCallExpr *expr) {
  const auto *object = expr->getImplicitObjectArgument()->IgnoreParens();
  auto *sampleCount = spvBuilder.createImageQuery(
      spv::Op::OpImageQuerySamples, astContext.UnsignedIntTy,
      expr->getExprLoc(), loadIfGLValue(object));
  if (!spirvOptions.noWarnEmulatedFeatures)
    emitWarning("GetSamplePosition is emulated using many SPIR-V instructions "
                "due to lack of direct SPIR-V equivalent, so it only supports "
                "standard sample settings with 1, 2, 4, 8, or 16 samples and "
                "will return float2(0, 0) for other cases",
                expr->getCallee()->getExprLoc());
  return emitGetSamplePosition(sampleCount, doExpr(expr->getArg(0)),
                               expr->getCallee()->getExprLoc());
}

SpirvInstruction *
SpirvEmitter::processSubpassLoad(const CXXMemberCallExpr *expr) {
  const auto *object = expr->getImplicitObjectArgument()->IgnoreParens();
  SpirvInstruction *sample =
      expr->getNumArgs() == 1 ? doExpr(expr->getArg(0)) : nullptr;
  auto *zero = spvBuilder.getConstantInt(astContext.IntTy, llvm::APInt(32, 0));
  auto *location = spvBuilder.getConstantComposite(
      astContext.getExtVectorType(astContext.IntTy, 2), {zero, zero});

  return processBufferTextureLoad(object, location, /*constOffset*/ 0,
                                  /*varOffset*/ 0, /*lod*/ sample,
                                  /*residencyCode*/ 0, expr->getExprLoc());
}

SpirvInstruction *
SpirvEmitter::processBufferTextureGetDimensions(const CXXMemberCallExpr *expr) {
  const auto *object = expr->getImplicitObjectArgument();
  auto *objectInstr = loadIfGLValue(object);
  const auto type = object->getType();
  const auto *recType = type->getAs<RecordType>();
  assert(recType);
  const auto typeName = recType->getDecl()->getName();
  const auto numArgs = expr->getNumArgs();
  const Expr *mipLevel = nullptr, *numLevels = nullptr, *numSamples = nullptr;

  assert(isTexture(type) || isRWTexture(type) || isBuffer(type) ||
         isRWBuffer(type));

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
  const auto storeToOutputArg = [this](const Expr *outputArg,
                                       SpirvInstruction *id, QualType type) {
    id = castToType(id, type, outputArg->getType(), outputArg->getExprLoc());
    spvBuilder.createStore(doExpr(outputArg), id, outputArg->getLocStart());
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
  if (isTextureMS(type)) {
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

  const QualType resultQualType =
      querySize == 1
          ? astContext.UnsignedIntTy
          : astContext.getExtVectorType(astContext.UnsignedIntTy, querySize);

  // Only Texture types use ImageQuerySizeLod.
  // TextureMS, RWTexture, Buffers, RWBuffers use ImageQuerySize.
  SpirvInstruction *lod = nullptr;
  if (isTexture(type) && !numSamples) {
    if (mipLevel) {
      // For Texture types when mipLevel argument is present.
      lod = doExpr(mipLevel);
    } else {
      // For Texture types when mipLevel argument is omitted.
      lod = spvBuilder.getConstantInt(astContext.IntTy, llvm::APInt(32, 0));
    }
  }

  SpirvInstruction *query =
      lod ? cast<SpirvInstruction>(spvBuilder.createImageQuery(
                spv::Op::OpImageQuerySizeLod, resultQualType,
                expr->getCallee()->getExprLoc(), objectInstr, lod))
          : cast<SpirvInstruction>(spvBuilder.createImageQuery(
                spv::Op::OpImageQuerySize, resultQualType,
                expr->getCallee()->getExprLoc(), objectInstr));

  if (querySize == 1) {
    const uint32_t argIndex = mipLevel ? 1 : 0;
    storeToOutputArg(expr->getArg(argIndex), query, resultQualType);
  } else {
    for (uint32_t i = 0; i < querySize; ++i) {
      const uint32_t argIndex = mipLevel ? i + 1 : i;
      auto *component = spvBuilder.createCompositeExtract(
          astContext.UnsignedIntTy, query, {i},
          expr->getCallee()->getExprLoc());
      // If the first arg is the mipmap level, we must write the results
      // starting from Arg(i+1), not Arg(i).
      storeToOutputArg(expr->getArg(argIndex), component,
                       astContext.UnsignedIntTy);
    }
  }

  if (numLevels || numSamples) {
    const Expr *numLevelsSamplesArg = numLevels ? numLevels : numSamples;
    const spv::Op opcode =
        numLevels ? spv::Op::OpImageQueryLevels : spv::Op::OpImageQuerySamples;
    auto *numLevelsSamplesQuery = spvBuilder.createImageQuery(
        opcode, astContext.UnsignedIntTy, expr->getCallee()->getExprLoc(),
        objectInstr);
    storeToOutputArg(numLevelsSamplesArg, numLevelsSamplesQuery,
                     astContext.UnsignedIntTy);
  }

  return nullptr;
}

SpirvInstruction *
SpirvEmitter::processTextureLevelOfDetail(const CXXMemberCallExpr *expr,
                                          bool unclamped) {
  // Possible signatures are as follows:
  // Texture1D(Array).CalculateLevelOfDetail(SamplerState S, float x);
  // Texture2D(Array).CalculateLevelOfDetail(SamplerState S, float2 xy);
  // TextureCube(Array).CalculateLevelOfDetail(SamplerState S, float3 xyz);
  // Texture3D.CalculateLevelOfDetail(SamplerState S, float3 xyz);
  // Return type is always a single float (LOD).
  assert(expr->getNumArgs() == 2u);
  const auto *object = expr->getImplicitObjectArgument();
  auto *objectInfo = loadIfGLValue(object);
  auto *samplerState = doExpr(expr->getArg(0));
  auto *coordinate = doExpr(expr->getArg(1));

  auto *sampledImage = spvBuilder.createSampledImage(
      object->getType(), objectInfo, samplerState, expr->getExprLoc());

  // The result type of OpImageQueryLod must be a float2.
  const QualType queryResultType =
      astContext.getExtVectorType(astContext.FloatTy, 2u);
  auto *query =
      spvBuilder.createImageQuery(spv::Op::OpImageQueryLod, queryResultType,
                                  expr->getExprLoc(), sampledImage, coordinate);

  // The first component of the float2 contains the mipmap array layer.
  // The second component of the float2 represents the unclamped lod.
  return spvBuilder.createCompositeExtract(astContext.FloatTy, query,
                                           unclamped ? 1 : 0,
                                           expr->getCallee()->getExprLoc());
}

SpirvInstruction *SpirvEmitter::processTextureGatherRGBACmpRGBA(
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
  const auto loc = expr->getCallee()->getExprLoc();
  const QualType imageType = imageExpr->getType();
  const QualType retType = callee->getReturnType();

  // If the last arg is an unsigned integer, it must be the status.
  const bool hasStatusArg =
      expr->getArg(numArgs - 1)->getType()->isUnsignedIntegerType();

  // Subtract 1 for status arg (if it exists), subtract 1 for compare_value (if
  // it exists), and subtract 2 for SamplerState and location.
  const auto numOffsetArgs = numArgs - hasStatusArg - isCmp - 2;
  // No offset args for TextureCube, 1 or 4 offset args for the rest.
  assert(numOffsetArgs == 0 || numOffsetArgs == 1 || numOffsetArgs == 4);

  auto *image = loadIfGLValue(imageExpr);
  auto *sampler = doExpr(expr->getArg(0));
  auto *coordinate = doExpr(expr->getArg(1));
  auto *compareVal = isCmp ? doExpr(expr->getArg(2)) : nullptr;

  // Handle offsets (if any).
  bool needsEmulation = false;
  SpirvInstruction *constOffset = nullptr, *varOffset = nullptr,
                   *constOffsets = nullptr;
  if (numOffsetArgs == 1) {
    // The offset arg is not optional.
    handleOffsetInMethodCall(expr, 2 + isCmp, &constOffset, &varOffset);
  } else if (numOffsetArgs == 4) {
    auto *offset0 = tryToEvaluateAsConst(expr->getArg(2 + isCmp));
    auto *offset1 = tryToEvaluateAsConst(expr->getArg(3 + isCmp));
    auto *offset2 = tryToEvaluateAsConst(expr->getArg(4 + isCmp));
    auto *offset3 = tryToEvaluateAsConst(expr->getArg(5 + isCmp));

    // If any of the offsets is not constant, we then need to emulate the call
    // using 4 OpImageGather instructions. Otherwise, we can leverage the
    // ConstOffsets image operand.
    if (offset0 && offset1 && offset2 && offset3) {
      const QualType v2i32 = astContext.getExtVectorType(astContext.IntTy, 2);
      const auto offsetType = astContext.getConstantArrayType(
          v2i32, llvm::APInt(32, 4), clang::ArrayType::Normal, 0);
      constOffsets = spvBuilder.getConstantComposite(
          offsetType, {offset0, offset1, offset2, offset3});
    } else {
      needsEmulation = true;
    }
  }

  auto *status = hasStatusArg ? doExpr(expr->getArg(numArgs - 1)) : nullptr;

  if (needsEmulation) {
    const auto elemType = hlsl::GetHLSLVecElementType(callee->getReturnType());

    SpirvInstruction *texels[4];
    for (uint32_t i = 0; i < 4; ++i) {
      varOffset = doExpr(expr->getArg(2 + isCmp + i));
      auto *gatherRet = spvBuilder.createImageGather(
          retType, imageType, image, sampler, coordinate,
          spvBuilder.getConstantInt(astContext.IntTy,
                                    llvm::APInt(32, component, true)),
          compareVal,
          /*constOffset*/ nullptr, varOffset, /*constOffsets*/ nullptr,
          /*sampleNumber*/ nullptr, status, loc);
      texels[i] =
          spvBuilder.createCompositeExtract(elemType, gatherRet, {i}, loc);
    }
    return spvBuilder.createCompositeConstruct(
        retType, {texels[0], texels[1], texels[2], texels[3]}, loc);
  }

  return spvBuilder.createImageGather(
      retType, imageType, image, sampler, coordinate,
      spvBuilder.getConstantInt(astContext.IntTy,
                                llvm::APInt(32, component, true)),
      compareVal, constOffset, varOffset, constOffsets,
      /*sampleNumber*/ nullptr, status, loc);
}

SpirvInstruction *
SpirvEmitter::processTextureGatherCmp(const CXXMemberCallExpr *expr) {
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
  const auto loc = expr->getExprLoc();
  const bool hasStatusArg =
      expr->getArg(numArgs - 1)->getType()->isUnsignedIntegerType();
  const bool hasOffsetArg = (numArgs == 5) || (numArgs == 4 && !hasStatusArg);

  const auto *imageExpr = expr->getImplicitObjectArgument();
  auto *image = loadIfGLValue(imageExpr);
  auto *sampler = doExpr(expr->getArg(0));
  auto *coordinate = doExpr(expr->getArg(1));
  auto *comparator = doExpr(expr->getArg(2));
  SpirvInstruction *constOffset = nullptr, *varOffset = nullptr;
  if (hasOffsetArg)
    handleOffsetInMethodCall(expr, 3, &constOffset, &varOffset);

  const auto retType = callee->getReturnType();
  const auto imageType = imageExpr->getType();
  const auto status =
      hasStatusArg ? doExpr(expr->getArg(numArgs - 1)) : nullptr;

  return spvBuilder.createImageGather(
      retType, imageType, image, sampler, coordinate,
      /*component*/ nullptr, comparator, constOffset, varOffset,
      /*constOffsets*/ nullptr,
      /*sampleNumber*/ nullptr, status, loc);
}

SpirvInstruction *SpirvEmitter::processBufferTextureLoad(
    const Expr *object, SpirvInstruction *location,
    SpirvInstruction *constOffset, SpirvInstruction *varOffset,
    SpirvInstruction *lod, SpirvInstruction *residencyCode,
    SourceLocation loc) {
  // Loading for Buffer and RWBuffer translates to an OpImageFetch.
  // The result type of an OpImageFetch must be a vec4 of float or int.
  const auto type = object->getType();
  assert(isBuffer(type) || isRWBuffer(type) || isTexture(type) ||
         isRWTexture(type) || isSubpassInput(type) || isSubpassInputMS(type));

  const bool doFetch = isBuffer(type) || isTexture(type);

  auto *objectInfo = loadIfGLValue(object);

  // For Texture2DMS and Texture2DMSArray, Sample must be used rather than Lod.
  SpirvInstruction *sampleNumber = nullptr;
  if (isTextureMS(type) || isSubpassInputMS(type)) {
    sampleNumber = lod;
    lod = nullptr;
  }

  const auto sampledType = hlsl::GetHLSLResourceResultType(type);
  QualType elemType = sampledType;
  uint32_t elemCount = 1;
  bool isTemplateOverStruct = false;

  // Check whether the template type is a vector type or struct type.
  if (!isVectorType(sampledType, &elemType, &elemCount)) {
    if (sampledType->getAsStructureType()) {
      isTemplateOverStruct = true;
      // For struct type, we need to make sure it can fit into a 4-component
      // vector. Detailed failing reasons will be emitted by the function so
      // we don't need to emit errors here.
      if (!canFitIntoOneRegister(astContext, sampledType, &elemType,
                                 &elemCount))
        return nullptr;
    }
  }

  if (!elemType->isFloatingType() && !elemType->isIntegerType()) {
    emitError("loading %0 value unsupported", object->getExprLoc()) << type;
    return nullptr;
  }

  // OpImageFetch and OpImageRead can only fetch a vector of 4 elements.
  const QualType texelType = astContext.getExtVectorType(elemType, 4u);
  auto *texel = spvBuilder.createImageFetchOrRead(
      doFetch, texelType, type, objectInfo, location, lod, constOffset,
      varOffset, /*constOffsets*/ nullptr, sampleNumber, residencyCode, loc);

  // If the result type is a vec1, vec2, or vec3, some extra processing
  // (extraction) is required.
  auto *retVal = extractVecFromVec4(texel, elemCount, elemType, loc);
  if (isTemplateOverStruct) {
    // Convert to the struct so that we are consistent with types in the AST.
    retVal = convertVectorToStruct(sampledType, elemType, retVal, loc);
  }
  retVal->setRValue();
  return retVal;
}

SpirvInstruction *SpirvEmitter::processByteAddressBufferLoadStore(
    const CXXMemberCallExpr *expr, uint32_t numWords, bool doStore) {
  SpirvInstruction *result = nullptr;
  const auto object = expr->getImplicitObjectArgument();
  auto *objectInfo = loadIfAliasVarRef(object);
  assert(numWords >= 1 && numWords <= 4);
  if (doStore) {
    assert(isRWByteAddressBuffer(object->getType()));
    assert(expr->getNumArgs() == 2);
  } else {
    assert(isRWByteAddressBuffer(object->getType()) ||
           isByteAddressBuffer(object->getType()));
    if (expr->getNumArgs() == 2) {
      emitError(
          "(RW)ByteAddressBuffer::Load(in address, out status) not supported",
          expr->getExprLoc());
      return 0;
    }
  }
  const Expr *addressExpr = expr->getArg(0);
  auto *byteAddress = doExpr(addressExpr);
  const QualType addressType = addressExpr->getType();
  // The front-end prevents usage of templated Load2, Load3, Load4, Store2,
  // Store3, Store4 intrinsic functions.
  const bool isTemplatedLoadOrStore =
      (numWords == 1) &&
      (doStore ? !expr->getArg(1)->getType()->isSpecificBuiltinType(
                     BuiltinType::UInt)
               : !expr->getType()->isSpecificBuiltinType(BuiltinType::UInt));

  // Do a OpShiftRightLogical by 2 (divide by 4 to get aligned memory
  // access). The AST always casts the address to unsinged integer, so shift
  // by unsinged integer 2.
  auto *constUint2 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 2));
  SpirvInstruction *address =
      spvBuilder.createBinaryOp(spv::Op::OpShiftRightLogical, addressType,
                                byteAddress, constUint2, expr->getExprLoc());

  if (isTemplatedLoadOrStore) {
    // Templated load. Need to (potentially) perform more
    // loads/casts/composite-constructs.
    uint32_t bitOffset = 0;
    if (doStore) {
      auto *values = doExpr(expr->getArg(1));
      RawBufferHandler(*this).processTemplatedStoreToBuffer(
          values, objectInfo, address, expr->getArg(1)->getType(), bitOffset);
      return nullptr;
    } else {
      RawBufferHandler rawBufferHandler(*this);
      return rawBufferHandler.processTemplatedLoadFromBuffer(
          objectInfo, address, expr->getType(), bitOffset);
    }
  }

  // Perform access chain into the RWByteAddressBuffer.
  // First index must be zero (member 0 of the struct is a
  // runtimeArray). The second index passed to OpAccessChain should be
  // the address.
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  if (doStore) {
    auto *values = doExpr(expr->getArg(1));
    auto *curStoreAddress = address;
    for (uint32_t wordCounter = 0; wordCounter < numWords; ++wordCounter) {
      // Extract a 32-bit word from the input.
      auto *curValue = numWords == 1
                           ? values
                           : spvBuilder.createCompositeExtract(
                                 astContext.UnsignedIntTy, values,
                                 {wordCounter}, expr->getArg(1)->getExprLoc());

      // Update the output address if necessary.
      if (wordCounter > 0) {
        auto *offset = spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                                 llvm::APInt(32, wordCounter));
        curStoreAddress =
            spvBuilder.createBinaryOp(spv::Op::OpIAdd, addressType, address,
                                      offset, expr->getCallee()->getExprLoc());
      }

      // Store the word to the right address at the output.
      auto *storePtr = spvBuilder.createAccessChain(
          astContext.UnsignedIntTy, objectInfo, {constUint0, curStoreAddress},
          object->getLocStart());
      spvBuilder.createStore(storePtr, curValue,
                             expr->getCallee()->getExprLoc());
    }
  } else {
    auto *loadPtr = spvBuilder.createAccessChain(
        astContext.UnsignedIntTy, objectInfo, {constUint0, address},
        object->getLocStart());
    result = spvBuilder.createLoad(astContext.UnsignedIntTy, loadPtr,
                                   expr->getCallee()->getExprLoc());
    if (numWords > 1) {
      // Load word 2, 3, and 4 where necessary. Use OpCompositeConstruct to
      // return a vector result.
      llvm::SmallVector<SpirvInstruction *, 4> values;
      values.push_back(result);
      for (uint32_t wordCounter = 2; wordCounter <= numWords; ++wordCounter) {
        auto *offset = spvBuilder.getConstantInt(
            astContext.UnsignedIntTy, llvm::APInt(32, wordCounter - 1));
        auto *newAddress =
            spvBuilder.createBinaryOp(spv::Op::OpIAdd, addressType, address,
                                      offset, expr->getCallee()->getExprLoc());
        loadPtr = spvBuilder.createAccessChain(
            astContext.UnsignedIntTy, objectInfo, {constUint0, newAddress},
            object->getLocStart());
        values.push_back(
            spvBuilder.createLoad(astContext.UnsignedIntTy, loadPtr,
                                  expr->getCallee()->getExprLoc()));
      }
      const QualType resultType =
          astContext.getExtVectorType(addressType, numWords);
      result = spvBuilder.createCompositeConstruct(resultType, values,
                                                   expr->getLocStart());
      result->setRValue();
    }
  }

  return result;
}

SpirvInstruction *
SpirvEmitter::processStructuredBufferLoad(const CXXMemberCallExpr *expr) {
  if (expr->getNumArgs() == 2) {
    emitError(
        "(RW)StructuredBuffer::Load(in location, out status) not supported",
        expr->getExprLoc());
    return 0;
  }

  const auto *buffer = expr->getImplicitObjectArgument();
  auto *info = loadIfAliasVarRef(buffer);

  const QualType structType =
      hlsl::GetHLSLResourceResultType(buffer->getType());

  auto *zero = spvBuilder.getConstantInt(astContext.IntTy, llvm::APInt(32, 0));
  auto *index = doExpr(expr->getArg(0));

  return turnIntoElementPtr(buffer->getType(), info, structType, {zero, index},
                            buffer->getExprLoc());
}

SpirvInstruction *
SpirvEmitter::incDecRWACSBufferCounter(const CXXMemberCallExpr *expr,
                                       bool isInc, bool loadObject) {
  auto *zero =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *sOne =
      spvBuilder.getConstantInt(astContext.IntTy, llvm::APInt(32, 1, true));

  const auto srcLoc = expr->getCallee()->getExprLoc();
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
    return nullptr;
  }

  auto *counterPtr = spvBuilder.createAccessChain(
      astContext.IntTy, counterPair->get(spvBuilder, spvContext), {zero},
      srcLoc);

  SpirvInstruction *index = nullptr;
  if (isInc) {
    index = spvBuilder.createAtomicOp(
        spv::Op::OpAtomicIAdd, astContext.IntTy, counterPtr, spv::Scope::Device,
        spv::MemorySemanticsMask::MaskNone, sOne, srcLoc);
  } else {
    // Note that OpAtomicISub returns the value before the subtraction;
    // so we need to do substraction again with OpAtomicISub's return value.
    auto *prev = spvBuilder.createAtomicOp(
        spv::Op::OpAtomicISub, astContext.IntTy, counterPtr, spv::Scope::Device,
        spv::MemorySemanticsMask::MaskNone, sOne, srcLoc);
    index = spvBuilder.createBinaryOp(spv::Op::OpISub, astContext.IntTy, prev,
                                      sOne, srcLoc);
  }

  return index;
}

bool SpirvEmitter::tryToAssignCounterVar(const DeclaratorDecl *dstDecl,
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
    dstPair->assign(*srcPair, spvBuilder, spvContext);
    return true;
  }

  // Handle AssocCounter#3
  llvm::SmallVector<uint32_t, 4> srcIndices;
  const auto *dstFields = declIdMapper.getCounterVarFields(dstDecl);
  const auto *srcFields = getIntermediateACSBufferCounter(srcExpr, &srcIndices);

  if (dstFields && srcFields) {
    // The destination is a struct whose fields are directly alias resources.
    // But that's not necessarily true for the source, which can be deep
    // nested structs. That means they will have different index "prefixes"
    // for all their fields; while the "prefix" for destination is effectively
    // an empty list (since it is not nested in other structs). We need to
    // strip the index prefix from the source.
    return dstFields->assign(*srcFields, /*dstIndices=*/{}, srcIndices,
                             spvBuilder, spvContext);
  }

  // AssocCounter#2 and AssocCounter#4 for the lhs cannot happen since the lhs
  // is a stand-alone decl in this method.

  return false;
}

bool SpirvEmitter::tryToAssignCounterVar(const Expr *dstExpr,
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
    dstPair->assign(*srcPair, spvBuilder, spvContext);
    return true;
  }

  // Handle AssocCounter#3 & AssocCounter#4
  llvm::SmallVector<uint32_t, 4> dstIndices;
  llvm::SmallVector<uint32_t, 4> srcIndices;
  const auto *srcFields = getIntermediateACSBufferCounter(srcExpr, &srcIndices);
  const auto *dstFields = getIntermediateACSBufferCounter(dstExpr, &dstIndices);

  if (dstFields && srcFields) {
    return dstFields->assign(*srcFields, dstIndices, srcIndices, spvBuilder,
                             spvContext);
  }

  return false;
}

const CounterIdAliasPair *
SpirvEmitter::getFinalACSBufferCounter(const Expr *expr) {
  // AssocCounter#1: referencing some stand-alone variable
  if (const auto *decl = getReferencedDef(expr))
    return declIdMapper.getCounterIdAliasPair(decl);

  // AssocCounter#2: referencing some non-struct field
  llvm::SmallVector<uint32_t, 4> rawIndices;

  const auto *base = collectArrayStructIndices(
      expr, /*rawIndex=*/true, &rawIndices, /*indices*/ nullptr);
  const auto *decl =
      (base && isa<CXXThisExpr>(base))
          ? getOrCreateDeclForMethodObject(cast<CXXMethodDecl>(curFunction))
          : getReferencedDef(base);
  return declIdMapper.getCounterIdAliasPair(decl, &rawIndices);
}

const CounterVarFields *SpirvEmitter::getIntermediateACSBufferCounter(
    const Expr *expr, llvm::SmallVector<uint32_t, 4> *rawIndices) {
  const auto *base = collectArrayStructIndices(expr, /*rawIndex=*/true,
                                               rawIndices, /*indices*/ nullptr);
  const auto *decl =
      (base && isa<CXXThisExpr>(base))
          // Use the decl we created to represent the implicit object
          ? getOrCreateDeclForMethodObject(cast<CXXMethodDecl>(curFunction))
          // Find the referenced decl from the original source code
          : getReferencedDef(base);

  return declIdMapper.getCounterVarFields(decl);
}

const ImplicitParamDecl *
SpirvEmitter::getOrCreateDeclForMethodObject(const CXXMethodDecl *method) {
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

SpirvInstruction *
SpirvEmitter::processACSBufferAppendConsume(const CXXMemberCallExpr *expr) {
  const bool isAppend = expr->getNumArgs() == 1;

  auto *zero =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));

  const auto *object =
      expr->getImplicitObjectArgument()->IgnoreParenNoopCasts(astContext);

  auto *bufferInfo = loadIfAliasVarRef(object);

  auto *index = incDecRWACSBufferCounter(
      expr, isAppend,
      // We have already translated the object in the above. Avoid duplication.
      /*loadObject=*/false);

  auto bufferElemTy = hlsl::GetHLSLResourceResultType(object->getType());

  // If this is a variable to communicate with host e.g., ACSBuffer
  // and its type is bool or vector of bool, its effective type used
  // for SPIRV must be uint not bool. We must convert it to uint here.
  bool needCast = false;
  if (bufferInfo->getLayoutRule() != SpirvLayoutRule::Void &&
      isBoolOrVecOfBoolType(bufferElemTy)) {
    uint32_t vecSize = 1;
    const bool isVec = isVectorType(bufferElemTy, nullptr, &vecSize);
    bufferElemTy =
        isVec ? astContext.getExtVectorType(astContext.UnsignedIntTy, vecSize)
              : astContext.UnsignedIntTy;
    needCast = true;
  }

  bufferInfo = turnIntoElementPtr(object->getType(), bufferInfo, bufferElemTy,
                                  {zero, index}, object->getExprLoc());

  if (isAppend) {
    // Write out the value
    auto *arg0 = doExpr(expr->getArg(0));
    if (!arg0)
      return nullptr;

    if (!arg0->isRValue()) {
      arg0 = spvBuilder.createLoad(bufferElemTy, arg0,
                                   expr->getArg(0)->getExprLoc());
    }
    if (needCast &&
        !isSameType(astContext, bufferElemTy, arg0->getAstResultType())) {
      arg0 = castToType(arg0, arg0->getAstResultType(), bufferElemTy,
                        expr->getArg(0)->getExprLoc());
    }
    storeValue(bufferInfo, arg0, bufferElemTy, expr->getCallee()->getExprLoc());
    return 0;
  } else {
    // Note that we are returning a pointer (lvalue) here inorder to further
    // acess the fields in this element, e.g., buffer.Consume().a.b. So we
    // cannot forcefully set all normal function calls as returning rvalue.
    return bufferInfo;
  }
}

SpirvInstruction *
SpirvEmitter::processStreamOutputAppend(const CXXMemberCallExpr *expr) {
  // TODO: handle multiple stream-output objects
  const auto *object =
      expr->getImplicitObjectArgument()->IgnoreParenNoopCasts(astContext);
  const auto *stream = cast<DeclRefExpr>(object)->getDecl();
  auto *value = doExpr(expr->getArg(0));

  declIdMapper.writeBackOutputStream(stream, stream->getType(), value);
  spvBuilder.createEmitVertex(expr->getExprLoc());

  return nullptr;
}

SpirvInstruction *
SpirvEmitter::processStreamOutputRestart(const CXXMemberCallExpr *expr) {
  // TODO: handle multiple stream-output objects
  spvBuilder.createEndPrimitive(expr->getExprLoc());
  return 0;
}

SpirvInstruction *
SpirvEmitter::emitGetSamplePosition(SpirvInstruction *sampleCount,
                                    SpirvInstruction *sampleIndex,
                                    SourceLocation loc) {
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

  const auto v2f32Type = astContext.getExtVectorType(astContext.FloatTy, 2);

  // Creates a SPIR-V function scope variable of type float2[len].
  const auto createArray = [this, v2f32Type, loc](const Float2 *ptr,
                                                  uint32_t len) {
    llvm::SmallVector<SpirvConstant *, 16> components;
    for (uint32_t i = 0; i < len; ++i) {
      auto *x = spvBuilder.getConstantFloat(astContext.FloatTy,
                                            llvm::APFloat(ptr[i].x));
      auto *y = spvBuilder.getConstantFloat(astContext.FloatTy,
                                            llvm::APFloat(ptr[i].y));
      components.push_back(spvBuilder.getConstantComposite(v2f32Type, {x, y}));
    }

    const auto arrType = astContext.getConstantArrayType(
        v2f32Type, llvm::APInt(32, len), clang::ArrayType::Normal, 0);
    auto *val = spvBuilder.getConstantComposite(arrType, components);

    const std::string varName =
        "var.GetSamplePosition.data." + std::to_string(len);
    auto *var = spvBuilder.addFnVar(arrType, loc, varName);
    spvBuilder.createStore(var, val, loc);
    return var;
  };

  auto *pos2Arr = createArray(pos2, 2);
  auto *pos4Arr = createArray(pos4, 4);
  auto *pos8Arr = createArray(pos8, 8);
  auto *pos16Arr = createArray(pos16, 16);

  auto *resultVar =
      spvBuilder.addFnVar(v2f32Type, loc, "var.GetSamplePosition.result");

  auto *then2BB = spvBuilder.createBasicBlock("if.GetSamplePosition.then2");
  auto *then4BB = spvBuilder.createBasicBlock("if.GetSamplePosition.then4");
  auto *then8BB = spvBuilder.createBasicBlock("if.GetSamplePosition.then8");
  auto *then16BB = spvBuilder.createBasicBlock("if.GetSamplePosition.then16");

  auto *else2BB = spvBuilder.createBasicBlock("if.GetSamplePosition.else2");
  auto *else4BB = spvBuilder.createBasicBlock("if.GetSamplePosition.else4");
  auto *else8BB = spvBuilder.createBasicBlock("if.GetSamplePosition.else8");
  auto *else16BB = spvBuilder.createBasicBlock("if.GetSamplePosition.else16");

  auto *merge2BB = spvBuilder.createBasicBlock("if.GetSamplePosition.merge2");
  auto *merge4BB = spvBuilder.createBasicBlock("if.GetSamplePosition.merge4");
  auto *merge8BB = spvBuilder.createBasicBlock("if.GetSamplePosition.merge8");
  auto *merge16BB = spvBuilder.createBasicBlock("if.GetSamplePosition.merge16");

  //   if (count == 2) {
  const auto check2 = spvBuilder.createBinaryOp(
      spv::Op::OpIEqual, astContext.BoolTy, sampleCount,
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 2)),
      loc);
  spvBuilder.createConditionalBranch(check2, then2BB, else2BB, loc, merge2BB);
  spvBuilder.addSuccessor(then2BB);
  spvBuilder.addSuccessor(else2BB);
  spvBuilder.setMergeTarget(merge2BB);

  //     position = pos2[index];
  //   }
  spvBuilder.setInsertPoint(then2BB);
  auto *ac =
      spvBuilder.createAccessChain(v2f32Type, pos2Arr, {sampleIndex}, loc);
  spvBuilder.createStore(resultVar, spvBuilder.createLoad(v2f32Type, ac, loc),
                         loc);
  spvBuilder.createBranch(merge2BB, loc);
  spvBuilder.addSuccessor(merge2BB);

  //   else if (count == 4) {
  spvBuilder.setInsertPoint(else2BB);
  const auto check4 = spvBuilder.createBinaryOp(
      spv::Op::OpIEqual, astContext.BoolTy, sampleCount,
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 4)),
      loc);
  spvBuilder.createConditionalBranch(check4, then4BB, else4BB, loc, merge4BB);
  spvBuilder.addSuccessor(then4BB);
  spvBuilder.addSuccessor(else4BB);
  spvBuilder.setMergeTarget(merge4BB);

  //     position = pos4[index];
  //   }
  spvBuilder.setInsertPoint(then4BB);
  ac = spvBuilder.createAccessChain(v2f32Type, pos4Arr, {sampleIndex}, loc);
  spvBuilder.createStore(resultVar, spvBuilder.createLoad(v2f32Type, ac, loc),
                         loc);
  spvBuilder.createBranch(merge4BB, loc);
  spvBuilder.addSuccessor(merge4BB);

  //   else if (count == 8) {
  spvBuilder.setInsertPoint(else4BB);
  const auto check8 = spvBuilder.createBinaryOp(
      spv::Op::OpIEqual, astContext.BoolTy, sampleCount,
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 8)),
      loc);
  spvBuilder.createConditionalBranch(check8, then8BB, else8BB, loc, merge8BB);
  spvBuilder.addSuccessor(then8BB);
  spvBuilder.addSuccessor(else8BB);
  spvBuilder.setMergeTarget(merge8BB);

  //     position = pos8[index];
  //   }
  spvBuilder.setInsertPoint(then8BB);
  ac = spvBuilder.createAccessChain(v2f32Type, pos8Arr, {sampleIndex}, loc);
  spvBuilder.createStore(resultVar, spvBuilder.createLoad(v2f32Type, ac, loc),
                         loc);
  spvBuilder.createBranch(merge8BB, loc);
  spvBuilder.addSuccessor(merge8BB);

  //   else if (count == 16) {
  spvBuilder.setInsertPoint(else8BB);
  const auto check16 = spvBuilder.createBinaryOp(
      spv::Op::OpIEqual, astContext.BoolTy, sampleCount,
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 16)),
      loc);
  spvBuilder.createConditionalBranch(check16, then16BB, else16BB, loc,
                                     merge16BB);
  spvBuilder.addSuccessor(then16BB);
  spvBuilder.addSuccessor(else16BB);
  spvBuilder.setMergeTarget(merge16BB);

  //     position = pos16[index];
  //   }
  spvBuilder.setInsertPoint(then16BB);
  ac = spvBuilder.createAccessChain(v2f32Type, pos16Arr, {sampleIndex}, loc);
  spvBuilder.createStore(resultVar, spvBuilder.createLoad(v2f32Type, ac, loc),
                         loc);
  spvBuilder.createBranch(merge16BB, loc);
  spvBuilder.addSuccessor(merge16BB);

  //   else {
  //     position = float2(0.0f, 0.0f);
  //   }
  spvBuilder.setInsertPoint(else16BB);
  auto *zero =
      spvBuilder.getConstantFloat(astContext.FloatTy, llvm::APFloat(0.0f));
  auto *v2f32Zero = spvBuilder.getConstantComposite(v2f32Type, {zero, zero});
  spvBuilder.createStore(resultVar, v2f32Zero, loc);
  spvBuilder.createBranch(merge16BB, loc);
  spvBuilder.addSuccessor(merge16BB);

  spvBuilder.setInsertPoint(merge16BB);
  spvBuilder.createBranch(merge8BB, loc);
  spvBuilder.addSuccessor(merge8BB);

  spvBuilder.setInsertPoint(merge8BB);
  spvBuilder.createBranch(merge4BB, loc);
  spvBuilder.addSuccessor(merge4BB);

  spvBuilder.setInsertPoint(merge4BB);
  spvBuilder.createBranch(merge2BB, loc);
  spvBuilder.addSuccessor(merge2BB);

  spvBuilder.setInsertPoint(merge2BB);
  return spvBuilder.createLoad(v2f32Type, resultVar, loc);
}

SpirvInstruction *
SpirvEmitter::doCXXMemberCallExpr(const CXXMemberCallExpr *expr) {
  const FunctionDecl *callee = expr->getDirectCallee();

  llvm::StringRef group;
  uint32_t opcode = static_cast<uint32_t>(hlsl::IntrinsicOp::Num_Intrinsics);

  if (hlsl::GetIntrinsicOp(callee, opcode, group)) {
    return processIntrinsicMemberCall(expr,
                                      static_cast<hlsl::IntrinsicOp>(opcode));
  }

  return processCall(expr);
}

void SpirvEmitter::handleOffsetInMethodCall(const CXXMemberCallExpr *expr,
                                            uint32_t index,
                                            SpirvInstruction **constOffset,
                                            SpirvInstruction **varOffset) {
  assert(constOffset && varOffset);
  // Ensure the given arg index is not out-of-range.
  assert(index < expr->getNumArgs());

  *constOffset = *varOffset = nullptr; // Initialize both first
  if ((*constOffset = tryToEvaluateAsConst(expr->getArg(index))))
    return; // Constant offset
  else
    *varOffset = doExpr(expr->getArg(index));
}

SpirvInstruction *
SpirvEmitter::processIntrinsicMemberCall(const CXXMemberCallExpr *expr,
                                         hlsl::IntrinsicOp opcode) {
  using namespace hlsl;

  SpirvInstruction *retVal = nullptr;
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
    retVal = processTextureLevelOfDetail(expr, /* unclamped */ false);
  case IntrinsicOp::MOP_CalculateLevelOfDetailUnclamped:
    retVal = processTextureLevelOfDetail(expr, /* unclamped */ true);
    break;
  case IntrinsicOp::MOP_IncrementCounter:
    retVal =
        spvBuilder.createUnaryOp(spv::Op::OpBitcast, astContext.UnsignedIntTy,
                                 incDecRWACSBufferCounter(expr, /*isInc*/ true),
                                 expr->getCallee()->getExprLoc());
    break;
  case IntrinsicOp::MOP_DecrementCounter:
    retVal = spvBuilder.createUnaryOp(
        spv::Op::OpBitcast, astContext.UnsignedIntTy,
        incDecRWACSBufferCounter(expr, /*isInc*/ false),
        expr->getCallee()->getExprLoc());
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
    emitError("no equivalent for %0 intrinsic method in Vulkan",
              expr->getCallee()->getExprLoc())
        << expr->getMethodDecl()->getName();
    return nullptr;
  default:
    emitError("intrinsic '%0' method unimplemented",
              expr->getCallee()->getExprLoc())
        << expr->getDirectCallee()->getName();
    return nullptr;
  }

  if (retVal)
    retVal->setRValue();
  return retVal;
}

SpirvInstruction *SpirvEmitter::createImageSample(
    QualType retType, QualType imageType, SpirvInstruction *image,
    SpirvInstruction *sampler, SpirvInstruction *coordinate,
    SpirvInstruction *compareVal, SpirvInstruction *bias, SpirvInstruction *lod,
    std::pair<SpirvInstruction *, SpirvInstruction *> grad,
    SpirvInstruction *constOffset, SpirvInstruction *varOffset,
    SpirvInstruction *constOffsets, SpirvInstruction *sample,
    SpirvInstruction *minLod, SpirvInstruction *residencyCodeId,
    SourceLocation loc) {

  // SampleDref* instructions in SPIR-V always return a scalar.
  // They also have the correct type in HLSL.
  if (compareVal) {
    return spvBuilder.createImageSample(retType, imageType, image, sampler,
                                        coordinate, compareVal, bias, lod, grad,
                                        constOffset, varOffset, constOffsets,
                                        sample, minLod, residencyCodeId, loc);
  }

  // Non-Dref Sample instructions in SPIR-V must always return a vec4.
  auto texelType = retType;
  QualType elemType = {};
  uint32_t retVecSize = 0;
  if (isVectorType(retType, &elemType, &retVecSize) && retVecSize != 4) {
    texelType = astContext.getExtVectorType(elemType, 4);
  } else if (isScalarType(retType)) {
    retVecSize = 1;
    elemType = retType;
    texelType = astContext.getExtVectorType(retType, 4);
  }

  // The Lod and Grad image operands requires explicit-lod instructions.
  // Otherwise we use implicit-lod instructions.
  const bool isExplicit = lod || (grad.first && grad.second);

  // Implicit-lod instructions are only allowed in pixel shader.
  if (!spvContext.isPS() && !isExplicit)
    needsLegalization = true;

  auto *retVal = spvBuilder.createImageSample(
      texelType, imageType, image, sampler, coordinate, compareVal, bias, lod,
      grad, constOffset, varOffset, constOffsets, sample, minLod,
      residencyCodeId, loc);

  // Extract smaller vector from the vec4 result if necessary.
  if (texelType != retType) {
    retVal = extractVecFromVec4(retVal, retVecSize, elemType, loc);
  }

  return retVal;
}

SpirvInstruction *
SpirvEmitter::processTextureSampleGather(const CXXMemberCallExpr *expr,
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
  const auto loc = expr->getExprLoc();
  const bool hasStatusArg =
      expr->getArg(numArgs - 1)->getType()->isUnsignedIntegerType();

  SpirvInstruction *clamp = nullptr;
  if (numArgs > 2 && expr->getArg(2)->getType()->isFloatingType())
    clamp = doExpr(expr->getArg(2));
  else if (numArgs > 3 && expr->getArg(3)->getType()->isFloatingType())
    clamp = doExpr(expr->getArg(3));
  const bool hasClampArg = (clamp != 0);
  const auto status =
      hasStatusArg ? doExpr(expr->getArg(numArgs - 1)) : nullptr;

  // Subtract 1 for status (if it exists), subtract 1 for clamp (if it exists),
  // and subtract 2 for sampler_state and location.
  const bool hasOffsetArg = numArgs - hasStatusArg - hasClampArg - 2 > 0;

  const auto *imageExpr = expr->getImplicitObjectArgument();
  const QualType imageType = imageExpr->getType();
  auto *image = loadIfGLValue(imageExpr);
  auto *sampler = doExpr(expr->getArg(0));
  auto *coordinate = doExpr(expr->getArg(1));
  // .Sample()/.Gather() may have a third optional paramter for offset.
  SpirvInstruction *constOffset = nullptr, *varOffset = nullptr;
  if (hasOffsetArg)
    handleOffsetInMethodCall(expr, 2, &constOffset, &varOffset);

  const auto retType = expr->getDirectCallee()->getReturnType();
  if (isSample) {
    return createImageSample(retType, imageType, image, sampler, coordinate,
                             /*compareVal*/ nullptr, /*bias*/ nullptr,
                             /*lod*/ nullptr, std::make_pair(nullptr, nullptr),
                             constOffset, varOffset,
                             /*constOffsets*/ nullptr, /*sampleNumber*/ nullptr,
                             /*minLod*/ clamp, status,
                             expr->getCallee()->getLocStart());
  } else {
    return spvBuilder.createImageGather(
        retType, imageType, image, sampler, coordinate,
        // .Gather() doc says we return four components of red data.
        spvBuilder.getConstantInt(astContext.IntTy, llvm::APInt(32, 0)),
        /*compareVal*/ nullptr, constOffset, varOffset,
        /*constOffsets*/ nullptr, /*sampleNumber*/ nullptr, status, loc);
  }
}

SpirvInstruction *
SpirvEmitter::processTextureSampleBiasLevel(const CXXMemberCallExpr *expr,
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
  auto *status = hasStatusArg ? doExpr(expr->getArg(numArgs - 1)) : nullptr;

  SpirvInstruction *clamp = nullptr;
  // The .SampleLevel() methods do not take the clamp argument.
  if (isBias) {
    if (numArgs > 3 && expr->getArg(3)->getType()->isFloatingType())
      clamp = doExpr(expr->getArg(3));
    else if (numArgs > 4 && expr->getArg(4)->getType()->isFloatingType())
      clamp = doExpr(expr->getArg(4));
  }
  const bool hasClampArg = clamp != nullptr;

  // Subtract 1 for clamp (if it exists), 1 for status (if it exists),
  // and 3 for sampler_state, location, and Bias/LOD.
  const bool hasOffsetArg = numArgs - hasClampArg - hasStatusArg - 3 > 0;

  const auto *imageExpr = expr->getImplicitObjectArgument();
  const QualType imageType = imageExpr->getType();
  auto *image = loadIfGLValue(imageExpr);
  auto *sampler = doExpr(expr->getArg(0));
  auto *coordinate = doExpr(expr->getArg(1));
  SpirvInstruction *lod = nullptr;
  SpirvInstruction *bias = nullptr;
  if (isBias) {
    bias = doExpr(expr->getArg(2));
  } else {
    lod = doExpr(expr->getArg(2));
  }
  // If offset is present in .Bias()/.SampleLevel(), it is the fourth argument.
  SpirvInstruction *constOffset = nullptr, *varOffset = nullptr;
  if (hasOffsetArg)
    handleOffsetInMethodCall(expr, 3, &constOffset, &varOffset);

  const auto retType = expr->getDirectCallee()->getReturnType();

  return createImageSample(
      retType, imageType, image, sampler, coordinate,
      /*compareVal*/ nullptr, bias, lod, std::make_pair(nullptr, nullptr),
      constOffset, varOffset,
      /*constOffsets*/ nullptr, /*sampleNumber*/ nullptr,
      /*minLod*/ clamp, status, expr->getCallee()->getLocStart());
}

SpirvInstruction *
SpirvEmitter::processTextureSampleGrad(const CXXMemberCallExpr *expr) {
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
  auto *status = hasStatusArg ? doExpr(expr->getArg(numArgs - 1)) : nullptr;

  SpirvInstruction *clamp = nullptr;
  if (numArgs > 4 && expr->getArg(4)->getType()->isFloatingType())
    clamp = doExpr(expr->getArg(4));
  else if (numArgs > 5 && expr->getArg(5)->getType()->isFloatingType())
    clamp = doExpr(expr->getArg(5));
  const bool hasClampArg = clamp != nullptr;

  // Subtract 1 for clamp (if it exists), 1 for status (if it exists),
  // and 4 for sampler_state, location, DDX, and DDY;
  const bool hasOffsetArg = numArgs - hasClampArg - hasStatusArg - 4 > 0;

  const auto *imageExpr = expr->getImplicitObjectArgument();
  const QualType imageType = imageExpr->getType();
  auto *image = loadIfGLValue(imageExpr);
  auto *sampler = doExpr(expr->getArg(0));
  auto *coordinate = doExpr(expr->getArg(1));
  auto *ddx = doExpr(expr->getArg(2));
  auto *ddy = doExpr(expr->getArg(3));
  // If offset is present in .SampleGrad(), it is the fifth argument.
  SpirvInstruction *constOffset = nullptr, *varOffset = nullptr;
  if (hasOffsetArg)
    handleOffsetInMethodCall(expr, 4, &constOffset, &varOffset);

  const auto retType = expr->getDirectCallee()->getReturnType();
  return createImageSample(
      retType, imageType, image, sampler, coordinate,
      /*compareVal*/ nullptr, /*bias*/ nullptr,
      /*lod*/ nullptr, std::make_pair(ddx, ddy), constOffset, varOffset,
      /*constOffsets*/ nullptr, /*sampleNumber*/ nullptr,
      /*minLod*/ clamp, status, expr->getCallee()->getLocStart());
}

SpirvInstruction *
SpirvEmitter::processTextureSampleCmpCmpLevelZero(const CXXMemberCallExpr *expr,
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
  auto *status = hasStatusArg ? doExpr(expr->getArg(numArgs - 1)) : nullptr;

  SpirvInstruction *clamp = nullptr;
  // The .SampleCmpLevelZero() methods do not take the clamp argument.
  if (isCmp) {
    if (numArgs > 3 && expr->getArg(3)->getType()->isFloatingType())
      clamp = doExpr(expr->getArg(3));
    else if (numArgs > 4 && expr->getArg(4)->getType()->isFloatingType())
      clamp = doExpr(expr->getArg(4));
  }
  const bool hasClampArg = clamp != nullptr;

  // Subtract 1 for clamp (if it exists), 1 for status (if it exists),
  // and 3 for sampler_state, location, and compare_value.
  const bool hasOffsetArg = numArgs - hasClampArg - hasStatusArg - 3 > 0;

  const auto *imageExpr = expr->getImplicitObjectArgument();
  auto *image = loadIfGLValue(imageExpr);
  auto *sampler = doExpr(expr->getArg(0));
  auto *coordinate = doExpr(expr->getArg(1));
  auto *compareVal = doExpr(expr->getArg(2));
  // If offset is present in .SampleCmp(), it will be the fourth argument.
  SpirvInstruction *constOffset = nullptr, *varOffset = nullptr;
  if (hasOffsetArg)
    handleOffsetInMethodCall(expr, 3, &constOffset, &varOffset);
  auto *lod = isCmp ? nullptr
                    : spvBuilder.getConstantFloat(astContext.FloatTy,
                                                  llvm::APFloat(0.0f));

  const auto retType = expr->getDirectCallee()->getReturnType();
  const auto imageType = imageExpr->getType();

  return createImageSample(
      retType, imageType, image, sampler, coordinate, compareVal,
      /*bias*/ nullptr, lod, std::make_pair(nullptr, nullptr), constOffset,
      varOffset,
      /*constOffsets*/ nullptr, /*sampleNumber*/ nullptr, /*minLod*/ clamp,
      status, expr->getCallee()->getLocStart());
}

SpirvInstruction *
SpirvEmitter::processBufferTextureLoad(const CXXMemberCallExpr *expr) {
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

  if (isRWByteAddressBuffer(objectType) || isByteAddressBuffer(objectType))
    return processByteAddressBufferLoadStore(expr, 1, /*doStore*/ false);

  if (isStructuredBuffer(objectType))
    return processStructuredBufferLoad(expr);

  const auto numArgs = expr->getNumArgs();
  const auto *locationArg = expr->getArg(0);
  const bool textureMS = isTextureMS(objectType);
  const bool hasStatusArg =
      expr->getArg(numArgs - 1)->getType()->isUnsignedIntegerType();
  auto *status = hasStatusArg ? doExpr(expr->getArg(numArgs - 1)) : nullptr;

  auto loc = expr->getExprLoc();
  if (isBuffer(objectType) || isRWBuffer(objectType) || isRWTexture(objectType))
    return processBufferTextureLoad(object, doExpr(locationArg),
                                    /*constOffset*/ nullptr,
                                    /*varOffset*/ nullptr, /*lod*/ nullptr,
                                    /*residencyCode*/ status, loc);

  // Subtract 1 for status (if it exists), and 1 for sampleIndex (if it exists),
  // and 1 for location.
  const bool hasOffsetArg = numArgs - hasStatusArg - textureMS - 1 > 0;

  if (isTexture(objectType)) {
    // .Load() has a second optional paramter for offset.
    SpirvInstruction *location = doExpr(locationArg);
    SpirvInstruction *constOffset = nullptr, *varOffset = nullptr;
    SpirvInstruction *coordinate = location, *lod = nullptr;

    if (textureMS) {
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
      splitVecLastElement(locationArg->getType(), location, &coordinate, &lod,
                          locationArg->getExprLoc());
      // For textures other than Texture2DMS(Array), offset should be the
      // second parameter (index 1).
      if (hasOffsetArg)
        handleOffsetInMethodCall(expr, 1, &constOffset, &varOffset);
    }

    return processBufferTextureLoad(object, coordinate, constOffset, varOffset,
                                    lod, status, loc);
  }
  emitError("Load() of the given object type unimplemented",
            object->getExprLoc());
  return nullptr;
}

SpirvInstruction *
SpirvEmitter::processGetDimensions(const CXXMemberCallExpr *expr) {
  const auto objectType = expr->getImplicitObjectArgument()->getType();
  if (isTexture(objectType) || isRWTexture(objectType) ||
      isBuffer(objectType) || isRWBuffer(objectType)) {
    return processBufferTextureGetDimensions(expr);
  } else if (isByteAddressBuffer(objectType) ||
             isRWByteAddressBuffer(objectType) ||
             isStructuredBuffer(objectType) ||
             isAppendStructuredBuffer(objectType) ||
             isConsumeStructuredBuffer(objectType)) {
    return processByteAddressBufferStructuredBufferGetDimensions(expr);
  } else {
    emitError("GetDimensions() of the given object type unimplemented",
              expr->getExprLoc());
    return nullptr;
  }
}

SpirvInstruction *
SpirvEmitter::doCXXOperatorCallExpr(const CXXOperatorCallExpr *expr) {
  { // Handle Buffer/RWBuffer/Texture/RWTexture indexing
    const Expr *baseExpr = nullptr;
    const Expr *indexExpr = nullptr;
    const Expr *lodExpr = nullptr;

    // For Textures, regular indexing (operator[]) uses slice 0.
    if (isBufferTextureIndexing(expr, &baseExpr, &indexExpr)) {
      auto *lod = isTexture(baseExpr->getType())
                      ? spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                                  llvm::APInt(32, 0))
                      : nullptr;
      return processBufferTextureLoad(baseExpr, doExpr(indexExpr),
                                      /*constOffset*/ nullptr,
                                      /*varOffset*/ nullptr, lod,
                                      /*residencyCode*/ nullptr,
                                      expr->getExprLoc());
    }
    // .mips[][] or .sample[][] must use the correct slice.
    if (isTextureMipsSampleIndexing(expr, &baseExpr, &indexExpr, &lodExpr)) {
      auto *lod = doExpr(lodExpr);
      return processBufferTextureLoad(baseExpr, doExpr(indexExpr),
                                      /*constOffset*/ nullptr,
                                      /*varOffset*/ nullptr, lod,
                                      /*residencyCode*/ nullptr,
                                      expr->getExprLoc());
    }
  }

  llvm::SmallVector<SpirvInstruction *, 4> indices;
  const Expr *baseExpr = collectArrayStructIndices(
      expr, /*rawIndex*/ false, /*rawIndices*/ nullptr, &indices);

  auto base = loadIfAliasVarRef(baseExpr);

  if (indices.empty())
    return base; // For indexing into size-1 vectors and 1xN matrices

  // If we are indexing into a rvalue, to use OpAccessChain, we first need
  // to create a local variable to hold the rvalue.
  //
  // TODO: We can optimize the codegen by emitting OpCompositeExtract if
  // all indices are contant integers.
  if (base->isRValue()) {
    base = createTemporaryVar(baseExpr->getType(), "vector", base,
                              baseExpr->getExprLoc());
  }

  return turnIntoElementPtr(baseExpr->getType(), base, expr->getType(), indices,
                            baseExpr->getExprLoc());
}

SpirvInstruction *
SpirvEmitter::doExtMatrixElementExpr(const ExtMatrixElementExpr *expr) {
  const Expr *baseExpr = expr->getBase();
  auto *baseInfo = doExpr(baseExpr);
  const auto layoutRule = baseInfo->getLayoutRule();
  const auto elemType = hlsl::GetHLSLMatElementType(baseExpr->getType());
  const auto accessor = expr->getEncodedElementAccess();

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(baseExpr->getType(), rowCount, colCount);

  // Construct a temporary vector out of all elements accessed:
  // 1. Create access chain for each element using OpAccessChain
  // 2. Load each element using OpLoad
  // 3. Create the vector using OpCompositeConstruct

  llvm::SmallVector<SpirvInstruction *, 4> elements;
  for (uint32_t i = 0; i < accessor.Count; ++i) {
    uint32_t row = 0, col = 0;
    SpirvInstruction *elem = nullptr;
    accessor.GetPosition(i, &row, &col);

    llvm::SmallVector<uint32_t, 2> indices;
    // If the matrix only has one row/column, we are indexing into a vector
    // then. Only one index is needed for such cases.
    if (rowCount > 1)
      indices.push_back(row);
    if (colCount > 1)
      indices.push_back(col);

    if (!baseInfo->isRValue()) {
      llvm::SmallVector<SpirvInstruction *, 2> indexInstructions(indices.size(),
                                                                 nullptr);
      for (uint32_t i = 0; i < indices.size(); ++i)
        indexInstructions[i] = spvBuilder.getConstantInt(
            astContext.IntTy, llvm::APInt(32, indices[i], true));

      if (!indices.empty()) {
        assert(!baseInfo->isRValue());
        // Load the element via access chain
        elem = spvBuilder.createAccessChain(
            elemType, baseInfo, indexInstructions, baseExpr->getLocStart());
      } else {
        // The matrix is of size 1x1. No need to use access chain, base should
        // be the source pointer.
        elem = baseInfo;
      }
      elem = spvBuilder.createLoad(elemType, elem, baseExpr->getLocStart());
    } else { // e.g., (mat1 + mat2)._m11
      elem = spvBuilder.createCompositeExtract(elemType, baseInfo, indices,
                                               baseExpr->getLocStart());
    }
    elements.push_back(elem);
  }

  const auto size = elements.size();
  auto *value = elements.front();
  if (size > 1) {
    value = spvBuilder.createCompositeConstruct(
        astContext.getExtVectorType(elemType, size), elements,
        expr->getLocStart());
  }

  // Note: Special-case: Booleans have no physical layout, and therefore when
  // layout is required booleans are represented as unsigned integers.
  // Therefore, after loading the uint we should convert it boolean.
  if (elemType->isBooleanType() && layoutRule != SpirvLayoutRule::Void) {
    const auto fromType =
        size == 1 ? astContext.UnsignedIntTy
                  : astContext.getExtVectorType(astContext.UnsignedIntTy, size);
    const auto toType =
        size == 1 ? astContext.BoolTy
                  : astContext.getExtVectorType(astContext.BoolTy, size);
    value = castToBool(value, fromType, toType, expr->getLocStart());
  }
  value->setRValue();
  return value;
}

SpirvInstruction *
SpirvEmitter::doHLSLVectorElementExpr(const HLSLVectorElementExpr *expr) {
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
    auto *baseInfo = doExpr(baseExpr);

    if (!baseInfo || baseSize == 1) {
      // Selecting one element from a size-1 vector. The underlying vector is
      // already treated as a scalar.
      return baseInfo;
    }

    // If the base is an lvalue, we should emit an access chain instruction
    // so that we can load/store the specified element. For rvalue base,
    // we should use composite extraction. We should check the immediate base
    // instead of the original base here since we can have something like
    // v.xyyz to turn a lvalue v into rvalue.
    const auto type = expr->getType();

    if (!baseInfo->isRValue()) { // E.g., v.x;
      auto *index = spvBuilder.getConstantInt(
          astContext.IntTy, llvm::APInt(32, accessor.Swz0, true));
      // We need a lvalue here. Do not try to load.
      return spvBuilder.createAccessChain(type, baseInfo, {index},
                                          baseExpr->getLocStart());
    } else { // E.g., (v + w).x;
      // The original base vector may not be a rvalue. Need to load it if
      // it is lvalue since ImplicitCastExpr (LValueToRValue) will be missing
      // for that case.
      SpirvInstruction *result = spvBuilder.createCompositeExtract(
          type, baseInfo, {accessor.Swz0}, baseExpr->getLocStart());
      // Special-case: Booleans in SPIR-V do not have a physical layout. Uint is
      // used to represent them when layout is required.
      if (expr->getType()->isBooleanType() &&
          baseInfo->getLayoutRule() != SpirvLayoutRule::Void)
        result = castToBool(result, astContext.UnsignedIntTy, astContext.BoolTy,
                            expr->getLocStart());
      return result;
    }
  }

  if (baseSize == 1) {
    // Selecting more than one element from a size-1 vector, for example,
    // <scalar>.xx. Construct the vector.
    auto *info = loadIfGLValue(baseExpr);
    const auto type = expr->getType();
    llvm::SmallVector<SpirvInstruction *, 4> components(accessorSize, info);
    info = spvBuilder.createCompositeConstruct(type, components,
                                               expr->getLocStart());
    info->setRValue();
    return info;
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

  auto *info = loadIfGLValue(baseExpr);
  // Use base for both vectors. But we are only selecting values from the
  // first one.
  return spvBuilder.createVectorShuffle(expr->getType(), info, info, selectors,
                                        expr->getLocStart());
}

SpirvInstruction *SpirvEmitter::doInitListExpr(const InitListExpr *expr) {
  if (auto *id = tryToEvaluateAsConst(expr)) {
    id->setRValue();
    return id;
  }

  auto *result = InitListHandler(astContext, *this).processInit(expr);
  result->setRValue();
  return result;
}

SpirvInstruction *SpirvEmitter::doMemberExpr(const MemberExpr *expr) {
  llvm::SmallVector<SpirvInstruction *, 4> indices;
  const Expr *base = collectArrayStructIndices(
      expr, /*rawIndex*/ false, /*rawIndices*/ nullptr, &indices);
  auto *instr = loadIfAliasVarRef(base);

  if (instr && !indices.empty()) {
    instr = turnIntoElementPtr(base->getType(), instr, expr->getType(), indices,
                               base->getExprLoc());
  }

  return instr;
}

SpirvVariable *SpirvEmitter::createTemporaryVar(QualType type,
                                                llvm::StringRef name,
                                                SpirvInstruction *init,
                                                SourceLocation loc) {
  // We are creating a temporary variable in the Function storage class here,
  // which means it has void layout rule.
  const std::string varName = "temp.var." + name.str();
  auto *var = spvBuilder.addFnVar(type, loc, varName);
  storeValue(var, init, type, loc);
  return var;
}

SpirvInstruction *SpirvEmitter::doUnaryOperator(const UnaryOperator *expr) {
  const auto opcode = expr->getOpcode();
  const auto *subExpr = expr->getSubExpr();
  const auto subType = subExpr->getType();
  auto *subValue = doExpr(subExpr);

  switch (opcode) {
  case UO_PreInc:
  case UO_PreDec:
  case UO_PostInc:
  case UO_PostDec: {
    const bool isPre = opcode == UO_PreInc || opcode == UO_PreDec;
    const bool isInc = opcode == UO_PreInc || opcode == UO_PostInc;

    const spv::Op spvOp = translateOp(isInc ? BO_Add : BO_Sub, subType);
    SpirvInstruction *originValue =
        subValue->isRValue()
            ? subValue
            : spvBuilder.createLoad(subType, subValue, subExpr->getLocStart());
    auto *one = hlsl::IsHLSLMatType(subType) ? getMatElemValueOne(subType)
                                             : getValueOne(subType);

    SpirvInstruction *incValue = nullptr;
    if (isMxNMatrix(subType)) {
      // For matrices, we can only increment/decrement each vector of it.
      const auto actOnEachVec = [this, spvOp, one,
                                 expr](uint32_t /*index*/, QualType vecType,
                                       SpirvInstruction *lhsVec) {
        auto *val = spvBuilder.createBinaryOp(spvOp, vecType, lhsVec, one,
                                              expr->getOperatorLoc());
        val->setRValue();
        return val;
      };
      incValue = processEachVectorInMatrix(subExpr, originValue, actOnEachVec,
                                           expr->getLocStart());
    } else {
      incValue = spvBuilder.createBinaryOp(spvOp, subType, originValue, one,
                                           expr->getOperatorLoc());
    }

    // If this is a RWBuffer/RWTexture assignment, OpImageWrite will be used.
    // Otherwise, store using OpStore.
    if (tryToAssignToRWBufferRWTexture(subExpr, incValue)) {
      incValue->setRValue();
      subValue = incValue;
    } else {
      spvBuilder.createStore(subValue, incValue, subExpr->getLocStart());
    }

    // Prefix increment/decrement operator returns a lvalue, while postfix
    // increment/decrement returns a rvalue.
    if (isPre) {
      return subValue;
    } else {
      originValue->setRValue();
      return originValue;
    }
  }
  case UO_Not: {
    subValue = spvBuilder.createUnaryOp(spv::Op::OpNot, subType, subValue,
                                        expr->getOperatorLoc());
    subValue->setRValue();
    return subValue;
  }
  case UO_LNot: {
    // Parsing will do the necessary casting to make sure we are applying the
    // ! operator on boolean values.
    subValue = spvBuilder.createUnaryOp(spv::Op::OpLogicalNot, subType,
                                        subValue, expr->getOperatorLoc());
    subValue->setRValue();
    return subValue;
  }
  case UO_Plus:
    // No need to do anything for the prefix + operator.
    return subValue;
  case UO_Minus: {
    // SPIR-V have two opcodes for negating values: OpSNegate and OpFNegate.
    const spv::Op spvOp = isFloatOrVecOfFloatType(subType) ? spv::Op::OpFNegate
                                                           : spv::Op::OpSNegate;
    subValue = spvBuilder.createUnaryOp(spvOp, subType, subValue,
                                        expr->getOperatorLoc());
    subValue->setRValue();
    return subValue;
  }
  default:
    break;
  }

  emitError("unary operator '%0' unimplemented", expr->getExprLoc())
      << expr->getOpcodeStr(opcode);
  expr->dump();
  return 0;
}

spv::Op SpirvEmitter::translateOp(BinaryOperator::Opcode op, QualType type) {
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

SpirvInstruction *
SpirvEmitter::processAssignment(const Expr *lhs, SpirvInstruction *rhs,
                                const bool isCompoundAssignment,
                                SpirvInstruction *lhsPtr) {
  lhs = lhs->IgnoreParenNoopCasts(astContext);

  // Assigning to vector swizzling should be handled differently.
  if (SpirvInstruction *result = tryToAssignToVectorElements(lhs, rhs))
    return result;

  // Assigning to matrix swizzling should be handled differently.
  if (SpirvInstruction *result = tryToAssignToMatrixElements(lhs, rhs))
    return result;

  // Assigning to a RWBuffer/RWTexture should be handled differently.
  if (SpirvInstruction *result = tryToAssignToRWBufferRWTexture(lhs, rhs))
    return result;

  // Assigning to a out attribute or indices object in mesh shader should be
  // handled differently.
  if (SpirvInstruction *result = tryToAssignToMSOutAttrsOrIndices(lhs, rhs))
    return result;

  // Normal assignment procedure

  if (!lhsPtr)
    lhsPtr = doExpr(lhs);

  storeValue(lhsPtr, rhs, lhs->getType(), lhs->getLocStart());

  // Plain assignment returns a rvalue, while compound assignment returns
  // lvalue.
  return isCompoundAssignment ? lhsPtr : rhs;
}

void SpirvEmitter::storeValue(SpirvInstruction *lhsPtr,
                              SpirvInstruction *rhsVal, QualType lhsValType,
                              SourceLocation loc) {
  // Defend against nullptr source or destination so errors can bubble up to the
  // user.
  if (!lhsPtr || !rhsVal)
    return;

  if (const auto *refType = lhsValType->getAs<ReferenceType>())
    lhsValType = refType->getPointeeType();

  QualType matElemType = {};
  const bool lhsIsMat = isMxNMatrix(lhsValType, &matElemType);
  const bool lhsIsFloatMat = lhsIsMat && matElemType->isFloatingType();
  const bool lhsIsNonFpMat = lhsIsMat && !matElemType->isFloatingType();

  if (isScalarType(lhsValType) || isVectorType(lhsValType) || lhsIsFloatMat) {
    // Special-case: According to the SPIR-V Spec: There is no physical size
    // or bit pattern defined for boolean type. Therefore an unsigned integer
    // is used to represent booleans when layout is required. In such cases,
    // we should cast the boolean to uint before creating OpStore.
    if (isBoolOrVecOfBoolType(lhsValType) &&
        lhsPtr->getLayoutRule() != SpirvLayoutRule::Void) {
      uint32_t vecSize = 1;
      const bool isVec = isVectorType(lhsValType, nullptr, &vecSize);
      const auto toType =
          isVec ? astContext.getExtVectorType(astContext.UnsignedIntTy, vecSize)
                : astContext.UnsignedIntTy;
      const auto fromType =
          isVec ? astContext.getExtVectorType(astContext.BoolTy, vecSize)
                : astContext.BoolTy;
      rhsVal = castToInt(rhsVal, fromType, toType, {});
    }

    spvBuilder.createStore(lhsPtr, rhsVal, loc);
  } else if (isOpaqueType(lhsValType)) {
    // Resource types are represented using RecordType in the AST.
    // Handle them before the general RecordType.
    //
    // HLSL allows to put resource types that translating into SPIR-V opaque
    // types in structs, or assign to variables of resource types. These can all
    // result in illegal SPIR-V for Vulkan. We just translate here literally and
    // let SPIRV-Tools opt to do the legalization work.
    //
    // Note: legalization specific code
    spvBuilder.createStore(lhsPtr, rhsVal, loc);
    needsLegalization = true;
  } else if (isAKindOfStructuredOrByteBuffer(lhsValType)) {
    // The rhs should be a pointer and the lhs should be a pointer-to-pointer.
    // Directly store the pointer here and let SPIRV-Tools opt to do the clean
    // up.
    //
    // Note: legalization specific code
    spvBuilder.createStore(lhsPtr, rhsVal, loc);
    needsLegalization = true;

    // For ConstantBuffers/TextureBuffers, we decompose and assign each field
    // recursively like normal structs using the following logic.
    //
    // The frontend forbids declaring ConstantBuffer<T> or TextureBuffer<T>
    // variables as function parameters/returns/variables, but happily accepts
    // assignments/returns from ConstantBuffer<T>/TextureBuffer<T> to function
    // parameters/returns/variables of type T. And ConstantBuffer<T> is not
    // represented differently as struct T.
  } else if (isOpaqueArrayType(lhsValType)) {
    // For opaque array types, we cannot perform OpLoad on the whole array and
    // then write out as a whole; instead, we need to OpLoad each element
    // using access chains. This is to influence later SPIR-V transformations
    // to use access chains to access each opaque object; if we do array
    // wholesale handling here, they will be in the final transformed code.
    // Drivers don't like that.
    // TODO: consider moving this hack into SPIRV-Tools as a transformation.
    assert(lhsValType->isConstantArrayType());
    assert(!rhsVal->isRValue());

    const auto *arrayType = astContext.getAsConstantArrayType(lhsValType);
    const auto elemType = arrayType->getElementType();
    const auto arraySize =
        static_cast<uint32_t>(arrayType->getSize().getZExtValue());

    // Do separate load of each element via access chain
    llvm::SmallVector<SpirvInstruction *, 8> elements;
    for (uint32_t i = 0; i < arraySize; ++i) {
      auto *subRhsPtr = spvBuilder.createAccessChain(
          elemType, rhsVal,
          {spvBuilder.getConstantInt(astContext.IntTy,
                                     llvm::APInt(32, i, true))},
          loc);
      elements.push_back(spvBuilder.createLoad(elemType, subRhsPtr, loc));
    }

    // Create a new composite and write out once
    spvBuilder.createStore(
        lhsPtr,
        spvBuilder.createCompositeConstruct(lhsValType, elements,
                                            rhsVal->getSourceLocation()),
        loc);
  } else if (lhsPtr->getLayoutRule() == rhsVal->getLayoutRule()) {
    // If lhs and rhs has the same memory layout, we should be safe to load
    // from rhs and directly store into lhs and avoid decomposing rhs.
    // Note: this check should happen after those setting needsLegalization.
    // TODO: is this optimization always correct?
    spvBuilder.createStore(lhsPtr, rhsVal, loc);
  } else if (lhsValType->isRecordType() || lhsValType->isConstantArrayType() ||
             lhsIsNonFpMat) {
    spvBuilder.createStore(
        lhsPtr,
        reconstructValue(rhsVal, lhsValType, lhsPtr->getLayoutRule(), loc),
        loc);
  } else {
    emitError("storing value of type %0 unimplemented", {}) << lhsValType;
  }
}

SpirvInstruction *SpirvEmitter::reconstructValue(SpirvInstruction *srcVal,
                                                 const QualType valType,
                                                 SpirvLayoutRule dstLR,
                                                 SourceLocation loc) {
  // Lambda for casting scalar or vector of bool<-->uint in cases where one side
  // of the reconstruction (lhs or rhs) has a layout rule.
  const auto handleBooleanLayout = [this, &srcVal, dstLR,
                                    loc](SpirvInstruction *val,
                                         QualType valType) {
    // We only need to cast if we have a scalar or vector of booleans.
    if (!isBoolOrVecOfBoolType(valType))
      return val;

    SpirvLayoutRule srcLR = srcVal->getLayoutRule();
    // Source value has a layout rule, and has therefore been represented
    // as a uint. Cast it to boolean before using.
    bool shouldCastToBool =
        srcLR != SpirvLayoutRule::Void && dstLR == SpirvLayoutRule::Void;
    // Destination has a layout rule, and should therefore be represented
    // as a uint. Cast to uint before using.
    bool shouldCastToUint =
        srcLR == SpirvLayoutRule::Void && dstLR != SpirvLayoutRule::Void;
    // No boolean layout issues to take care of.
    if (!shouldCastToBool && !shouldCastToUint)
      return val;

    uint32_t vecSize = 1;
    isVectorType(valType, nullptr, &vecSize);
    QualType boolType =
        vecSize == 1 ? astContext.BoolTy
                     : astContext.getExtVectorType(astContext.BoolTy, vecSize);
    QualType uintType =
        vecSize == 1
            ? astContext.UnsignedIntTy
            : astContext.getExtVectorType(astContext.UnsignedIntTy, vecSize);

    if (shouldCastToBool)
      return castToBool(val, uintType, boolType, loc);
    if (shouldCastToUint)
      return castToInt(val, boolType, uintType, loc);

    return val;
  };

  // Lambda for cases where we want to reconstruct an array
  const auto reconstructArray = [this, &srcVal, valType, dstLR,
                                 loc](uint32_t arraySize,
                                      QualType arrayElemType) {
    llvm::SmallVector<SpirvInstruction *, 4> elements;
    for (uint32_t i = 0; i < arraySize; ++i) {
      SpirvInstruction *subSrcVal =
          spvBuilder.createCompositeExtract(arrayElemType, srcVal, {i}, loc);
      subSrcVal->setLayoutRule(srcVal->getLayoutRule());
      elements.push_back(
          reconstructValue(subSrcVal, arrayElemType, dstLR, loc));
    }
    auto *result = spvBuilder.createCompositeConstruct(
        valType, elements, srcVal->getSourceLocation());
    result->setLayoutRule(dstLR);
    return result;
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
      isMxNMatrix(valType, &matElemType, &numRows, &numCols) &&
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
    llvm::SmallVector<SpirvInstruction *, 4> elements;
    for (const auto *field : recordType->getDecl()->fields()) {
      SpirvInstruction *subSrcVal = spvBuilder.createCompositeExtract(
          field->getType(), srcVal, {index}, loc);
      subSrcVal->setLayoutRule(srcVal->getLayoutRule());
      elements.push_back(
          reconstructValue(subSrcVal, field->getType(), dstLR, loc));
      ++index;
    }
    auto *result = spvBuilder.createCompositeConstruct(
        valType, elements, srcVal->getSourceLocation());
    result->setLayoutRule(dstLR);
    return result;
  }

  return handleBooleanLayout(srcVal, valType);
}

SpirvInstruction *SpirvEmitter::processBinaryOp(
    const Expr *lhs, const Expr *rhs, const BinaryOperatorKind opcode,
    const QualType computationType, const QualType resultType,
    SourceRange sourceRange, SourceLocation loc, SpirvInstruction **lhsInfo,
    const spv::Op mandateGenOpcode) {
  const QualType lhsType = lhs->getType();
  const QualType rhsType = rhs->getType();

  // If the operands are of matrix type, we need to dispatch the operation
  // onto each element vector iff the operands are not degenerated matrices
  // and we don't have a matrix specific SPIR-V instruction for the operation.
  if (!isSpirvMatrixOp(mandateGenOpcode) && isMxNMatrix(lhsType)) {
    return processMatrixBinaryOp(lhs, rhs, opcode, sourceRange, loc);
  }

  // Comma operator works differently from other binary operations as there is
  // no SPIR-V instruction for it. For each comma, we must evaluate lhs and rhs
  // respectively, and return the results of rhs.
  if (opcode == BO_Comma) {
    (void)doExpr(lhs);
    return doExpr(rhs);
  }

  SpirvInstruction *rhsVal = nullptr, *lhsPtr = nullptr, *lhsVal = nullptr;

  if (BinaryOperator::isCompoundAssignmentOp(opcode)) {
    // Evalute rhs before lhs
    rhsVal = loadIfGLValue(rhs);
    lhsVal = lhsPtr = doExpr(lhs);
    // This is a compound assignment. We need to load the lhs value if lhs
    // is not already rvalue and does not generate a vector shuffle.
    if (!lhsPtr->isRValue() && !isVectorShuffle(lhs)) {
      lhsVal = loadIfGLValue(lhs, lhsPtr);
    }
    // For a compound assignments, the AST does not have the proper implicit
    // cast if lhs and rhs have different types. So we need to manually cast lhs
    // to the computation type.
    if (computationType != lhsType)
      lhsVal = castToType(lhsVal, lhsType, computationType, lhs->getExprLoc());
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
    rhsVal = spvBuilder.createBinaryOp(spv::Op::OpBitwiseAnd, computationType,
                                       rhsVal, getMaskForBitwidthValue(rhsType),
                                       loc);
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
    if (SpirvConstant *lhsValConstant = dyn_cast<SpirvConstant>(lhsVal)) {
      if (SpirvConstant *rhsValConstant = dyn_cast<SpirvConstant>(rhsVal)) {
        if (isAcceptedSpecConstantBinaryOp(spvOp)) {
          if (lhsValConstant->isSpecConstant() ||
              rhsValConstant->isSpecConstant()) {
            auto *val = spvBuilder.createSpecConstantBinaryOp(
                spvOp, resultType, lhsVal, rhsVal, loc);
            val->setRValue();
            return val;
          }
        }
      }
    }

    // Normal binary operation
    SpirvInstruction *val = nullptr;
    if (BinaryOperator::isCompoundAssignmentOp(opcode)) {
      val = spvBuilder.createBinaryOp(spvOp, computationType, lhsVal, rhsVal,
                                      loc);
      // For a compound assignments, the AST does not have the proper implicit
      // cast if lhs and rhs have different types. So we need to manually cast
      // the result back to lhs' type.
      if (computationType != lhsType)
        val = castToType(val, computationType, lhsType, lhs->getExprLoc());
    } else {
      val = spvBuilder.createBinaryOp(spvOp, resultType, lhsVal, rhsVal, loc);
    }

    val->setRValue();

    // Propagate RelaxedPrecision
    if (lhsVal->isRelaxedPrecision() || rhsVal->isRelaxedPrecision())
      val->setRelaxedPrecision();

    return val;
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
  return nullptr;
}

void SpirvEmitter::initOnce(QualType varType, std::string varName,
                            SpirvVariable *var, const Expr *varInit) {
  // For uninitialized resource objects, we do nothing since there is no
  // meaningful zero values for them.
  if (!varInit && hlsl::IsHLSLResourceType(varType))
    return;

  varName = "init.done." + varName;

  auto loc = varInit ? varInit->getLocStart() : SourceLocation();

  // Create a file/module visible variable to hold the initialization state.
  SpirvVariable *initDoneVar = spvBuilder.addModuleVar(
      astContext.BoolTy, spv::StorageClass::Private, /*isPrecise*/ false,
      varName, spvBuilder.getConstantBool(false));

  auto *condition = spvBuilder.createLoad(astContext.BoolTy, initDoneVar, loc);

  auto *todoBB = spvBuilder.createBasicBlock("if.init.todo");
  auto *doneBB = spvBuilder.createBasicBlock("if.init.done");

  // If initDoneVar contains true, we jump to the "done" basic block; otherwise,
  // jump to the "todo" basic block.
  spvBuilder.createConditionalBranch(condition, doneBB, todoBB, loc, doneBB);
  spvBuilder.addSuccessor(todoBB);
  spvBuilder.addSuccessor(doneBB);
  spvBuilder.setMergeTarget(doneBB);

  spvBuilder.setInsertPoint(todoBB);
  // Do initialization and mark done
  if (varInit) {
    var->setStorageClass(spv::StorageClass::Private);
    storeValue(
        // Static function variable are of private storage class
        var, loadIfGLValue(varInit), varInit->getType(), varInit->getLocEnd());
  } else {
    spvBuilder.createStore(var, spvBuilder.getConstantNull(varType), loc);
  }
  spvBuilder.createStore(initDoneVar, spvBuilder.getConstantBool(true), loc);
  spvBuilder.createBranch(doneBB, loc);
  spvBuilder.addSuccessor(doneBB);

  spvBuilder.setInsertPoint(doneBB);
}

bool SpirvEmitter::isVectorShuffle(const Expr *expr) {
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

bool SpirvEmitter::isTextureMipsSampleIndexing(const CXXOperatorCallExpr *expr,
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
  if (!isTexture(objectType))
    return false;

  if (base)
    *base = object;
  if (lod)
    *lod = innerExpr->getArg(1);
  if (location)
    *location = outerExpr->getArg(1);
  return true;
}

bool SpirvEmitter::isBufferTextureIndexing(const CXXOperatorCallExpr *indexExpr,
                                           const Expr **base,
                                           const Expr **index) {
  if (!indexExpr)
    return false;

  // Must be operator[]
  if (indexExpr->getOperator() != OverloadedOperatorKind::OO_Subscript)
    return false;
  const Expr *object = indexExpr->getArg(0);
  const auto objectType = object->getType();
  if (isBuffer(objectType) || isRWBuffer(objectType) || isTexture(objectType) ||
      isRWTexture(objectType)) {
    if (base)
      *base = object;
    if (index)
      *index = indexExpr->getArg(1);
    return true;
  }
  return false;
}

void SpirvEmitter::condenseVectorElementExpr(
    const HLSLVectorElementExpr *expr, const Expr **basePtr,
    hlsl::VectorMemberAccessPositions *flattenedAccessor) {
  llvm::SmallVector<hlsl::VectorMemberAccessPositions, 2> accessors;
  *basePtr = expr;

  // Recursively descending until we find the true base vector (the base vector
  // that does not have a base vector). In the meanwhile, collecting accessors
  // in the reverse order.
  // Example: for myVector.yxwz.yxz.xx.yx, the true base is 'myVector'.
  while (const auto *vecElemBase = dyn_cast<HLSLVectorElementExpr>(*basePtr)) {
    accessors.push_back(vecElemBase->getEncodedElementAccess());
    *basePtr = vecElemBase->getBase();
    // We need to skip any number of parentheses around swizzling at any level.
    while (const auto *parenExpr = dyn_cast<ParenExpr>(*basePtr))
      *basePtr = parenExpr->getSubExpr();
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

SpirvInstruction *SpirvEmitter::createVectorSplat(const Expr *scalarExpr,
                                                  uint32_t size) {
  SpirvInstruction *scalarVal = nullptr;

  // Try to evaluate the element as constant first. If successful, then we
  // can generate constant instructions for this vector splat.
  if ((scalarVal = tryToEvaluateAsConst(scalarExpr))) {
    scalarVal->setRValue();
  } else {
    scalarVal = loadIfGLValue(scalarExpr);
  }

  if (!scalarVal || size == 1) {
    // Just return the scalar value for vector splat with size 1.
    // Note that can be used as an lvalue, so we need to carry over
    // the lvalueness for non-constant cases.
    return scalarVal;
  }

  const auto vecType = astContext.getExtVectorType(scalarExpr->getType(), size);

  // TODO: we are saying the constant has Function storage class here.
  // Should find a more meaningful one.
  if (auto *constVal = dyn_cast<SpirvConstant>(scalarVal)) {
    llvm::SmallVector<SpirvConstant *, 4> elements(size_t(size), constVal);
    auto *value = spvBuilder.getConstantComposite(vecType, elements);
    value->setRValue();
    return value;
  } else {
    llvm::SmallVector<SpirvInstruction *, 4> elements(size_t(size), scalarVal);
    auto *value = spvBuilder.createCompositeConstruct(
        vecType, elements, scalarExpr->getLocStart());
    value->setRValue();
    return value;
  }
}

void SpirvEmitter::splitVecLastElement(QualType vecType, SpirvInstruction *vec,
                                       SpirvInstruction **residual,
                                       SpirvInstruction **lastElement,
                                       SourceLocation loc) {
  assert(hlsl::IsHLSLVecType(vecType));

  const uint32_t count = hlsl::GetHLSLVecSize(vecType);
  assert(count > 1);
  const QualType elemType = hlsl::GetHLSLVecElementType(vecType);

  if (count == 2) {
    *residual = spvBuilder.createCompositeExtract(elemType, vec, 0, loc);
  } else {
    llvm::SmallVector<uint32_t, 4> indices;
    for (uint32_t i = 0; i < count - 1; ++i)
      indices.push_back(i);

    const QualType type = astContext.getExtVectorType(elemType, count - 1);
    *residual = spvBuilder.createVectorShuffle(type, vec, vec, indices, loc);
  }

  *lastElement =
      spvBuilder.createCompositeExtract(elemType, vec, {count - 1}, loc);
}

SpirvInstruction *SpirvEmitter::convertVectorToStruct(QualType structType,
                                                      QualType elemType,
                                                      SpirvInstruction *vector,
                                                      SourceLocation loc) {
  assert(structType->isStructureType());

  const auto *structDecl = structType->getAsStructureType()->getDecl();
  uint32_t vectorIndex = 0;
  uint32_t elemCount = 1;
  llvm::SmallVector<SpirvInstruction *, 4> members;

  for (const auto *field : structDecl->fields()) {
    if (isScalarType(field->getType())) {
      members.push_back(spvBuilder.createCompositeExtract(
          elemType, vector, {vectorIndex++}, loc));
    } else if (isVectorType(field->getType(), nullptr, &elemCount)) {
      llvm::SmallVector<uint32_t, 4> indices;
      for (uint32_t i = 0; i < elemCount; ++i)
        indices.push_back(vectorIndex++);

      members.push_back(spvBuilder.createVectorShuffle(
          astContext.getExtVectorType(elemType, elemCount), vector, vector,
          indices, loc));
    } else {
      assert(false && "unhandled type");
    }
  }

  return spvBuilder.createCompositeConstruct(structType, members,
                                             vector->getSourceLocation());
}

SpirvInstruction *
SpirvEmitter::tryToGenFloatVectorScale(const BinaryOperator *expr) {
  const QualType type = expr->getType();
  const SourceRange range = expr->getSourceRange();
  QualType elemType = {};

  // We can only translate floatN * float into OpVectorTimesScalar.
  // So the result type must be floatN. Note that float1 is not a valid vector
  // in SPIR-V.
  if (!(isVectorType(type, &elemType) && elemType->isFloatingType()))
    return nullptr;

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
          SpirvInstruction *lhsPtr = nullptr;
          auto *result =
              processBinaryOp(lhs, cast->getSubExpr(), expr->getOpcode(),
                              vecType, vecType, range, expr->getOperatorLoc(),
                              &lhsPtr, spv::Op::OpVectorTimesScalar);
          return processAssignment(lhs, result, true, lhsPtr);
        } else {
          return processBinaryOp(lhs, cast->getSubExpr(), expr->getOpcode(),
                                 vecType, vecType, range,
                                 expr->getOperatorLoc(), nullptr,
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
                               vecType, vecType, range, expr->getOperatorLoc(),
                               nullptr, spv::Op::OpVectorTimesScalar);
      }
    }
  }

  return nullptr;
}

SpirvInstruction *
SpirvEmitter::tryToGenFloatMatrixScale(const BinaryOperator *expr) {
  const QualType type = expr->getType();
  const SourceRange range = expr->getSourceRange();

  // We translate 'floatMxN * float' into OpMatrixTimesScalar.
  // We translate 'floatMx1 * float' and 'float1xN * float' using
  // OpVectorTimesScalar.
  // So the result type can be floatMxN, floatMx1, or float1xN.
  if (!hlsl::IsHLSLMatType(type) ||
      !hlsl::GetHLSLMatElementType(type)->isFloatingType() || is1x1Matrix(type))
    return 0;

  const Expr *lhs = expr->getLHS();
  const Expr *rhs = expr->getRHS();
  const QualType lhsType = lhs->getType();
  const QualType rhsType = rhs->getType();

  const auto selectOpcode = [](const QualType ty) {
    return isMx1Matrix(ty) || is1xNMatrix(ty) ? spv::Op::OpVectorTimesScalar
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
          SpirvInstruction *lhsPtr = nullptr;
          auto *result = processBinaryOp(
              lhs, cast->getSubExpr(), expr->getOpcode(), matType, matType,
              range, expr->getOperatorLoc(), &lhsPtr, opcode);
          return processAssignment(lhs, result, true, lhsPtr);
        } else {
          return processBinaryOp(lhs, cast->getSubExpr(), expr->getOpcode(),
                                 matType, matType, range,
                                 expr->getOperatorLoc(), nullptr, opcode);
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
                               matType, matType, range, expr->getOperatorLoc(),
                               nullptr, opcode);
      }
    }
  }

  return nullptr;
}

SpirvInstruction *
SpirvEmitter::tryToAssignToVectorElements(const Expr *lhs,
                                          SpirvInstruction *rhs) {
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
      auto *oldVec = doExpr(base);
      auto *newVec = spvBuilder.createCompositeInsert(
          baseType, oldVec, {accessor.Swz0}, rhs, lhs->getLocStart());
      auto *result = tryToAssignToRWBufferRWTexture(base, newVec);
      assert(result); // Definitely RWBuffer/RWTexture assignment
      (void)result;
      return rhs; // TODO: incorrect for compound assignments
    } else {
      // Assigning to one component of mesh out attribute/indices vector object.
      SpirvInstruction *vecComponent = spvBuilder.getConstantInt(
          astContext.UnsignedIntTy, llvm::APInt(32, accessor.Swz0));
      if (tryToAssignToMSOutAttrsOrIndices(base, rhs, vecComponent))
        return rhs;
      // Assigning to one normal vector component. Nothing special, just fall
      // back to the normal CodeGen path.
      return nullptr;
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

  if (tryToAssignToMSOutAttrsOrIndices(base, rhs, /*vecComponent=*/nullptr,
                                       /*noWriteBack=*/true)) {
    // Assigning to 'n' components of mesh out attribute/indices vector object.
    const QualType elemType =
        hlsl::GetHLSLVecElementType(rhs->getAstResultType());
    uint32_t i = 0;
    for (; i < accessor.Count; ++i) {
      auto *rhsElem = spvBuilder.createCompositeExtract(elemType, rhs, {i},
                                                        lhs->getLocStart());
      uint32_t position;
      accessor.GetPosition(i, &position);
      SpirvInstruction *vecComponent = spvBuilder.getConstantInt(
          astContext.UnsignedIntTy, llvm::APInt(32, position));
      if (!tryToAssignToMSOutAttrsOrIndices(base, rhsElem, vecComponent))
        break;
    }
    assert(i == accessor.Count);
    return rhs;
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

  auto *vec1 = doExpr(base);
  auto *vec1Val = vec1->isRValue() ? vec1
                                   : spvBuilder.createLoad(baseType, vec1,
                                                           base->getLocStart());
  auto *shuffle = spvBuilder.createVectorShuffle(baseType, vec1Val, rhs,
                                                 selectors, lhs->getLocStart());

  if (!tryToAssignToRWBufferRWTexture(base, shuffle))
    spvBuilder.createStore(vec1, shuffle, lhs->getLocStart());

  // TODO: OK, this return value is incorrect for compound assignments, for
  // which cases we should return lvalues. Should at least emit errors if
  // this return value is used (can be checked via ASTContext.getParents).
  return rhs;
}

SpirvInstruction *
SpirvEmitter::tryToAssignToRWBufferRWTexture(const Expr *lhs,
                                             SpirvInstruction *rhs) {
  const Expr *baseExpr = nullptr;
  const Expr *indexExpr = nullptr;
  const auto lhsExpr = dyn_cast<CXXOperatorCallExpr>(lhs);
  if (isBufferTextureIndexing(lhsExpr, &baseExpr, &indexExpr)) {
    auto *loc = doExpr(indexExpr);
    const QualType imageType = baseExpr->getType();
    auto *baseInfo = doExpr(baseExpr);
    auto *image =
        spvBuilder.createLoad(imageType, baseInfo, baseExpr->getExprLoc());
    spvBuilder.createImageWrite(imageType, image, loc, rhs, lhs->getExprLoc());
    return rhs;
  }
  return nullptr;
}

SpirvInstruction *
SpirvEmitter::tryToAssignToMatrixElements(const Expr *lhs,
                                          SpirvInstruction *rhs) {
  const auto *lhsExpr = dyn_cast<ExtMatrixElementExpr>(lhs);
  if (!lhsExpr)
    return nullptr;

  const Expr *baseMat = lhsExpr->getBase();
  auto *base = doExpr(baseMat);
  const QualType elemType = hlsl::GetHLSLMatElementType(baseMat->getType());

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

    llvm::SmallVector<SpirvInstruction *, 2> indexInstructions(indices.size(),
                                                               nullptr);
    for (uint32_t i = 0; i < indices.size(); ++i)
      indexInstructions[i] = spvBuilder.getConstantInt(
          astContext.IntTy, llvm::APInt(32, indices[i], true));

    // If we are writing to only one element, the rhs should already be a
    // scalar value.
    auto *rhsElem = rhs;
    if (accessor.Count > 1) {
      rhsElem = spvBuilder.createCompositeExtract(elemType, rhs, {i},
                                                  rhs->getSourceLocation());
    }

    // If the lhs is actually a matrix of size 1x1, we don't need the access
    // chain. base is already the dest pointer.
    auto *lhsElemPtr = base;
    if (!indexInstructions.empty()) {
      assert(!base->isRValue());
      // Load the element via access chain
      lhsElemPtr = spvBuilder.createAccessChain(
          elemType, lhsElemPtr, indexInstructions, lhs->getLocStart());
    }

    spvBuilder.createStore(lhsElemPtr, rhsElem, lhs->getLocStart());
  }

  // TODO: OK, this return value is incorrect for compound assignments, for
  // which cases we should return lvalues. Should at least emit errors if
  // this return value is used (can be checked via ASTContext.getParents).
  return rhs;
}

SpirvInstruction *SpirvEmitter::tryToAssignToMSOutAttrsOrIndices(
    const Expr *lhs, SpirvInstruction *rhs, SpirvInstruction *vecComponent,
    bool noWriteBack) {
  // Early exit for non-mesh shaders.
  if (!spvContext.isMS())
    return nullptr;

  llvm::SmallVector<SpirvInstruction *, 4> indices;
  bool isMSOutAttribute = false;
  bool isMSOutAttributeBlock = false;
  bool isMSOutIndices = false;

  const Expr *base = collectArrayStructIndices(lhs, /*rawIndex*/ false,
                                               /*rawIndices*/ nullptr, &indices,
                                               &isMSOutAttribute);
  // Expecting at least one array index - early exit.
  if (!base || indices.empty())
    return nullptr;

  const DeclaratorDecl *varDecl = nullptr;
  if (isMSOutAttribute) {
    const MemberExpr *memberExpr = dyn_cast<MemberExpr>(base);
    assert(memberExpr);
    varDecl = cast<DeclaratorDecl>(memberExpr->getMemberDecl());
  } else {
    if (const auto *arg = dyn_cast<DeclRefExpr>(base)) {
      if (varDecl = dyn_cast<DeclaratorDecl>(arg->getDecl())) {
        if (varDecl->hasAttr<HLSLIndicesAttr>()) {
          isMSOutIndices = true;
        } else if (varDecl->hasAttr<HLSLVerticesAttr>() ||
                   varDecl->hasAttr<HLSLPrimitivesAttr>()) {
          isMSOutAttributeBlock = true;
        }
      }
    }
  }

  // Return if no out attribute or indices object found.
  if (!(isMSOutAttribute || isMSOutAttributeBlock || isMSOutIndices)) {
    return nullptr;
  }

  // For noWriteBack, return without generating write instructions.
  if (noWriteBack) {
    return rhs;
  }

  // Add vecComponent to indices.
  if (vecComponent) {
    indices.push_back(vecComponent);
  }

  if (isMSOutAttribute) {
    assignToMSOutAttribute(varDecl, rhs, indices);
  } else if (isMSOutIndices) {
    assignToMSOutIndices(varDecl, rhs, indices);
  } else {
    assert(isMSOutAttributeBlock);
    QualType type = varDecl->getType();
    assert(isa<ConstantArrayType>(type));
    type = astContext.getAsConstantArrayType(type)->getElementType();
    assert(type->isStructureType());

    // Extract subvalue and assign to its corresponding member attribute.
    const auto *structDecl = type->getAs<RecordType>()->getDecl();
    for (const auto *field : structDecl->fields()) {
      const auto fieldType = field->getType();
      SpirvInstruction *subValue = spvBuilder.createCompositeExtract(
          fieldType, rhs, {getNumBaseClasses(type) + field->getFieldIndex()},
          lhs->getLocStart());
      assignToMSOutAttribute(field, subValue, indices);
    }
  }

  // TODO: OK, this return value is incorrect for compound assignments, for
  // which cases we should return lvalues. Should at least emit errors if
  // this return value is used (can be checked via ASTContext.getParents).
  return rhs;
}

void SpirvEmitter::assignToMSOutAttribute(
    const DeclaratorDecl *decl, SpirvInstruction *value,
    const llvm::SmallVector<SpirvInstruction *, 4> &indices) {
  assert(spvContext.isMS() && !indices.empty());

  // Extract attribute index and vecComponent (if any).
  SpirvInstruction *attrIndex = indices.front();
  SpirvInstruction *vecComponent = nullptr;
  if (indices.size() > 1) {
    vecComponent = indices.back();
  }

  auto semanticInfo = declIdMapper.getStageVarSemantic(decl);
  assert(semanticInfo.isValid());
  const auto loc = decl->getLocation();
  // Special handle writes to clip/cull distance attributes.
  if (!declIdMapper.glPerVertex.tryToAccess(
          hlsl::DXIL::SigPointKind::MSOut, semanticInfo.semantic->GetKind(),
          semanticInfo.index, attrIndex, &value, /*noWriteBack=*/false,
          vecComponent, loc)) {
    // All other attribute writes are handled below.
    auto *varInstr = declIdMapper.getStageVarInstruction(decl);
    QualType valueType = value->getAstResultType();
    varInstr = spvBuilder.createAccessChain(valueType, varInstr, indices, loc);
    spvBuilder.createStore(varInstr, value, loc);
  }
}

void SpirvEmitter::assignToMSOutIndices(
    const DeclaratorDecl *decl, SpirvInstruction *value,
    const llvm::SmallVector<SpirvInstruction *, 4> &indices) {
  assert(spvContext.isMS() && !indices.empty());

  // Extract vertex index and vecComponent (if any).
  SpirvInstruction *vertIndex = indices.front();
  SpirvInstruction *vecComponent = nullptr;
  if (indices.size() > 1) {
    vecComponent = indices.back();
  }
  auto *var = declIdMapper.getStageVarInstruction(decl);
  const auto *varTypeDecl = astContext.getAsConstantArrayType(decl->getType());
  QualType varType = varTypeDecl->getElementType();
  uint32_t numVertices = 1;
  if (!isVectorType(varType, nullptr, &numVertices)) {
    assert(isScalarType(varType));
  }
  QualType valueType = value->getAstResultType();
  uint32_t numValues = 1;
  if (!isVectorType(valueType, nullptr, &numValues)) {
    assert(isScalarType(valueType));
  }

  const auto loc = decl->getLocation();
  if (numVertices == 1) {
    // for "point" output topology.
    assert(numValues == 1);
    // create accesschain for PrimitiveIndicesNV[vertIndex].
    auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, var,
                                             {vertIndex}, loc);
    // finally create store for PrimitiveIndicesNV[vertIndex] = value.
    spvBuilder.createStore(ptr, value, loc);
  } else {
    // for "line" or "triangle" output topology.
    assert(numVertices == 2 || numVertices == 3);
    // set baseOffset = vertIndex * numVertices.
    auto *baseOffset = spvBuilder.createBinaryOp(
        spv::Op::OpIMul, astContext.UnsignedIntTy, vertIndex,
        spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                  llvm::APInt(32, numVertices)),
        loc);
    if (vecComponent) {
      // write an individual vector component of uint2 or uint3.
      assert(numValues == 1);
      // set baseOffset = baseOffset + vecComponent.
      baseOffset =
          spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                    baseOffset, vecComponent, loc);
      // create accesschain for PrimitiveIndicesNV[baseOffset].
      auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, var,
                                               {baseOffset}, loc);
      // finally create store for PrimitiveIndicesNV[baseOffset] = value.
      spvBuilder.createStore(ptr, value, loc);
    } else {
      // write all vector components of uint2 or uint3.
      assert(numValues == numVertices);
      auto *curOffset = baseOffset;
      for (uint32_t i = 0; i < numValues; ++i) {
        if (i != 0) {
          // set curOffset = baseOffset + i.
          curOffset = spvBuilder.createBinaryOp(
              spv::Op::OpIAdd, astContext.UnsignedIntTy, baseOffset,
              spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                        llvm::APInt(32, i)),
              loc);
        }
        // create accesschain for PrimitiveIndicesNV[curOffset].
        auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, var,
                                                 {curOffset}, loc);
        // finally create store for PrimitiveIndicesNV[curOffset] = value[i].
        spvBuilder.createStore(ptr,
                               spvBuilder.createCompositeExtract(
                                   astContext.UnsignedIntTy, value, {i}, loc),
                               loc);
      }
    }
  }
}

SpirvInstruction *SpirvEmitter::processEachVectorInMatrix(
    const Expr *matrix, SpirvInstruction *matrixVal,
    llvm::function_ref<SpirvInstruction *(uint32_t, QualType,
                                          SpirvInstruction *)>
        actOnEachVector,
    SourceLocation loc) {
  const auto matType = matrix->getType();
  assert(isMxNMatrix(matType));
  const QualType vecType = getComponentVectorType(astContext, matType);

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(matType, rowCount, colCount);

  llvm::SmallVector<SpirvInstruction *, 4> vectors;
  // Extract each component vector and do operation on it
  for (uint32_t i = 0; i < rowCount; ++i) {
    auto *lhsVec = spvBuilder.createCompositeExtract(vecType, matrixVal, {i},
                                                     matrix->getLocStart());
    vectors.push_back(actOnEachVector(i, vecType, lhsVec));
  }

  // Construct the result matrix
  auto *val = spvBuilder.createCompositeConstruct(matType, vectors, loc);
  val->setRValue();
  return val;
}

void SpirvEmitter::createSpecConstant(const VarDecl *varDecl) {
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

  spvBuilder.decorateSpecId(
      specConstant, varDecl->getAttr<VKConstantIdAttr>()->getSpecConstId(),
      varDecl->getLocation());

  specConstant->setDebugName(varDecl->getName());
  declIdMapper.registerSpecConstant(varDecl, specConstant);
}

SpirvInstruction *
SpirvEmitter::processMatrixBinaryOp(const Expr *lhs, const Expr *rhs,
                                    const BinaryOperatorKind opcode,
                                    SourceRange range, SourceLocation loc) {
  // TODO: some code are duplicated from processBinaryOp. Try to unify them.
  const auto lhsType = lhs->getType();
  assert(isMxNMatrix(lhsType));
  const spv::Op spvOp = translateOp(opcode, lhsType);

  SpirvInstruction *rhsVal = nullptr, *lhsPtr = nullptr, *lhsVal = nullptr;
  if (BinaryOperator::isCompoundAssignmentOp(opcode)) {
    // Evalute rhs before lhs
    rhsVal = doExpr(rhs);
    lhsPtr = doExpr(lhs);
    lhsVal = spvBuilder.createLoad(lhsType, lhsPtr, lhs->getLocStart());
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
    const auto actOnEachVec = [this, spvOp, rhsVal, rhs,
                               loc](uint32_t index, QualType vecType,
                                    SpirvInstruction *lhsVec) {
      // For each vector of lhs, we need to load the corresponding vector of
      // rhs and do the operation on them.
      auto *rhsVec = spvBuilder.createCompositeExtract(vecType, rhsVal, {index},
                                                       rhs->getLocStart());
      auto *val =
          spvBuilder.createBinaryOp(spvOp, vecType, lhsVec, rhsVec, loc);
      val->setRValue();
      return val;
    };
    return processEachVectorInMatrix(lhs, lhsVal, actOnEachVec,
                                     lhs->getLocStart());
  }
  case BO_Assign:
    llvm_unreachable("assignment should not be handled here");
  default:
    break;
  }

  emitError("binary operator '%0' over matrix type unimplemented",
            lhs->getExprLoc())
      << BinaryOperator::getOpcodeStr(opcode) << range;
  return nullptr;
}

const Expr *SpirvEmitter::collectArrayStructIndices(
    const Expr *expr, bool rawIndex,
    llvm::SmallVectorImpl<uint32_t> *rawIndices,
    llvm::SmallVectorImpl<SpirvInstruction *> *indices,
    bool *isMSOutAttribute) {
  assert((rawIndex && rawIndices) || (!rawIndex && indices));

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
        indexing->getBase()->IgnoreParenNoopCasts(astContext), rawIndex,
        rawIndices, indices, isMSOutAttribute);

    if (isMSOutAttribute && base) {
      if (const auto *arg = dyn_cast<DeclRefExpr>(base)) {
        if (const auto *varDecl = dyn_cast<VarDecl>(arg->getDecl())) {
          if (varDecl->hasAttr<HLSLVerticesAttr>() ||
              varDecl->hasAttr<HLSLPrimitivesAttr>()) {
            assert(spvContext.isMS());
            *isMSOutAttribute = true;
            return expr;
          }
        }
      }
    }

    // Append the index of the current level
    const auto *fieldDecl = cast<FieldDecl>(indexing->getMemberDecl());
    assert(fieldDecl);
    // If we are accessing a derived struct, we need to account for the number
    // of base structs, since they are placed as fields at the beginning of the
    // derived struct.
    auto baseType = indexing->getBase()->getType();
    if (baseType->isPointerType()) {
      baseType = baseType->getPointeeType();
    }
    const uint32_t index =
        getNumBaseClasses(baseType) + fieldDecl->getFieldIndex();
    if (rawIndex) {
      rawIndices->push_back(index);
    } else {
      indices->push_back(spvBuilder.getConstantInt(
          astContext.IntTy, llvm::APInt(32, index, true)));
    }

    return base;
  }

  if (const auto *indexing = dyn_cast<ArraySubscriptExpr>(expr)) {
    if (rawIndex)
      return nullptr; // TODO: handle constant array index

    // The base of an ArraySubscriptExpr has a wrapping LValueToRValue implicit
    // cast. We need to ingore it to avoid creating OpLoad.
    const Expr *thisBase = indexing->getBase()->IgnoreParenLValueCasts();
    const Expr *base = collectArrayStructIndices(thisBase, rawIndex, rawIndices,
                                                 indices, isMSOutAttribute);
    // The index into an array must be an integer number.
    const auto *idxExpr = indexing->getIdx();
    const auto idxExprType = idxExpr->getType();
    SpirvInstruction *thisIndex = doExpr(idxExpr);
    if (!idxExprType->isIntegerType() || idxExprType->isBooleanType()) {
      thisIndex = castToInt(thisIndex, idxExprType, astContext.UnsignedIntTy,
                            idxExpr->getExprLoc());
    }

    indices->push_back(thisIndex);
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
      const Expr *base = collectArrayStructIndices(
          thisBase, rawIndex, rawIndices, indices, isMSOutAttribute);

      if (thisBaseType != base->getType() &&
          isAKindOfStructuredOrByteBuffer(thisBaseType)) {
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
      if (isStructuredBuffer(thisBaseType))
        indices->push_back(
            spvBuilder.getConstantInt(astContext.IntTy, llvm::APInt(32, 0)));

      if ((hlsl::IsHLSLVecType(thisBaseType) &&
           (hlsl::GetHLSLVecSize(thisBaseType) == 1)) ||
          is1x1Matrix(thisBaseType) || is1xNMatrix(thisBaseType)) {
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
      indices->push_back(
          spvBuilder.getConstantInt(astContext.IntTy, llvm::APInt(32, 0)));
      indices->push_back(doExpr(index));
      return object;
    }
  }

  // This the deepest we can go. No more array or struct indexing.
  return expr;
}

SpirvInstruction *SpirvEmitter::turnIntoElementPtr(
    QualType baseType, SpirvInstruction *base, QualType elemType,
    const llvm::SmallVector<SpirvInstruction *, 4> &indices,
    SourceLocation loc) {
  // If this is a rvalue, we need a temporary object to hold it
  // so that we can get access chain from it.
  const bool needTempVar = base->isRValue();
  SpirvInstruction *accessChainBase = base;

  if (needTempVar) {
    auto varName = getAstTypeName(baseType);
    const auto var = createTemporaryVar(baseType, varName, base, loc);
    var->setLayoutRule(SpirvLayoutRule::Void);
    var->setStorageClass(spv::StorageClass::Function);
    var->setContainsAliasComponent(base->containsAliasComponent());
    accessChainBase = var;
  }

  base = spvBuilder.createAccessChain(elemType, accessChainBase, indices, loc);

  // Okay, this part seems weird, but it is intended:
  // If the base is originally a rvalue, the whole AST involving the base
  // is consistently set up to handle rvalues. By copying the base into
  // a temporary variable and grab an access chain from it, we are breaking
  // the consistency by turning the base from rvalue into lvalue. Keep in
  // mind that there will be no LValueToRValue casts in the AST for us
  // to rely on to load the access chain if a rvalue is expected. Therefore,
  // we must do the load here. Otherwise, it's up to the consumer of this
  // access chain to do the load, and that can be everywhere.
  if (needTempVar) {
    base = spvBuilder.createLoad(elemType, base, loc);
  }

  return base;
}

SpirvInstruction *SpirvEmitter::castToBool(SpirvInstruction *fromVal,
                                           QualType fromType,
                                           QualType toBoolType,
                                           SourceLocation loc) {
  if (isSameType(astContext, fromType, toBoolType))
    return fromVal;

  { // Special case handling for converting to a matrix of booleans.
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(fromType, &elemType, &rowCount, &colCount)) {
      const auto fromRowQualType =
          astContext.getExtVectorType(elemType, colCount);
      const auto toBoolRowQualType =
          astContext.getExtVectorType(astContext.BoolTy, colCount);
      llvm::SmallVector<SpirvInstruction *, 4> rows;
      for (uint32_t i = 0; i < rowCount; ++i) {
        auto *row = spvBuilder.createCompositeExtract(fromRowQualType, fromVal,
                                                      {i}, loc);
        rows.push_back(
            castToBool(row, fromRowQualType, toBoolRowQualType, loc));
      }
      return spvBuilder.createCompositeConstruct(toBoolType, rows, loc);
    }
  }

  // Converting to bool means comparing with value zero.
  const spv::Op spvOp = translateOp(BO_NE, fromType);
  auto *zeroVal = getValueZero(fromType);
  return spvBuilder.createBinaryOp(spvOp, toBoolType, fromVal, zeroVal, loc);
}

SpirvInstruction *SpirvEmitter::castToInt(SpirvInstruction *fromVal,
                                          QualType fromType, QualType toIntType,
                                          SourceLocation srcLoc) {
  if (isEnumType(fromType))
    fromType = astContext.IntTy;

  if (isSameType(astContext, fromType, toIntType))
    return fromVal;

  if (isBoolOrVecOfBoolType(fromType)) {
    auto *one = getValueOne(toIntType);
    auto *zero = getValueZero(toIntType);
    return spvBuilder.createSelect(toIntType, fromVal, one, zero, srcLoc);
  }

  if (isSintOrVecOfSintType(fromType) || isUintOrVecOfUintType(fromType)) {
    // First convert the source to the bitwidth of the destination if necessary.
    QualType convertedType = {};
    fromVal =
        convertBitwidth(fromVal, srcLoc, fromType, toIntType, &convertedType);
    // If bitwidth conversion was the only thing we needed to do, we're done.
    if (isSameScalarOrVecType(convertedType, toIntType))
      return fromVal;
    return spvBuilder.createUnaryOp(spv::Op::OpBitcast, toIntType, fromVal,
                                    srcLoc);
  }

  if (isFloatOrVecOfFloatType(fromType)) {
    // First convert the source to the bitwidth of the destination if necessary.
    fromVal = convertBitwidth(fromVal, srcLoc, fromType, toIntType);
    if (isSintOrVecOfSintType(toIntType)) {
      return spvBuilder.createUnaryOp(spv::Op::OpConvertFToS, toIntType,
                                      fromVal, srcLoc);
    } else if (isUintOrVecOfUintType(toIntType)) {
      return spvBuilder.createUnaryOp(spv::Op::OpConvertFToU, toIntType,
                                      fromVal, srcLoc);
    } else {
      emitError("casting from floating point to integer unimplemented", srcLoc);
    }
  }

  {
    QualType elemType = {};
    uint32_t numRows = 0, numCols = 0;
    if (isMxNMatrix(fromType, &elemType, &numRows, &numCols)) {
      // The source matrix and the target matrix must have the same dimensions.
      QualType toElemType = {};
      uint32_t toNumRows = 0, toNumCols = 0;
      const bool isMat =
          isMxNMatrix(toIntType, &toElemType, &toNumRows, &toNumCols);
      assert(isMat && numRows == toNumRows && numCols == toNumCols);
      (void)isMat;
      (void)toNumRows;
      (void)toNumCols;

      // Casting to a matrix of integers: Cast each row and construct a
      // composite.
      llvm::SmallVector<SpirvInstruction *, 4> castedRows;
      const QualType vecType = getComponentVectorType(astContext, fromType);
      const auto fromVecQualType =
          astContext.getExtVectorType(elemType, numCols);
      const auto toIntVecQualType =
          astContext.getExtVectorType(toElemType, numCols);
      for (uint32_t row = 0; row < numRows; ++row) {
        auto *rowId =
            spvBuilder.createCompositeExtract(vecType, fromVal, {row}, srcLoc);
        castedRows.push_back(
            castToInt(rowId, fromVecQualType, toIntVecQualType, srcLoc));
      }
      return spvBuilder.createCompositeConstruct(toIntType, castedRows, srcLoc);
    }
  }

  return nullptr;
}

SpirvInstruction *SpirvEmitter::convertBitwidth(SpirvInstruction *fromVal,
                                                SourceLocation loc,
                                                QualType fromType,
                                                QualType toType,
                                                QualType *resultType) {
  // At the moment, we will not make bitwidth conversions to/from literal int
  // and literal float types because they do not represent the intended SPIR-V
  // bitwidth.
  if (isLitTypeOrVecOfLitType(fromType) || isLitTypeOrVecOfLitType(toType))
    return fromVal;

  const auto fromBitwidth = getElementSpirvBitwidth(
      astContext, fromType, spirvOptions.enable16BitTypes);
  const auto toBitwidth = getElementSpirvBitwidth(
      astContext, toType, spirvOptions.enable16BitTypes);
  if (fromBitwidth == toBitwidth) {
    if (resultType)
      *resultType = fromType;
    return fromVal;
  }

  // We want the 'fromType' with the 'toBitwidth'.
  const QualType targetType =
      getTypeWithCustomBitwidth(astContext, fromType, toBitwidth);
  if (resultType)
    *resultType = targetType;

  if (isFloatOrVecOfFloatType(fromType))
    return spvBuilder.createUnaryOp(spv::Op::OpFConvert, targetType, fromVal,
                                    loc);
  if (isSintOrVecOfSintType(fromType))
    return spvBuilder.createUnaryOp(spv::Op::OpSConvert, targetType, fromVal,
                                    loc);
  if (isUintOrVecOfUintType(fromType))
    return spvBuilder.createUnaryOp(spv::Op::OpUConvert, targetType, fromVal,
                                    loc);
  llvm_unreachable("invalid type passed to convertBitwidth");
}

SpirvInstruction *SpirvEmitter::castToFloat(SpirvInstruction *fromVal,
                                            QualType fromType,
                                            QualType toFloatType,
                                            SourceLocation srcLoc) {
  if (isSameType(astContext, fromType, toFloatType))
    return fromVal;

  if (isBoolOrVecOfBoolType(fromType)) {
    auto *one = getValueOne(toFloatType);
    auto *zero = getValueZero(toFloatType);
    return spvBuilder.createSelect(toFloatType, fromVal, one, zero, srcLoc);
  }

  if (isSintOrVecOfSintType(fromType)) {
    // First convert the source to the bitwidth of the destination if necessary.
    fromVal = convertBitwidth(fromVal, srcLoc, fromType, toFloatType);
    return spvBuilder.createUnaryOp(spv::Op::OpConvertSToF, toFloatType,
                                    fromVal, srcLoc);
  }

  if (isUintOrVecOfUintType(fromType)) {
    // First convert the source to the bitwidth of the destination if necessary.
    fromVal = convertBitwidth(fromVal, srcLoc, fromType, toFloatType);
    return spvBuilder.createUnaryOp(spv::Op::OpConvertUToF, toFloatType,
                                    fromVal, srcLoc);
  }

  if (isFloatOrVecOfFloatType(fromType)) {
    // This is the case of float to float conversion with different bitwidths.
    return convertBitwidth(fromVal, srcLoc, fromType, toFloatType);
  }

  // Casting matrix types
  {
    QualType elemType = {};
    uint32_t numRows = 0, numCols = 0;
    if (isMxNMatrix(fromType, &elemType, &numRows, &numCols)) {
      // The source matrix and the target matrix must have the same dimensions.
      QualType toElemType = {};
      uint32_t toNumRows = 0, toNumCols = 0;
      const auto isMat =
          isMxNMatrix(toFloatType, &toElemType, &toNumRows, &toNumCols);
      assert(isMat && numRows == toNumRows && numCols == toNumCols);
      (void)isMat;
      (void)toNumRows;
      (void)toNumCols;

      // Casting to a matrix of floats: Cast each row and construct a
      // composite.
      llvm::SmallVector<SpirvInstruction *, 4> castedRows;
      const QualType vecType = getComponentVectorType(astContext, fromType);
      const auto fromVecQualType =
          astContext.getExtVectorType(elemType, numCols);
      const auto toIntVecQualType =
          astContext.getExtVectorType(toElemType, numCols);
      for (uint32_t row = 0; row < numRows; ++row) {
        auto *rowId =
            spvBuilder.createCompositeExtract(vecType, fromVal, {row}, srcLoc);
        castedRows.push_back(
            castToFloat(rowId, fromVecQualType, toIntVecQualType, srcLoc));
      }
      return spvBuilder.createCompositeConstruct(toFloatType, castedRows,
                                                 srcLoc);
    }
  }

  emitError("casting to floating point unimplemented", srcLoc);
  return nullptr;
}

SpirvInstruction *
SpirvEmitter::processIntrinsicCallExpr(const CallExpr *callExpr) {
  const FunctionDecl *callee = callExpr->getDirectCallee();
  const SourceLocation srcLoc = callExpr->getExprLoc();
  assert(hlsl::IsIntrinsicOp(callee) &&
         "doIntrinsicCallExpr was called for a non-intrinsic function.");

  const bool isFloatType = isFloatOrVecMatOfFloatType(callExpr->getType());
  const bool isSintType = isSintOrVecMatOfSintType(callExpr->getType());

  // Figure out which intrinsic function to translate.
  llvm::StringRef group;
  uint32_t opcode = static_cast<uint32_t>(hlsl::IntrinsicOp::Num_Intrinsics);
  hlsl::GetIntrinsicOp(callee, opcode, group);

  GLSLstd450 glslOpcode = GLSLstd450Bad;

  SpirvInstruction *retVal = nullptr;

#define INTRINSIC_SPIRV_OP_CASE(intrinsicOp, spirvOp, doEachVec)               \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    retVal = processIntrinsicUsingSpirvInst(callExpr, spv::Op::Op##spirvOp,    \
                                            doEachVec);                        \
  } break

#define INTRINSIC_OP_CASE(intrinsicOp, glslOp, doEachVec)                      \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = GLSLstd450::GLSLstd450##glslOp;                               \
    retVal = processIntrinsicUsingGLSLInst(callExpr, glslOpcode, doEachVec,    \
                                           srcLoc);                            \
  } break

#define INTRINSIC_OP_CASE_INT_FLOAT(intrinsicOp, glslIntOp, glslFloatOp,       \
                                    doEachVec)                                 \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = isFloatType ? GLSLstd450::GLSLstd450##glslFloatOp             \
                             : GLSLstd450::GLSLstd450##glslIntOp;              \
    retVal = processIntrinsicUsingGLSLInst(callExpr, glslOpcode, doEachVec,    \
                                           srcLoc);                            \
  } break

#define INTRINSIC_OP_CASE_SINT_UINT(intrinsicOp, glslSintOp, glslUintOp,       \
                                    doEachVec)                                 \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = isSintType ? GLSLstd450::GLSLstd450##glslSintOp               \
                            : GLSLstd450::GLSLstd450##glslUintOp;              \
    retVal = processIntrinsicUsingGLSLInst(callExpr, glslOpcode, doEachVec,    \
                                           srcLoc);                            \
  } break

#define INTRINSIC_OP_CASE_SINT_UINT_FLOAT(intrinsicOp, glslSintOp, glslUintOp, \
                                          glslFloatOp, doEachVec)              \
  case hlsl::IntrinsicOp::IOP_##intrinsicOp: {                                 \
    glslOpcode = isFloatType                                                   \
                     ? GLSLstd450::GLSLstd450##glslFloatOp                     \
                     : isSintType ? GLSLstd450::GLSLstd450##glslSintOp         \
                                  : GLSLstd450::GLSLstd450##glslUintOp;        \
    retVal = processIntrinsicUsingGLSLInst(callExpr, glslOpcode, doEachVec,    \
                                           srcLoc);                            \
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
    emitError("deprecated %0 intrinsic function will not be supported", srcLoc)
        << callee->getName();
    return nullptr;
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
    retVal = spvBuilder.createImageSparseTexelsResident(
        doExpr(callExpr->getArg(0)), srcLoc);
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
  case hlsl::IntrinsicOp::IOP_mad:
  case hlsl::IntrinsicOp::IOP_umad:
    retVal = processIntrinsicMad(callExpr);
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
                                        /*actPerRowForMatrices*/ true, srcLoc);
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
                                    srcLoc);
    const QualType retType = callExpr->getCallReturnType(astContext);
    auto *var =
        declIdMapper.getBuiltinVar(spv::BuiltIn::SubgroupSize, retType, srcLoc);

    retVal = spvBuilder.createLoad(retType, var, srcLoc);
  } break;
  case hlsl::IntrinsicOp::IOP_WaveGetLaneIndex: {
    featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "WaveGetLaneIndex",
                                    srcLoc);
    const QualType retType = callExpr->getCallReturnType(astContext);
    auto *var = declIdMapper.getBuiltinVar(
        spv::BuiltIn::SubgroupLocalInvocationId, retType, srcLoc);
    retVal = spvBuilder.createLoad(retType, var, srcLoc);
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
    retVal = processWaveCountBits(callExpr, spv::GroupOperation::Reduce);
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
        callExpr, translateWaveOp(hlslOpcode, retType, srcLoc),
        spv::GroupOperation::Reduce);
  } break;
  case hlsl::IntrinsicOp::IOP_WavePrefixUSum:
  case hlsl::IntrinsicOp::IOP_WavePrefixSum:
  case hlsl::IntrinsicOp::IOP_WavePrefixUProduct:
  case hlsl::IntrinsicOp::IOP_WavePrefixProduct: {
    const auto retType = callExpr->getCallReturnType(astContext);
    retVal = processWaveReductionOrPrefix(
        callExpr, translateWaveOp(hlslOpcode, retType, srcLoc),
        spv::GroupOperation::ExclusiveScan);
  } break;
  case hlsl::IntrinsicOp::IOP_WavePrefixCountBits:
    retVal = processWaveCountBits(callExpr, spv::GroupOperation::ExclusiveScan);
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
    emitError("no equivalent for %0 intrinsic function in Vulkan", srcLoc)
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
      retVal = processNonFpMatrixTranspose(matType, doExpr(mat), srcLoc);

    break;
  }
  // DXR raytracing intrinsics
  case hlsl::IntrinsicOp::IOP_DispatchRaysDimensions:
  case hlsl::IntrinsicOp::IOP_DispatchRaysIndex:
  case hlsl::IntrinsicOp::IOP_HitKind:
  case hlsl::IntrinsicOp::IOP_InstanceIndex:
  case hlsl::IntrinsicOp::IOP_InstanceID:
  case hlsl::IntrinsicOp::IOP_ObjectRayDirection:
  case hlsl::IntrinsicOp::IOP_ObjectRayOrigin:
  case hlsl::IntrinsicOp::IOP_ObjectToWorld3x4:
  case hlsl::IntrinsicOp::IOP_ObjectToWorld4x3:
  case hlsl::IntrinsicOp::IOP_PrimitiveIndex:
  case hlsl::IntrinsicOp::IOP_RayFlags:
  case hlsl::IntrinsicOp::IOP_RayTCurrent:
  case hlsl::IntrinsicOp::IOP_RayTMin:
  case hlsl::IntrinsicOp::IOP_WorldRayDirection:
  case hlsl::IntrinsicOp::IOP_WorldRayOrigin:
  case hlsl::IntrinsicOp::IOP_WorldToObject3x4:
  case hlsl::IntrinsicOp::IOP_WorldToObject4x3: {
    retVal = processRayBuiltins(callExpr, hlslOpcode);
    break;
  }
  case hlsl::IntrinsicOp::IOP_AcceptHitAndEndSearch:
  case hlsl::IntrinsicOp::IOP_IgnoreHit: {

    // Any modifications made to the ray payload in an any hit shader are
    // preserved before calling AcceptHit/IgnoreHit. Write out the results to
    // the payload which is visible only in entry functions
    const auto iter = functionInfoMap.find(curFunction);
    if (iter != functionInfoMap.end()) {
      const auto &entryInfo = iter->second;
      if (entryInfo->isEntryFunction) {
        const auto payloadArg = curFunction->getParamDecl(0);
        const auto payloadArgInst =
            declIdMapper.getDeclEvalInfo(payloadArg, payloadArg->getLocStart());
        auto tempLoad = spvBuilder.createLoad(
            payloadArg->getType(), payloadArgInst, payloadArg->getLocStart());
        spvBuilder.createStore(currentRayPayload, tempLoad,
                               callExpr->getExprLoc());
      }
    }
    spvBuilder.createRayTracingOpsNV(
        hlslOpcode == hlsl::IntrinsicOp ::IOP_AcceptHitAndEndSearch
            ? spv::Op::OpTerminateRayNV
            : spv::Op::OpIgnoreIntersectionNV,
        QualType(), {}, srcLoc);
    break;
  }
  case hlsl::IntrinsicOp::IOP_ReportHit: {
    retVal = processReportHit(callExpr);
    break;
  }
  case hlsl::IntrinsicOp::IOP_TraceRay: {
    processTraceRay(callExpr);
    break;
  }
  case hlsl::IntrinsicOp::IOP_CallShader: {
    processCallShader(callExpr);
    break;
  }
  case hlsl::IntrinsicOp::IOP_DispatchMesh: {
    processDispatchMesh(callExpr);
    break;
  }
  case hlsl::IntrinsicOp::IOP_SetMeshOutputCounts: {
    processMeshOutputCounts(callExpr);
    break;
  }
    INTRINSIC_SPIRV_OP_CASE(ddx, DPdx, true);
    INTRINSIC_SPIRV_OP_CASE(ddx_coarse, DPdxCoarse, false);
    INTRINSIC_SPIRV_OP_CASE(ddx_fine, DPdxFine, false);
    INTRINSIC_SPIRV_OP_CASE(ddy, DPdy, true);
    INTRINSIC_SPIRV_OP_CASE(ddy_coarse, DPdyCoarse, false);
    INTRINSIC_SPIRV_OP_CASE(ddy_fine, DPdyFine, false);
    INTRINSIC_SPIRV_OP_CASE(countbits, BitCount, false);
    INTRINSIC_SPIRV_OP_CASE(isinf, IsInf, true);
    INTRINSIC_SPIRV_OP_CASE(isnan, IsNan, true);
    INTRINSIC_SPIRV_OP_CASE(fmod, FMod, true);
    INTRINSIC_SPIRV_OP_CASE(fwidth, Fwidth, true);
    INTRINSIC_SPIRV_OP_CASE(reversebits, BitReverse, false);
    INTRINSIC_OP_CASE(round, Round, true);
    INTRINSIC_OP_CASE(uabs, SAbs, true);
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
    emitError("%0 intrinsic function unimplemented", srcLoc)
        << callee->getName();
    return 0;
  }

#undef INTRINSIC_OP_CASE
#undef INTRINSIC_OP_CASE_INT_FLOAT

  if (retVal)
    retVal->setRValue();
  return retVal;
}

SpirvInstruction *
SpirvEmitter::processIntrinsicInterlockedMethod(const CallExpr *expr,
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

  auto *zero =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  const auto *dest = expr->getArg(0);
  const auto srcLoc = expr->getExprLoc();
  const auto baseType = dest->getType()->getCanonicalTypeUnqualified();

  if (!baseType->isIntegerType()) {
    emitError("can only perform atomic operations on scalar integer values",
              dest->getLocStart());
    return nullptr;
  }

  const auto doArg = [baseType, this](const CallExpr *callExpr,
                                      uint32_t argIndex) {
    const Expr *valueExpr = callExpr->getArg(argIndex);
    if (const auto *castExpr = dyn_cast<ImplicitCastExpr>(valueExpr))
      if (castExpr->getCastKind() == CK_IntegralCast &&
          castExpr->getSubExpr()->getType() == baseType)
        valueExpr = castExpr->getSubExpr();

    auto *argInstr = doExpr(valueExpr);
    if (valueExpr->getType() != baseType)
      argInstr = castToInt(argInstr, valueExpr->getType(), baseType,
                           valueExpr->getExprLoc());
    return argInstr;
  };

  const auto writeToOutputArg = [&baseType, dest,
                                 this](SpirvInstruction *toWrite,
                                       const CallExpr *callExpr,
                                       uint32_t outputArgIndex) {
    const auto outputArg = callExpr->getArg(outputArgIndex);
    const auto outputArgType = outputArg->getType();
    if (baseType != outputArgType)
      toWrite =
          castToInt(toWrite, baseType, outputArgType, dest->getLocStart());
    spvBuilder.createStore(doExpr(outputArg), toWrite, callExpr->getExprLoc());
  };

  // If a vector swizzling of a texture is done as an argument of an
  // interlocked method, we need to handle the access to the texture
  // buffer element correctly. For example:
  //
  //  InterlockedAdd(myRWTexture[index].r, 1);
  //
  // `-CallExpr
  //  |-ImplicitCastExpr
  //  | `-DeclRefExpr Function 'InterlockedAdd'
  //  |                        'void (unsigned int &, unsigned int)'
  //  |-HLSLVectorElementExpr 'unsigned int' lvalue vectorcomponent r
  //  | `-ImplicitCastExpr 'vector<uint, 1>':'vector<unsigned int, 1>'
  //  |                                       <HLSLVectorSplat>
  //  |   `-CXXOperatorCallExpr 'unsigned int' lvalue
  const auto *cxxOpCall = dyn_cast<CXXOperatorCallExpr>(dest);
  if (const auto *vector = dyn_cast<HLSLVectorElementExpr>(dest)) {
    const Expr *base = vector->getBase();
    cxxOpCall = dyn_cast<CXXOperatorCallExpr>(base);
    if (const auto *cast = dyn_cast<CastExpr>(base)) {
      cxxOpCall = dyn_cast<CXXOperatorCallExpr>(cast->getSubExpr());
    }
  }

  // If the argument is indexing into a texture/buffer, we need to create an
  // OpImageTexelPointer instruction.
  SpirvInstruction *ptr = nullptr;
  if (cxxOpCall) {
    const Expr *base = nullptr;
    const Expr *index = nullptr;
    if (isBufferTextureIndexing(cxxOpCall, &base, &index)) {
      if (hlsl::IsHLSLResourceType(base->getType())) {
        const auto resultTy = hlsl::GetHLSLResourceResultType(base->getType());
        if (!isScalarType(resultTy, nullptr)) {
          emitError("Interlocked operation for texture buffer whose result "
                    "type is non-scalar type is not allowed",
                    dest->getExprLoc());
          return nullptr;
        }
      }
      auto *baseInstr = doExpr(base);
      if (baseInstr->isRValue()) {
        // OpImageTexelPointer's Image argument must have a type of
        // OpTypePointer with Type OpTypeImage. Need to create a temporary
        // variable if the baseId is an rvalue.
        baseInstr =
            createTemporaryVar(base->getType(), getAstTypeName(base->getType()),
                               baseInstr, base->getExprLoc());
      }
      auto *coordInstr = doExpr(index);
      ptr = spvBuilder.createImageTexelPointer(baseType, baseInstr, coordInstr,
                                               zero, srcLoc);
    }
  }
  if (!ptr) {
    auto *ptrInfo = doExpr(dest);
    const auto sc = ptrInfo->getStorageClass();
    if (sc == spv::StorageClass::Private || sc == spv::StorageClass::Function) {
      emitError("using static variable or function scope variable in "
                "interlocked operation is not allowed",
                dest->getExprLoc());
      return nullptr;
    }
    ptr = ptrInfo;
  }

  const bool isCompareExchange =
      opcode == hlsl::IntrinsicOp::IOP_InterlockedCompareExchange;
  const bool isCompareStore =
      opcode == hlsl::IntrinsicOp::IOP_InterlockedCompareStore;

  if (isCompareExchange || isCompareStore) {
    auto *comparator = doArg(expr, 1);
    auto *valueInstr = doArg(expr, 2);
    auto *originalVal = spvBuilder.createAtomicCompareExchange(
        baseType, ptr, spv::Scope::Device, spv::MemorySemanticsMask::MaskNone,
        spv::MemorySemanticsMask::MaskNone, valueInstr, comparator, srcLoc);
    if (isCompareExchange)
      writeToOutputArg(originalVal, expr, 3);
  } else {
    auto *value = doArg(expr, 1);
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
    auto *originalVal = spvBuilder.createAtomicOp(
        atomicOp, baseType, ptr, spv::Scope::Device,
        spv::MemorySemanticsMask::MaskNone, value, srcLoc);
    if (expr->getNumArgs() > 2)
      writeToOutputArg(originalVal, expr, 2);
  }

  return nullptr;
}

SpirvInstruction *
SpirvEmitter::processIntrinsicNonUniformResourceIndex(const CallExpr *expr) {
  auto *index = doExpr(expr->getArg(0));
  // Decorate the expression in NonUniformResourceIndex() with NonUniformEXT.
  // Aside from this, we also need to eventually populate the NonUniformEXT
  // status to the usage of this expression: the "pointer" operand to a memory
  // access instruction. Vulkan spec has the following rules:
  //
  // If an instruction loads from or stores to a resource (including atomics and
  // image instructions) and the resource descriptor being accessed is not
  // dynamically uniform, then the operand corresponding to that resource (e.g.
  // the pointer or sampled image operand) must be decorated with NonUniformEXT.
  index->setNonUniform();
  return index;
}

SpirvInstruction *
SpirvEmitter::processIntrinsicMsad4(const CallExpr *callExpr) {
  const auto loc = callExpr->getExprLoc();
  if (!spirvOptions.noWarnEmulatedFeatures)
    emitWarning("msad4 intrinsic function is emulated using many SPIR-V "
                "instructions due to lack of direct SPIR-V equivalent",
                loc);

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

  auto *glsl = spvBuilder.getGLSLExtInstSet();
  const auto boolType = astContext.BoolTy;
  const auto intType = astContext.IntTy;
  const auto uintType = astContext.UnsignedIntTy;
  const auto uint4Type = astContext.getExtVectorType(uintType, 4);
  auto *reference = doExpr(callExpr->getArg(0));
  auto *source = doExpr(callExpr->getArg(1));
  auto *accum = doExpr(callExpr->getArg(2));
  const auto uint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  const auto uint8 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 8));
  const auto uint16 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 16));
  const auto uint24 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 24));

  // Step 1.
  auto *v1x = spvBuilder.createCompositeExtract(uintType, source, {0}, loc);
  // r0.x = v1xS8 = v1.x shifted by 8 bits
  auto *v1xS8 = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical, uintType,
                                          v1x, uint8, loc);
  // r0.y = v1xS16 = v1.x shifted by 16 bits
  auto *v1xS16 = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                           uintType, v1x, uint16, loc);
  // r0.z = v1xS24 = v1.x shifted by 24 bits
  auto *v1xS24 = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                           uintType, v1x, uint24, loc);

  // Step 2.
  // Do bfi 3 times. DXIL bfi is equivalent to SPIR-V OpBitFieldInsert.
  auto *v1y = spvBuilder.createCompositeExtract(uintType, source, {1}, loc);
  // Note that t0.x = v1.x, nothing we need to do for that.
  auto *t0y =
      spvBuilder.createBitFieldInsert(uintType, /*base*/ v1xS8, /*insert*/ v1y,
                                      /*offset*/ uint24,
                                      /*width*/ uint8, loc);
  auto *t0z =
      spvBuilder.createBitFieldInsert(uintType, /*base*/ v1xS16, /*insert*/ v1y,
                                      /*offset*/ uint16,
                                      /*width*/ uint16, loc);
  auto *t0w =
      spvBuilder.createBitFieldInsert(uintType, /*base*/ v1xS24, /*insert*/ v1y,
                                      /*offset*/ uint8,
                                      /*width*/ uint24, loc);

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

  auto *accum0 = spvBuilder.createCompositeExtract(uintType, accum, {0}, loc);
  auto *accum1 = spvBuilder.createCompositeExtract(uintType, accum, {1}, loc);
  auto *accum2 = spvBuilder.createCompositeExtract(uintType, accum, {2}, loc);
  auto *accum3 = spvBuilder.createCompositeExtract(uintType, accum, {3}, loc);
  const llvm::SmallVector<SpirvInstruction *, 4> sources = {v1x, t0y, t0z, t0w};
  llvm::SmallVector<SpirvInstruction *, 4> accums = {accum0, accum1, accum2,
                                                     accum3};
  llvm::SmallVector<SpirvInstruction *, 4> refBytes;
  llvm::SmallVector<SpirvInstruction *, 4> signedRefBytes;
  llvm::SmallVector<SpirvInstruction *, 4> isRefByteZero;
  for (uint32_t i = 0; i < 4; ++i) {
    refBytes.push_back(spvBuilder.createBitFieldExtract(
        uintType, reference, /*offset*/
        spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                  llvm::APInt(32, i * 8)),
        /*count*/ uint8, /*isSigned*/ false, loc));
    signedRefBytes.push_back(spvBuilder.createUnaryOp(
        spv::Op::OpBitcast, intType, refBytes.back(), loc));
    isRefByteZero.push_back(spvBuilder.createBinaryOp(
        spv::Op::OpIEqual, boolType, refBytes.back(), uint0, loc));
  }

  for (uint32_t msadNum = 0; msadNum < 4; ++msadNum) {
    for (uint32_t byteCount = 0; byteCount < 4; ++byteCount) {
      // 'count' is always 8 because we are extracting 8 bits out of 32.
      auto *srcByte = spvBuilder.createBitFieldExtract(
          uintType, sources[msadNum],
          /*offset*/
          spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                    llvm::APInt(32, 8 * byteCount)),
          /*count*/ uint8, /*isSigned*/ false, loc);
      auto *signedSrcByte =
          spvBuilder.createUnaryOp(spv::Op::OpBitcast, intType, srcByte, loc);
      auto *sub = spvBuilder.createBinaryOp(spv::Op::OpISub, intType,
                                            signedRefBytes[byteCount],
                                            signedSrcByte, loc);
      auto *absSub = spvBuilder.createExtInst(
          intType, glsl, GLSLstd450::GLSLstd450SAbs, {sub}, loc);
      auto *diff = spvBuilder.createSelect(
          uintType, isRefByteZero[byteCount], uint0,
          spvBuilder.createUnaryOp(spv::Op::OpBitcast, uintType, absSub, loc),
          loc);

      // As pointed out by the DXIL reference above, it is *not* required to
      // saturate the output to UINT_MAX in case of overflow. Wrapping around is
      // also allowed. For simplicity, we will wrap around at this point.
      accums[msadNum] = spvBuilder.createBinaryOp(spv::Op::OpIAdd, uintType,
                                                  accums[msadNum], diff, loc);
    }
  }
  return spvBuilder.createCompositeConstruct(uint4Type, accums, loc);
}

SpirvInstruction *SpirvEmitter::processWaveQuery(const CallExpr *callExpr,
                                                 spv::Op opcode) {
  // Signatures:
  // bool WaveIsFirstLane()
  // uint WaveGetLaneCount()
  // uint WaveGetLaneIndex()
  assert(callExpr->getNumArgs() == 0);
  featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "Wave Operation",
                                  callExpr->getExprLoc());
  const QualType retType = callExpr->getCallReturnType(astContext);
  return spvBuilder.createGroupNonUniformElect(
      opcode, retType, spv::Scope::Subgroup, callExpr->getExprLoc());
}

SpirvInstruction *SpirvEmitter::processWaveVote(const CallExpr *callExpr,
                                                spv::Op opcode) {
  // Signatures:
  // bool WaveActiveAnyTrue( bool expr )
  // bool WaveActiveAllTrue( bool expr )
  // bool uint4 WaveActiveBallot( bool expr )
  assert(callExpr->getNumArgs() == 1);
  featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "Wave Operation",
                                  callExpr->getExprLoc());
  auto *predicate = doExpr(callExpr->getArg(0));
  const QualType retType = callExpr->getCallReturnType(astContext);
  return spvBuilder.createGroupNonUniformUnaryOp(
      callExpr->getExprLoc(), opcode, retType, spv::Scope::Subgroup, predicate);
}

spv::Op SpirvEmitter::translateWaveOp(hlsl::IntrinsicOp op, QualType type,
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

SpirvInstruction *
SpirvEmitter::processWaveCountBits(const CallExpr *callExpr,
                                   spv::GroupOperation groupOp) {
  // Signatures:
  // uint WaveActiveCountBits(bool bBit)
  // uint WavePrefixCountBits(Bool bBit)
  assert(callExpr->getNumArgs() == 1);

  featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "Wave Operation",
                                  callExpr->getExprLoc());
  auto *predicate = doExpr(callExpr->getArg(0));
  const auto srcLoc = callExpr->getExprLoc();
  const QualType u32Type = astContext.UnsignedIntTy;
  const QualType v4u32Type = astContext.getExtVectorType(u32Type, 4);
  const QualType retType = callExpr->getCallReturnType(astContext);
  auto *ballot = spvBuilder.createGroupNonUniformUnaryOp(
      srcLoc, spv::Op::OpGroupNonUniformBallot, v4u32Type, spv::Scope::Subgroup,
      predicate);

  return spvBuilder.createGroupNonUniformUnaryOp(
      srcLoc, spv::Op::OpGroupNonUniformBallotBitCount, retType,
      spv::Scope::Subgroup, ballot,
      llvm::Optional<spv::GroupOperation>(groupOp));
}

SpirvInstruction *SpirvEmitter::processWaveReductionOrPrefix(
    const CallExpr *callExpr, spv::Op opcode, spv::GroupOperation groupOp) {
  // Signatures:
  // bool WaveActiveAllEqual( <type> expr )
  // <type> WaveActiveSum( <type> expr )
  // <type> WaveActiveProduct( <type> expr )
  // <int_type> WaveActiveBitAnd( <int_type> expr )
  // <int_type> WaveActiveBitOr( <int_type> expr )
  // <int_type> WaveActiveBitXor( <int_type> expr )
  // <type> WaveActiveMin( <type> expr)
  // <type> WaveActiveMax( <type> expr)
  //
  // <type> WavePrefixProduct(<type> value)
  // <type> WavePrefixSum(<type> value)
  assert(callExpr->getNumArgs() == 1);
  featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "Wave Operation",
                                  callExpr->getExprLoc());
  auto *predicate = doExpr(callExpr->getArg(0));
  const QualType retType = callExpr->getCallReturnType(astContext);
  return spvBuilder.createGroupNonUniformUnaryOp(
      callExpr->getExprLoc(), opcode, retType, spv::Scope::Subgroup, predicate,
      llvm::Optional<spv::GroupOperation>(groupOp));
}

SpirvInstruction *SpirvEmitter::processWaveBroadcast(const CallExpr *callExpr) {
  // Signatures:
  // <type> WaveReadLaneFirst(<type> expr)
  // <type> WaveReadLaneAt(<type> expr, uint laneIndex)
  const auto numArgs = callExpr->getNumArgs();
  const auto srcLoc = callExpr->getExprLoc();
  assert(numArgs == 1 || numArgs == 2);
  featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "Wave Operation",
                                  callExpr->getExprLoc());
  auto *value = doExpr(callExpr->getArg(0));
  const QualType retType = callExpr->getCallReturnType(astContext);
  if (numArgs == 2)
    return spvBuilder.createGroupNonUniformBinaryOp(
        spv::Op::OpGroupNonUniformBroadcast, retType, spv::Scope::Subgroup,
        value, doExpr(callExpr->getArg(1)), srcLoc);
  else
    return spvBuilder.createGroupNonUniformUnaryOp(
        srcLoc, spv::Op::OpGroupNonUniformBroadcastFirst, retType,
        spv::Scope::Subgroup, value);
}

SpirvInstruction *
SpirvEmitter::processWaveQuadWideShuffle(const CallExpr *callExpr,
                                         hlsl::IntrinsicOp op) {
  // Signatures:
  // <type> QuadReadAcrossX(<type> localValue)
  // <type> QuadReadAcrossY(<type> localValue)
  // <type> QuadReadAcrossDiagonal(<type> localValue)
  // <type> QuadReadLaneAt(<type> sourceValue, uint quadLaneID)
  assert(callExpr->getNumArgs() == 1 || callExpr->getNumArgs() == 2);
  featureManager.requestTargetEnv(SPV_ENV_VULKAN_1_1, "Wave Operation",
                                  callExpr->getExprLoc());

  auto *value = doExpr(callExpr->getArg(0));
  const auto srcLoc = callExpr->getExprLoc();
  const QualType retType = callExpr->getCallReturnType(astContext);

  SpirvInstruction *target = nullptr;
  spv::Op opcode = spv::Op::OpGroupNonUniformQuadSwap;
  switch (op) {
  case hlsl::IntrinsicOp::IOP_QuadReadAcrossX:
    target =
        spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
    break;
  case hlsl::IntrinsicOp::IOP_QuadReadAcrossY:
    target =
        spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 1));
    break;
  case hlsl::IntrinsicOp::IOP_QuadReadAcrossDiagonal:
    target =
        spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 2));
    break;
  case hlsl::IntrinsicOp::IOP_QuadReadLaneAt:
    target = doExpr(callExpr->getArg(1));
    opcode = spv::Op::OpGroupNonUniformQuadBroadcast;
    break;
  default:
    llvm_unreachable("case should not appear here");
  }

  return spvBuilder.createGroupNonUniformBinaryOp(
      opcode, retType, spv::Scope::Subgroup, value, target, srcLoc);
}

SpirvInstruction *SpirvEmitter::processIntrinsicModf(const CallExpr *callExpr) {
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

  auto *glslInstSet = spvBuilder.getGLSLExtInstSet();
  const Expr *arg = callExpr->getArg(0);
  const Expr *ipArg = callExpr->getArg(1);
  const auto loc = callExpr->getLocStart();
  const auto argType = arg->getType();
  const auto ipType = ipArg->getType();
  const auto returnType = callExpr->getType();
  auto *argInstr = doExpr(arg);

  // For scalar and vector argument types.
  {
    if (isScalarType(argType) || isVectorType(argType)) {
      // The struct members *must* have the same type.
      const auto modfStructType = spvContext.getHybridStructType(
          {HybridStructType::FieldInfo(argType, "frac"),
           HybridStructType::FieldInfo(argType, "ip")},
          "ModfStructType");
      auto *modf = spvBuilder.createExtInst(modfStructType, glslInstSet,
                                            GLSLstd450::GLSLstd450ModfStruct,
                                            {argInstr}, loc);
      SpirvInstruction *ip =
          spvBuilder.createCompositeExtract(argType, modf, {1}, loc);
      // This will do nothing if the input number (x) and the ip are both of the
      // same type. Otherwise, it will convert the ip into int as necessary.
      ip = castToInt(ip, argType, ipType, ipArg->getLocStart());
      processAssignment(ipArg, ip, false, nullptr);
      return spvBuilder.createCompositeExtract(argType, modf, {0}, loc);
    }
  }

  // For matrix argument types.
  {
    uint32_t rowCount = 0, colCount = 0;
    QualType elemType = {};
    if (isMxNMatrix(argType, &elemType, &rowCount, &colCount)) {
      const auto colType = astContext.getExtVectorType(elemType, colCount);
      const auto modfStructType = spvContext.getHybridStructType(
          {HybridStructType::FieldInfo(colType, "frac"),
           HybridStructType::FieldInfo(colType, "ip")},
          "ModfStructType");
      llvm::SmallVector<SpirvInstruction *, 4> fracs;
      llvm::SmallVector<SpirvInstruction *, 4> ips;
      for (uint32_t i = 0; i < rowCount; ++i) {
        auto *curRow =
            spvBuilder.createCompositeExtract(colType, argInstr, {i}, loc);
        auto *modf = spvBuilder.createExtInst(modfStructType, glslInstSet,
                                              GLSLstd450::GLSLstd450ModfStruct,
                                              {curRow}, loc);
        ips.push_back(
            spvBuilder.createCompositeExtract(colType, modf, {1}, loc));
        fracs.push_back(
            spvBuilder.createCompositeExtract(colType, modf, {0}, loc));
      }

      SpirvInstruction *ip =
          spvBuilder.createCompositeConstruct(argType, ips, loc);
      // If the 'ip' is not a float type, the AST will not contain a CastExpr
      // because this is internal to the intrinsic function. So, in such a
      // case we need to cast manually.
      if (!hlsl::GetHLSLMatElementType(ipType)->isFloatingType())
        ip = castToInt(ip, argType, ipType, ipArg->getLocStart());
      processAssignment(ipArg, ip, false, nullptr);
      return spvBuilder.createCompositeConstruct(returnType, fracs, loc);
    }
  }

  emitError("invalid argument type passed to Modf intrinsic function",
            callExpr->getExprLoc());
  return nullptr;
}

SpirvInstruction *SpirvEmitter::processIntrinsicMad(const CallExpr *callExpr) {
  // Signature is: ret mad(a,b,c)
  // All of the above must be a scalar, vector, or matrix with the same
  // component types. Component types can be float or int.
  // The return value is equal to  "a * b + c"

  // In the case of float arguments, we can use the GLSL extended instruction
  // set's Fma instruction with NoContraction decoration. In the case of integer
  // arguments, we'll have to manually perform an OpIMul followed by an OpIAdd
  // (We should also apply NoContraction decoration to these two instructions to
  // get precise arithmetic).

  // TODO: We currently don't propagate the NoContraction decoration.

  const auto loc = callExpr->getLocStart();
  const Expr *arg0 = callExpr->getArg(0);
  const Expr *arg1 = callExpr->getArg(1);
  const Expr *arg2 = callExpr->getArg(2);
  // All arguments and the return type are the same.
  const auto argType = arg0->getType();
  auto *arg0Instr = doExpr(arg0);
  auto *arg1Instr = doExpr(arg1);
  auto *arg2Instr = doExpr(arg2);
  auto arg0Loc = arg0->getLocStart();
  auto arg1Loc = arg1->getLocStart();
  auto arg2Loc = arg2->getLocStart();

  // For floating point arguments, we can use the extended instruction set's Fma
  // instruction. Sadly we can't simply call processIntrinsicUsingGLSLInst
  // because we need to specifically decorate the Fma instruction with
  // NoContraction decoration.
  if (isFloatOrVecMatOfFloatType(argType)) {
    auto *glslInstSet = spvBuilder.getGLSLExtInstSet();
    // For matrix cases, operate on each row of the matrix.
    if (isMxNMatrix(arg0->getType())) {
      const auto actOnEachVec = [this, loc, glslInstSet, arg1Instr, arg2Instr,
                                 arg1Loc,
                                 arg2Loc](uint32_t index, QualType vecType,
                                          SpirvInstruction *arg0Row) {
        auto *arg1Row = spvBuilder.createCompositeExtract(vecType, arg1Instr,
                                                          {index}, arg1Loc);
        auto *arg2Row = spvBuilder.createCompositeExtract(vecType, arg2Instr,
                                                          {index}, arg2Loc);
        auto *fma =
            spvBuilder.createExtInst(vecType, glslInstSet, GLSLstd450Fma,
                                     {arg0Row, arg1Row, arg2Row}, loc);
        spvBuilder.decorateNoContraction(fma, loc);
        return fma;
      };
      return processEachVectorInMatrix(arg0, arg0Instr, actOnEachVec, loc);
    }
    // Non-matrix cases
    auto *fma =
        spvBuilder.createExtInst(argType, glslInstSet, GLSLstd450Fma,
                                 {arg0Instr, arg1Instr, arg2Instr}, loc);
    spvBuilder.decorateNoContraction(fma, loc);
    return fma;
  }

  // For scalar and vector argument types.
  {
    if (isScalarType(argType) || isVectorType(argType)) {
      auto *mul = spvBuilder.createBinaryOp(spv::Op::OpIMul, argType, arg0Instr,
                                            arg1Instr, loc);
      auto *add = spvBuilder.createBinaryOp(spv::Op::OpIAdd, argType, mul,
                                            arg2Instr, loc);
      spvBuilder.decorateNoContraction(mul, loc);
      spvBuilder.decorateNoContraction(add, loc);
      return add;
    }
  }

  // For matrix argument types.
  {
    uint32_t rowCount = 0, colCount = 0;
    QualType elemType = {};
    if (isMxNMatrix(argType, &elemType, &rowCount, &colCount)) {
      const auto colType = astContext.getExtVectorType(elemType, colCount);
      llvm::SmallVector<SpirvInstruction *, 4> resultRows;
      for (uint32_t i = 0; i < rowCount; ++i) {
        auto *rowArg0 =
            spvBuilder.createCompositeExtract(colType, arg0Instr, {i}, arg0Loc);
        auto *rowArg1 =
            spvBuilder.createCompositeExtract(colType, arg1Instr, {i}, arg1Loc);
        auto *rowArg2 =
            spvBuilder.createCompositeExtract(colType, arg2Instr, {i}, arg2Loc);
        auto *mul = spvBuilder.createBinaryOp(spv::Op::OpIMul, colType, rowArg0,
                                              rowArg1, loc);
        auto *add = spvBuilder.createBinaryOp(spv::Op::OpIAdd, colType, mul,
                                              rowArg2, loc);
        spvBuilder.decorateNoContraction(mul, loc);
        spvBuilder.decorateNoContraction(add, loc);
        resultRows.push_back(add);
      }
      return spvBuilder.createCompositeConstruct(argType, resultRows, loc);
    }
  }

  emitError("invalid argument type passed to mad intrinsic function",
            callExpr->getExprLoc());
  return 0;
}

SpirvInstruction *SpirvEmitter::processIntrinsicLit(const CallExpr *callExpr) {
  // Signature is: float4 lit(float n_dot_l, float n_dot_h, float m)
  //
  // This function returns a lighting coefficient vector
  // (ambient, diffuse, specular, 1) where:
  // ambient  = 1.
  // diffuse  = (n_dot_l < 0) ? 0 : n_dot_l
  // specular = (n_dot_l < 0 || n_dot_h < 0) ? 0 : ((n_dot_h) * m)
  auto *glslInstSet = spvBuilder.getGLSLExtInstSet();
  auto *nDotL = doExpr(callExpr->getArg(0));
  auto *nDotH = doExpr(callExpr->getArg(1));
  auto *m = doExpr(callExpr->getArg(2));
  const auto loc = callExpr->getExprLoc();
  const QualType floatType = astContext.FloatTy;
  const QualType boolType = astContext.BoolTy;
  SpirvInstruction *floatZero =
      spvBuilder.getConstantFloat(astContext.FloatTy, llvm::APFloat(0.0f));
  SpirvInstruction *floatOne =
      spvBuilder.getConstantFloat(astContext.FloatTy, llvm::APFloat(1.0f));
  const QualType retType = callExpr->getType();
  auto *diffuse = spvBuilder.createExtInst(floatType, glslInstSet,
                                           GLSLstd450::GLSLstd450FMax,
                                           {floatZero, nDotL}, loc);
  auto *min = spvBuilder.createExtInst(
      floatType, glslInstSet, GLSLstd450::GLSLstd450FMin, {nDotL, nDotH}, loc);
  auto *isNeg = spvBuilder.createBinaryOp(spv::Op::OpFOrdLessThan, boolType,
                                          min, floatZero, loc);
  auto *mul =
      spvBuilder.createBinaryOp(spv::Op::OpFMul, floatType, nDotH, m, loc);
  auto *specular =
      spvBuilder.createSelect(floatType, isNeg, floatZero, mul, loc);
  return spvBuilder.createCompositeConstruct(
      retType, {floatOne, diffuse, specular, floatOne}, callExpr->getLocEnd());
}

SpirvInstruction *
SpirvEmitter::processIntrinsicFrexp(const CallExpr *callExpr) {
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

  auto *glslInstSet = spvBuilder.getGLSLExtInstSet();
  const Expr *arg = callExpr->getArg(0);
  const auto argType = arg->getType();
  const auto returnType = callExpr->getType();
  const auto loc = callExpr->getExprLoc();
  auto *argInstr = doExpr(arg);
  auto *expInstr = doExpr(callExpr->getArg(1));

  // For scalar and vector argument types.
  {
    uint32_t elemCount = 1;
    if (isScalarType(argType) || isVectorType(argType, nullptr, &elemCount)) {
      const QualType expType =
          elemCount == 1
              ? astContext.IntTy
              : astContext.getExtVectorType(astContext.IntTy, elemCount);
      const auto *frexpStructType = spvContext.getHybridStructType(
          {HybridStructType::FieldInfo(argType, "mantissa"),
           HybridStructType::FieldInfo(expType, "exponent")},
          "FrexpStructType");
      auto *frexp = spvBuilder.createExtInst(frexpStructType, glslInstSet,
                                             GLSLstd450::GLSLstd450FrexpStruct,
                                             {argInstr}, loc);
      auto *exponentInt =
          spvBuilder.createCompositeExtract(expType, frexp, {1}, loc);

      // Since the SPIR-V instruction returns an int, and the intrinsic HLSL
      // expects a float, an conversion must take place before writing the
      // results.
      auto *exponentFloat = spvBuilder.createUnaryOp(
          spv::Op::OpConvertSToF, returnType, exponentInt, loc);
      spvBuilder.createStore(expInstr, exponentFloat, loc);
      return spvBuilder.createCompositeExtract(argType, frexp, {0}, loc);
    }
  }

  // For matrix argument types.
  {
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(argType, nullptr, &rowCount, &colCount)) {
      const auto expType =
          astContext.getExtVectorType(astContext.IntTy, colCount);
      const auto colType =
          astContext.getExtVectorType(astContext.FloatTy, colCount);
      const auto *frexpStructType = spvContext.getHybridStructType(
          {HybridStructType::FieldInfo(colType, "mantissa"),
           HybridStructType::FieldInfo(expType, "exponent")},
          "FrexpStructType");
      llvm::SmallVector<SpirvInstruction *, 4> exponents;
      llvm::SmallVector<SpirvInstruction *, 4> mantissas;
      for (uint32_t i = 0; i < rowCount; ++i) {
        auto *curRow = spvBuilder.createCompositeExtract(colType, argInstr, {i},
                                                         arg->getLocStart());
        auto *frexp = spvBuilder.createExtInst(
            frexpStructType, glslInstSet, GLSLstd450::GLSLstd450FrexpStruct,
            {curRow}, loc);
        auto *exponentInt =
            spvBuilder.createCompositeExtract(expType, frexp, {1}, loc);

        // Since the SPIR-V instruction returns an int, and the intrinsic HLSL
        // expects a float, an conversion must take place before writing the
        // results.
        auto *exponentFloat = spvBuilder.createUnaryOp(
            spv::Op::OpConvertSToF, colType, exponentInt, loc);
        exponents.push_back(exponentFloat);
        mantissas.push_back(
            spvBuilder.createCompositeExtract(colType, frexp, {0}, loc));
      }
      auto *exponentsResult =
          spvBuilder.createCompositeConstruct(returnType, exponents, loc);
      spvBuilder.createStore(expInstr, exponentsResult, loc);
      return spvBuilder.createCompositeConstruct(returnType, mantissas,
                                                 callExpr->getLocEnd());
    }
  }

  emitError("invalid argument type passed to Frexp intrinsic function",
            callExpr->getExprLoc());
  return nullptr;
}

SpirvInstruction *
SpirvEmitter::processIntrinsicLdexp(const CallExpr *callExpr) {
  // Signature: ret ldexp(x, exp)
  // This function uses the following formula: x * 2^exp.
  // Note that we cannot use GLSL extended instruction Ldexp since it requires
  // the exponent to be an integer (vector) but HLSL takes an float (vector)
  // exponent. So we must calculate the result manually.
  auto *glsl = spvBuilder.getGLSLExtInstSet();
  const Expr *x = callExpr->getArg(0);
  const auto paramType = x->getType();
  auto *xInstr = doExpr(x);
  auto *expInstr = doExpr(callExpr->getArg(1));
  const auto loc = callExpr->getLocStart();
  const auto arg1Loc = callExpr->getArg(1)->getLocStart();

  // For scalar and vector argument types.
  if (isScalarType(paramType) || isVectorType(paramType)) {
    const auto twoExp = spvBuilder.createExtInst(
        paramType, glsl, GLSLstd450::GLSLstd450Exp2, {expInstr}, loc);
    return spvBuilder.createBinaryOp(spv::Op::OpFMul, paramType, xInstr, twoExp,
                                     loc);
  }

  // For matrix argument types.
  {
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(paramType, nullptr, &rowCount, &colCount)) {
      const auto actOnEachVec = [this, loc, glsl, expInstr,
                                 arg1Loc](uint32_t index, QualType vecType,
                                          SpirvInstruction *xRowInstr) {
        auto *expRowInstr = spvBuilder.createCompositeExtract(vecType, expInstr,
                                                              {index}, arg1Loc);
        auto *twoExp = spvBuilder.createExtInst(
            vecType, glsl, GLSLstd450::GLSLstd450Exp2, {expRowInstr}, loc);
        return spvBuilder.createBinaryOp(spv::Op::OpFMul, vecType, xRowInstr,
                                         twoExp, loc);
      };
      return processEachVectorInMatrix(x, xInstr, actOnEachVec, loc);
    }
  }

  emitError("invalid argument type passed to ldexp intrinsic function",
            callExpr->getExprLoc());
  return nullptr;
}

SpirvInstruction *SpirvEmitter::processIntrinsicDst(const CallExpr *callExpr) {
  // Signature is float4 dst(float4 src0, float4 src1)
  // result.x = 1;
  // result.y = src0.y * src1.y;
  // result.z = src0.z;
  // result.w = src1.w;
  const QualType f32 = astContext.FloatTy;
  auto *arg0Id = doExpr(callExpr->getArg(0));
  auto *arg1Id = doExpr(callExpr->getArg(1));
  auto arg0Loc = callExpr->getArg(0)->getLocStart();
  auto arg1Loc = callExpr->getArg(1)->getLocStart();
  auto *arg0y = spvBuilder.createCompositeExtract(f32, arg0Id, {1}, arg0Loc);
  auto *arg1y = spvBuilder.createCompositeExtract(f32, arg1Id, {1}, arg1Loc);
  auto *arg0z = spvBuilder.createCompositeExtract(f32, arg0Id, {2}, arg0Loc);
  auto *arg1w = spvBuilder.createCompositeExtract(f32, arg1Id, {3}, arg1Loc);
  auto loc = callExpr->getLocEnd();
  auto *arg0yMularg1y =
      spvBuilder.createBinaryOp(spv::Op::OpFMul, f32, arg0y, arg1y, loc);
  return spvBuilder.createCompositeConstruct(
      callExpr->getType(),
      {spvBuilder.getConstantFloat(astContext.FloatTy, llvm::APFloat(1.0f)),
       arg0yMularg1y, arg0z, arg1w},
      loc);
}

SpirvInstruction *SpirvEmitter::processIntrinsicClip(const CallExpr *callExpr) {
  // Discards the current pixel if the specified value is less than zero.
  // TODO: If the argument can be const folded and evaluated, we could
  // potentially avoid creating a branch. This would be a bit challenging for
  // matrix/vector arguments.

  assert(callExpr->getNumArgs() == 1u);
  const Expr *arg = callExpr->getArg(0);
  const auto loc = callExpr->getExprLoc();
  const auto argType = arg->getType();
  const auto boolType = astContext.BoolTy;
  SpirvInstruction *condition = nullptr;

  // Could not determine the argument as a constant. We need to branch based on
  // the argument. If the argument is a vector/matrix, clipping is done if *any*
  // element of the vector/matrix is less than zero.
  auto *argInstr = doExpr(arg);

  QualType elemType = {};
  uint32_t elemCount = 0, rowCount = 0, colCount = 0;
  if (isScalarType(argType)) {
    auto *zero = getValueZero(argType);
    condition = spvBuilder.createBinaryOp(spv::Op::OpFOrdLessThan, boolType,
                                          argInstr, zero, loc);
  } else if (isVectorType(argType, nullptr, &elemCount)) {
    auto *zero = getValueZero(argType);
    const QualType boolVecType =
        astContext.getExtVectorType(boolType, elemCount);
    auto *cmp = spvBuilder.createBinaryOp(spv::Op::OpFOrdLessThan, boolVecType,
                                          argInstr, zero, loc);
    condition = spvBuilder.createUnaryOp(spv::Op::OpAny, boolType, cmp, loc);
  } else if (isMxNMatrix(argType, &elemType, &rowCount, &colCount)) {
    const auto floatVecType = astContext.getExtVectorType(elemType, colCount);
    auto *elemZero = getValueZero(elemType);
    llvm::SmallVector<SpirvConstant *, 4> elements(size_t(colCount), elemZero);
    auto *zero = spvBuilder.getConstantComposite(floatVecType, elements);
    llvm::SmallVector<SpirvInstruction *, 4> cmpResults;
    for (uint32_t i = 0; i < rowCount; ++i) {
      auto *lhsVec =
          spvBuilder.createCompositeExtract(floatVecType, argInstr, {i}, loc);
      const auto boolColType = astContext.getExtVectorType(boolType, colCount);
      auto *cmp = spvBuilder.createBinaryOp(spv::Op::OpFOrdLessThan,
                                            boolColType, lhsVec, zero, loc);
      auto *any = spvBuilder.createUnaryOp(spv::Op::OpAny, boolType, cmp, loc);
      cmpResults.push_back(any);
    }
    const auto boolRowType = astContext.getExtVectorType(boolType, rowCount);
    auto *results =
        spvBuilder.createCompositeConstruct(boolRowType, cmpResults, loc);
    condition =
        spvBuilder.createUnaryOp(spv::Op::OpAny, boolType, results, loc);
  } else {
    emitError("invalid argument type passed to clip intrinsic function", loc);
    return nullptr;
  }

  // Then we need to emit the instruction for the conditional branch.
  auto *thenBB = spvBuilder.createBasicBlock("if.true");
  auto *mergeBB = spvBuilder.createBasicBlock("if.merge");
  // Create the branch instruction. This will end the current basic block.
  spvBuilder.createConditionalBranch(condition, thenBB, mergeBB, loc, mergeBB);
  spvBuilder.addSuccessor(thenBB);
  spvBuilder.addSuccessor(mergeBB);
  spvBuilder.setMergeTarget(mergeBB);
  // Handle the then branch
  spvBuilder.setInsertPoint(thenBB);
  spvBuilder.createKill(loc);
  spvBuilder.addSuccessor(mergeBB);
  // From now on, we'll emit instructions into the merge block.
  spvBuilder.setInsertPoint(mergeBB);
  return nullptr;
}

SpirvInstruction *
SpirvEmitter::processIntrinsicClamp(const CallExpr *callExpr) {
  // According the HLSL reference: clamp(X, Min, Max) takes 3 arguments. Each
  // one may be int, uint, or float.
  auto *glslInstSet = spvBuilder.getGLSLExtInstSet();
  const QualType returnType = callExpr->getType();
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
  const auto loc = callExpr->getExprLoc();
  auto *argXInstr = doExpr(argX);
  auto *argMinInstr = doExpr(argMin);
  auto *argMaxInstr = doExpr(argMax);
  const auto argMinLoc = argMin->getLocStart();
  const auto argMaxLoc = argMax->getLocStart();

  // FClamp, UClamp, and SClamp do not operate on matrices, so we should perform
  // the operation on each vector of the matrix.
  if (isMxNMatrix(argX->getType())) {
    const auto actOnEachVec = [this, loc, glslInstSet, glslOpcode, argMinInstr,
                               argMaxInstr, argMinLoc,
                               argMaxLoc](uint32_t index, QualType vecType,
                                          SpirvInstruction *curRow) {
      auto *minRowInstr = spvBuilder.createCompositeExtract(
          vecType, argMinInstr, {index}, argMinLoc);
      auto *maxRowInstr = spvBuilder.createCompositeExtract(
          vecType, argMaxInstr, {index}, argMaxLoc);
      return spvBuilder.createExtInst(vecType, glslInstSet, glslOpcode,
                                      {curRow, minRowInstr, maxRowInstr}, loc);
    };
    return processEachVectorInMatrix(argX, argXInstr, actOnEachVec, loc);
  }

  return spvBuilder.createExtInst(returnType, glslInstSet, glslOpcode,
                                  {argXInstr, argMinInstr, argMaxInstr}, loc);
}

SpirvInstruction *
SpirvEmitter::processIntrinsicMemoryBarrier(const CallExpr *callExpr,
                                            bool isDevice, bool groupSync,
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
      spv::MemorySemanticsMask::AcquireRelease;

  // Get <result-id> for execution scope.
  // If present, execution scope is always Workgroup!
  llvm::Optional<spv::Scope> execScope = llvm::None;
  if (groupSync) {
    execScope = spv::Scope::Workgroup;
  }

  // Get <result-id> for memory scope
  const spv::Scope memScope =
      (isDevice || isAllBarrier) ? spv::Scope::Device : spv::Scope::Workgroup;

  // Get <result-id> for memory semantics
  const auto memSemaMask = isAllBarrier ? allMemoryBarrierSema
                                        : isDevice ? deviceMemoryBarrierSema
                                                   : groupMemoryBarrierSema;
  spvBuilder.createBarrier(memScope, memSemaMask, execScope,
                           callExpr->getExprLoc());
  return nullptr;
}

SpirvInstruction *SpirvEmitter::processNonFpMatrixTranspose(
    QualType matType, SpirvInstruction *matrix, SourceLocation loc) {
  // Simplest way is to flatten the matrix construct a new matrix from the
  // flattened elements. (for a mat4x4).
  QualType elemType = {};
  uint32_t numRows = 0, numCols = 0;
  const bool isMat = isMxNMatrix(matType, &elemType, &numRows, &numCols);
  assert(isMat && !elemType->isFloatingType());
  (void)isMat;
  const auto colQualType = astContext.getExtVectorType(elemType, numRows);

  // You cannot perform a composite construct of an array using a few vectors.
  // The number of constutients passed to OpCompositeConstruct must be equal to
  // the number of array elements.
  llvm::SmallVector<SpirvInstruction *, 4> elems;
  for (uint32_t i = 0; i < numRows; ++i)
    for (uint32_t j = 0; j < numCols; ++j)
      elems.push_back(
          spvBuilder.createCompositeExtract(elemType, matrix, {i, j}, loc));

  llvm::SmallVector<SpirvInstruction *, 4> cols;
  for (uint32_t i = 0; i < numCols; ++i) {
    // The elements in the ith vector of the "transposed" array are at offset i,
    // i + <original-vector-size>, ...
    llvm::SmallVector<SpirvInstruction *, 4> indexes;
    for (uint32_t j = 0; j < numRows; ++j)
      indexes.push_back(elems[i + (j * numCols)]);

    cols.push_back(
        spvBuilder.createCompositeConstruct(colQualType, indexes, loc));
  }

  auto transposeType = astContext.getConstantArrayType(
      colQualType, llvm::APInt(32, numCols), clang::ArrayType::Normal, 0);
  return spvBuilder.createCompositeConstruct(transposeType, cols, loc);
}

SpirvInstruction *SpirvEmitter::processNonFpDot(SpirvInstruction *vec1Id,
                                                SpirvInstruction *vec2Id,
                                                uint32_t vecSize,
                                                QualType elemType,
                                                SourceLocation loc) {
  llvm::SmallVector<SpirvInstruction *, 4> muls;
  for (uint32_t i = 0; i < vecSize; ++i) {
    auto *elem1 = spvBuilder.createCompositeExtract(elemType, vec1Id, {i}, loc);
    auto *elem2 = spvBuilder.createCompositeExtract(elemType, vec2Id, {i}, loc);
    muls.push_back(spvBuilder.createBinaryOp(translateOp(BO_Mul, elemType),
                                             elemType, elem1, elem2, loc));
  }
  SpirvInstruction *sum = muls[0];
  for (uint32_t i = 1; i < vecSize; ++i) {
    sum = spvBuilder.createBinaryOp(translateOp(BO_Add, elemType), elemType,
                                    sum, muls[i], loc);
  }
  return sum;
}

SpirvInstruction *SpirvEmitter::processNonFpScalarTimesMatrix(
    QualType scalarType, SpirvInstruction *scalar, QualType matrixType,
    SpirvInstruction *matrix, SourceLocation loc) {
  assert(isScalarType(scalarType));
  QualType elemType = {};
  uint32_t numRows = 0, numCols = 0;
  const bool isMat = isMxNMatrix(matrixType, &elemType, &numRows, &numCols);
  assert(isMat);
  assert(isSameType(astContext, scalarType, elemType));
  (void)isMat;

  // We need to multiply the scalar by each vector of the matrix.
  // The front-end guarantees that the scalar and matrix element type are
  // the same. For example, if the scalar is a float, the matrix is casted
  // to a float matrix before being passed to mul(). It is also guaranteed
  // that types such as bool are casted to float or int before being
  // passed to mul().
  const auto rowType = astContext.getExtVectorType(elemType, numCols);
  llvm::SmallVector<SpirvInstruction *, 4> splat(size_t(numCols), scalar);
  auto *scalarSplat = spvBuilder.createCompositeConstruct(rowType, splat, loc);
  llvm::SmallVector<SpirvInstruction *, 4> mulRows;
  for (uint32_t row = 0; row < numRows; ++row) {
    auto *rowInstr =
        spvBuilder.createCompositeExtract(rowType, matrix, {row}, loc);
    mulRows.push_back(spvBuilder.createBinaryOp(
        translateOp(BO_Mul, scalarType), rowType, rowInstr, scalarSplat, loc));
  }
  return spvBuilder.createCompositeConstruct(matrixType, mulRows, loc);
}

SpirvInstruction *SpirvEmitter::processNonFpVectorTimesMatrix(
    QualType vecType, SpirvInstruction *vector, QualType matType,
    SpirvInstruction *matrix, SourceLocation loc,
    SpirvInstruction *matrixTranspose) {
  // This function assumes that the vector element type and matrix elemet type
  // are the same.
  QualType vecElemType = {}, matElemType = {};
  uint32_t vecSize = 0, numRows = 0, numCols = 0;
  const bool isVec = isVectorType(vecType, &vecElemType, &vecSize);
  const bool isMat = isMxNMatrix(matType, &matElemType, &numRows, &numCols);
  assert(isSameType(astContext, vecElemType, matElemType));
  assert(isVec);
  assert(isMat);
  assert(vecSize == numRows);
  (void)isVec;
  (void)isMat;

  // When processing vector times matrix, the vector is a row vector, and it
  // should be multiplied by the matrix *columns*. The most efficient way to
  // handle this in SPIR-V would be to first transpose the matrix, and then use
  // OpAccessChain.
  if (!matrixTranspose)
    matrixTranspose = processNonFpMatrixTranspose(matType, matrix, loc);

  llvm::SmallVector<SpirvInstruction *, 4> resultElems;
  for (uint32_t col = 0; col < numCols; ++col) {
    auto *colInstr =
        spvBuilder.createCompositeExtract(vecType, matrixTranspose, {col}, loc);
    resultElems.push_back(
        processNonFpDot(vector, colInstr, vecSize, vecElemType, loc));
  }
  return spvBuilder.createCompositeConstruct(
      astContext.getExtVectorType(vecElemType, numCols), resultElems, loc);
}

SpirvInstruction *SpirvEmitter::processNonFpMatrixTimesVector(
    QualType matType, SpirvInstruction *matrix, QualType vecType,
    SpirvInstruction *vector, SourceLocation loc) {
  // This function assumes that the vector element type and matrix elemet type
  // are the same.
  QualType vecElemType = {}, matElemType = {};
  uint32_t vecSize = 0, numRows = 0, numCols = 0;
  const bool isVec = isVectorType(vecType, &vecElemType, &vecSize);
  const bool isMat = isMxNMatrix(matType, &matElemType, &numRows, &numCols);
  assert(isSameType(astContext, vecElemType, matElemType));
  assert(isVec);
  assert(isMat);
  assert(vecSize == numCols);
  (void)isVec;
  (void)isMat;

  // When processing matrix times vector, the vector is a column vector. So we
  // simply get each row of the matrix and perform a dot product with the
  // vector.
  llvm::SmallVector<SpirvInstruction *, 4> resultElems;
  for (uint32_t row = 0; row < numRows; ++row) {
    auto *rowInstr =
        spvBuilder.createCompositeExtract(vecType, matrix, {row}, loc);
    resultElems.push_back(
        processNonFpDot(rowInstr, vector, vecSize, vecElemType, loc));
  }
  return spvBuilder.createCompositeConstruct(
      astContext.getExtVectorType(vecElemType, numRows), resultElems, loc);
}

SpirvInstruction *SpirvEmitter::processNonFpMatrixTimesMatrix(
    QualType lhsType, SpirvInstruction *lhs, QualType rhsType,
    SpirvInstruction *rhs, SourceLocation loc) {
  // This function assumes that the vector element type and matrix elemet type
  // are the same.
  QualType lhsElemType = {}, rhsElemType = {};
  uint32_t lhsNumRows = 0, lhsNumCols = 0;
  uint32_t rhsNumRows = 0, rhsNumCols = 0;
  const bool lhsIsMat =
      isMxNMatrix(lhsType, &lhsElemType, &lhsNumRows, &lhsNumCols);
  const bool rhsIsMat =
      isMxNMatrix(rhsType, &rhsElemType, &rhsNumRows, &rhsNumCols);
  assert(isSameType(astContext, lhsElemType, rhsElemType));
  assert(lhsIsMat && rhsIsMat);
  assert(lhsNumCols == rhsNumRows);
  (void)rhsIsMat;
  (void)lhsIsMat;

  auto *rhsTranspose = processNonFpMatrixTranspose(rhsType, rhs, loc);
  const auto vecType = astContext.getExtVectorType(lhsElemType, lhsNumCols);
  llvm::SmallVector<SpirvInstruction *, 4> resultRows;
  for (uint32_t row = 0; row < lhsNumRows; ++row) {
    auto *rowInstr =
        spvBuilder.createCompositeExtract(vecType, lhs, {row}, loc);
    resultRows.push_back(processNonFpVectorTimesMatrix(
        vecType, rowInstr, rhsType, rhs, loc, rhsTranspose));
  }

  // The resulting matrix will have 'lhsNumRows' rows and 'rhsNumCols' columns.
  const auto resultColType =
      astContext.getExtVectorType(lhsElemType, rhsNumCols);
  const auto resultType = astContext.getConstantArrayType(
      resultColType, llvm::APInt(32, lhsNumRows), clang::ArrayType::Normal, 0);
  return spvBuilder.createCompositeConstruct(resultType, resultRows, loc);
}

SpirvInstruction *SpirvEmitter::processIntrinsicMul(const CallExpr *callExpr) {
  const QualType returnType = callExpr->getType();

  // Get the function parameters. Expect 2 parameters.
  assert(callExpr->getNumArgs() == 2u);
  const Expr *arg0 = callExpr->getArg(0);
  const Expr *arg1 = callExpr->getArg(1);
  const QualType arg0Type = arg0->getType();
  const QualType arg1Type = arg1->getType();
  auto loc = callExpr->getExprLoc();

  // The HLSL mul() function takes 2 arguments. Each argument may be a scalar,
  // vector, or matrix. The frontend ensures that the two arguments have the
  // same component type. The only allowed component types are int and float.

  // mul(scalar, vector)
  {
    uint32_t elemCount = 0;
    if (isScalarType(arg0Type) && isVectorType(arg1Type, nullptr, &elemCount)) {

      auto *arg1Id = doExpr(arg1);

      // We can use OpVectorTimesScalar if arguments are floats.
      if (arg0Type->isFloatingType())
        return spvBuilder.createBinaryOp(spv::Op::OpVectorTimesScalar,
                                         returnType, arg1Id, doExpr(arg0), loc);

      // Use OpIMul for integers
      return spvBuilder.createBinaryOp(spv::Op::OpIMul, returnType,
                                       createVectorSplat(arg0, elemCount),
                                       arg1Id, loc);
    }
  }

  // mul(vector, scalar)
  {
    uint32_t elemCount = 0;
    if (isVectorType(arg0Type, nullptr, &elemCount) && isScalarType(arg1Type)) {

      auto *arg0Id = doExpr(arg0);

      // We can use OpVectorTimesScalar if arguments are floats.
      if (arg1Type->isFloatingType())
        return spvBuilder.createBinaryOp(spv::Op::OpVectorTimesScalar,
                                         returnType, arg0Id, doExpr(arg1), loc);

      // Use OpIMul for integers
      return spvBuilder.createBinaryOp(spv::Op::OpIMul, returnType, arg0Id,
                                       createVectorSplat(arg1, elemCount), loc);
    }
  }

  // mul(vector, vector)
  if (isVectorType(arg0Type) && isVectorType(arg1Type))
    return processIntrinsicDot(callExpr);

  // All the following cases require handling arg0 and arg1 expressions first.
  auto *arg0Id = doExpr(arg0);
  auto *arg1Id = doExpr(arg1);

  // mul(scalar, scalar)
  if (isScalarType(arg0Type) && isScalarType(arg1Type))
    return spvBuilder.createBinaryOp(translateOp(BO_Mul, arg0Type), returnType,
                                     arg0Id, arg1Id, loc);

  // mul(scalar, matrix)
  {
    QualType elemType = {};
    if (isScalarType(arg0Type) && isMxNMatrix(arg1Type, &elemType)) {
      // OpMatrixTimesScalar can only be used if *both* the matrix element type
      // and the scalar type are float.
      if (arg0Type->isFloatingType() && elemType->isFloatingType())
        return spvBuilder.createBinaryOp(spv::Op::OpMatrixTimesScalar,
                                         returnType, arg1Id, arg0Id, loc);
      else
        return processNonFpScalarTimesMatrix(arg0Type, arg0Id, arg1Type, arg1Id,
                                             callExpr->getExprLoc());
    }
  }

  // mul(matrix, scalar)
  {
    QualType elemType = {};
    if (isScalarType(arg1Type) && isMxNMatrix(arg0Type, &elemType)) {
      // OpMatrixTimesScalar can only be used if *both* the matrix element type
      // and the scalar type are float.
      if (arg1Type->isFloatingType() && elemType->isFloatingType())
        return spvBuilder.createBinaryOp(spv::Op::OpMatrixTimesScalar,
                                         returnType, arg0Id, arg1Id, loc);
      else
        return processNonFpScalarTimesMatrix(arg1Type, arg1Id, arg0Type, arg0Id,
                                             callExpr->getExprLoc());
    }
  }

  // mul(vector, matrix)
  {
    QualType vecElemType = {}, matElemType = {};
    uint32_t elemCount = 0, numRows = 0;
    if (isVectorType(arg0Type, &vecElemType, &elemCount) &&
        isMxNMatrix(arg1Type, &matElemType, &numRows)) {
      assert(elemCount == numRows);

      if (vecElemType->isFloatingType() && matElemType->isFloatingType())
        return spvBuilder.createBinaryOp(spv::Op::OpMatrixTimesVector,
                                         returnType, arg1Id, arg0Id, loc);
      else
        return processNonFpVectorTimesMatrix(arg0Type, arg0Id, arg1Type, arg1Id,
                                             callExpr->getExprLoc());
    }
  }

  // mul(matrix, vector)
  {
    QualType vecElemType = {}, matElemType = {};
    uint32_t elemCount = 0, numCols = 0;
    if (isMxNMatrix(arg0Type, &matElemType, nullptr, &numCols) &&
        isVectorType(arg1Type, &vecElemType, &elemCount)) {
      assert(elemCount == numCols);
      if (vecElemType->isFloatingType() && matElemType->isFloatingType())
        return spvBuilder.createBinaryOp(spv::Op::OpVectorTimesMatrix,
                                         returnType, arg1Id, arg0Id, loc);
      else
        return processNonFpMatrixTimesVector(arg0Type, arg0Id, arg1Type, arg1Id,
                                             callExpr->getExprLoc());
    }
  }

  // mul(matrix, matrix)
  {
    // The front-end ensures that the two matrix element types match.
    QualType elemType = {};
    uint32_t lhsCols = 0, rhsRows = 0;
    if (isMxNMatrix(arg0Type, &elemType, nullptr, &lhsCols) &&
        isMxNMatrix(arg1Type, nullptr, &rhsRows, nullptr)) {
      assert(lhsCols == rhsRows);
      if (elemType->isFloatingType())
        return spvBuilder.createBinaryOp(spv::Op::OpMatrixTimesMatrix,
                                         returnType, arg1Id, arg0Id, loc);
      else
        return processNonFpMatrixTimesMatrix(arg0Type, arg0Id, arg1Type, arg1Id,
                                             callExpr->getExprLoc());
    }
  }

  emitError("invalid argument type passed to mul intrinsic function",
            callExpr->getExprLoc());
  return nullptr;
}

SpirvInstruction *SpirvEmitter::processIntrinsicDot(const CallExpr *callExpr) {
  const QualType returnType = callExpr->getType();

  // Get the function parameters. Expect 2 vectors as parameters.
  assert(callExpr->getNumArgs() == 2u);
  const Expr *arg0 = callExpr->getArg(0);
  const Expr *arg1 = callExpr->getArg(1);
  auto *arg0Id = doExpr(arg0);
  auto *arg1Id = doExpr(arg1);
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

  auto loc = callExpr->getLocStart();

  // According to HLSL reference, the dot function only works on integers
  // and floats.
  assert(returnType->isFloatingType() || returnType->isIntegerType());

  // Special case: dot product of two vectors, each of size 1. That is
  // basically the same as regular multiplication of 2 scalars.
  if (vec0Size == 1) {
    const spv::Op spvOp = translateOp(BO_Mul, arg0Type);
    return spvBuilder.createBinaryOp(spvOp, returnType, arg0Id, arg1Id, loc);
  }

  // If the vectors are of type Float, we can use OpDot.
  if (returnType->isFloatingType()) {
    return spvBuilder.createBinaryOp(spv::Op::OpDot, returnType, arg0Id, arg1Id,
                                     loc);
  }
  // Vector component type is Integer (signed or unsigned).
  // Create all instructions necessary to perform a dot product on
  // two integer vectors. SPIR-V OpDot does not support integer vectors.
  // Therefore, we use other SPIR-V instructions (addition and
  // multiplication).
  else {
    SpirvInstruction *result = nullptr;
    llvm::SmallVector<SpirvInstruction *, 4> multIds;
    const spv::Op multSpvOp = translateOp(BO_Mul, arg0Type);
    const spv::Op addSpvOp = translateOp(BO_Add, arg0Type);

    // Extract members from the two vectors and multiply them.
    for (unsigned int i = 0; i < vec0Size; ++i) {
      auto *vec0member = spvBuilder.createCompositeExtract(
          returnType, arg0Id, {i}, arg0->getLocStart());
      auto *vec1member = spvBuilder.createCompositeExtract(
          returnType, arg1Id, {i}, arg1->getLocStart());
      auto *multId = spvBuilder.createBinaryOp(multSpvOp, returnType,
                                               vec0member, vec1member, loc);
      multIds.push_back(multId);
    }
    // Add all the multiplications.
    result = multIds[0];
    for (unsigned int i = 1; i < vec0Size; ++i) {
      auto *additionId = spvBuilder.createBinaryOp(addSpvOp, returnType, result,
                                                   multIds[i], loc);
      result = additionId;
    }
    return result;
  }
}

SpirvInstruction *SpirvEmitter::processIntrinsicRcp(const CallExpr *callExpr) {
  // 'rcp' takes only 1 argument that is a scalar, vector, or matrix of type
  // float or double.
  assert(callExpr->getNumArgs() == 1u);
  const QualType returnType = callExpr->getType();
  const Expr *arg = callExpr->getArg(0);
  auto *argId = doExpr(arg);
  const QualType argType = arg->getType();
  auto loc = callExpr->getLocStart();

  // For cases with matrix argument.
  QualType elemType = {};
  uint32_t numRows = 0, numCols = 0;
  if (isMxNMatrix(argType, &elemType, &numRows, &numCols)) {
    auto *vecOne = getVecValueOne(elemType, numCols);
    const auto actOnEachVec = [this, vecOne, loc](uint32_t /*index*/,
                                                  QualType vecType,
                                                  SpirvInstruction *curRow) {
      return spvBuilder.createBinaryOp(spv::Op::OpFDiv, vecType, vecOne, curRow,
                                       loc);
    };
    return processEachVectorInMatrix(arg, argId, actOnEachVec, loc);
  }

  // For cases with scalar or vector arguments.
  return spvBuilder.createBinaryOp(spv::Op::OpFDiv, returnType,
                                   getValueOne(argType), argId, loc);
}

SpirvInstruction *
SpirvEmitter::processIntrinsicAllOrAny(const CallExpr *callExpr,
                                       spv::Op spvOp) {
  // 'all' and 'any' take only 1 parameter.
  assert(callExpr->getNumArgs() == 1u);
  const QualType returnType = callExpr->getType();
  const Expr *arg = callExpr->getArg(0);
  const QualType argType = arg->getType();
  const auto loc = callExpr->getExprLoc();

  // Handle scalars, vectors of size 1, and 1x1 matrices as arguments.
  // Optimization: can directly cast them to boolean. No need for OpAny/OpAll.
  {
    QualType scalarType = {};
    if (isScalarType(argType, &scalarType) &&
        (scalarType->isBooleanType() || scalarType->isFloatingType() ||
         scalarType->isIntegerType()))
      return castToBool(doExpr(arg), argType, returnType, loc);
  }

  // Handle vectors larger than 1, Mx1 matrices, and 1xN matrices as arguments.
  // Cast the vector to a boolean vector, then run OpAny/OpAll on it.
  {
    QualType elemType = {};
    uint32_t size = 0;
    if (isVectorType(argType, &elemType, &size)) {
      const QualType castToBoolType =
          astContext.getExtVectorType(returnType, size);
      auto *castedToBool =
          castToBool(doExpr(arg), argType, castToBoolType, loc);
      return spvBuilder.createUnaryOp(spvOp, returnType, castedToBool, loc);
    }
  }

  // Handle MxN matrices as arguments.
  {
    QualType elemType = {};
    uint32_t matRowCount = 0, matColCount = 0;
    if (isMxNMatrix(argType, &elemType, &matRowCount, &matColCount)) {
      auto *matrix = doExpr(arg);
      const QualType vecType = getComponentVectorType(astContext, argType);
      llvm::SmallVector<SpirvInstruction *, 4> rowResults;
      for (uint32_t i = 0; i < matRowCount; ++i) {
        // Extract the row which is a float vector of size matColCount.
        auto *rowFloatVec = spvBuilder.createCompositeExtract(
            vecType, matrix, {i}, arg->getLocStart());
        // Cast the float vector to boolean vector.
        const auto rowFloatQualType =
            astContext.getExtVectorType(elemType, matColCount);
        const auto rowBoolQualType =
            astContext.getExtVectorType(returnType, matColCount);
        auto *rowBoolVec = castToBool(rowFloatVec, rowFloatQualType,
                                      rowBoolQualType, arg->getLocStart());
        // Perform OpAny/OpAll on the boolean vector.
        rowResults.push_back(
            spvBuilder.createUnaryOp(spvOp, returnType, rowBoolVec, loc));
      }
      // Create a new vector that is the concatenation of results of all rows.
      const QualType vecOfBools =
          astContext.getExtVectorType(astContext.BoolTy, matRowCount);
      auto *row =
          spvBuilder.createCompositeConstruct(vecOfBools, rowResults, loc);

      // Run OpAny/OpAll on the newly-created vector.
      return spvBuilder.createUnaryOp(spvOp, returnType, row, loc);
    }
  }

  // All types should be handled already.
  llvm_unreachable("Unknown argument type passed to all()/any().");
  return nullptr;
}

SpirvInstruction *
SpirvEmitter::processIntrinsicAsType(const CallExpr *callExpr) {
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
  const Expr *arg0 = callExpr->getArg(0);
  const QualType argType = arg0->getType();
  const auto loc = callExpr->getExprLoc();

  // Method 3 return type may be the same as arg type, so it would be a no-op.
  if (isSameType(astContext, returnType, argType))
    return doExpr(arg0);

  switch (numArgs) {
  case 1: {
    // Handling Method 1, 2, and 3.
    auto *argInstr = doExpr(arg0);
    QualType fromElemType = {};
    uint32_t numRows = 0, numCols = 0;
    // For non-matrix arguments (scalar or vector), just do an OpBitCast.
    if (!isMxNMatrix(argType, &fromElemType, &numRows, &numCols)) {
      return spvBuilder.createUnaryOp(spv::Op::OpBitcast, returnType, argInstr,
                                      loc);
    }

    // Input or output type is a matrix.
    const QualType toElemType = hlsl::GetHLSLMatElementType(returnType);
    llvm::SmallVector<SpirvInstruction *, 4> castedRows;
    const auto fromVecType = astContext.getExtVectorType(fromElemType, numCols);
    const auto toVecType = astContext.getExtVectorType(toElemType, numCols);
    for (uint32_t row = 0; row < numRows; ++row) {
      auto *rowInstr = spvBuilder.createCompositeExtract(
          fromVecType, argInstr, {row}, arg0->getLocStart());
      castedRows.push_back(spvBuilder.createUnaryOp(spv::Op::OpBitcast,
                                                    toVecType, rowInstr, loc));
    }
    return spvBuilder.createCompositeConstruct(returnType, castedRows, loc);
  }
  case 2: {
    auto *lowbits = doExpr(arg0);
    auto *highbits = doExpr(callExpr->getArg(1));
    const auto uintType = astContext.UnsignedIntTy;
    const auto doubleType = astContext.DoubleTy;
    // Handling Method 4
    if (argType->isUnsignedIntegerType()) {
      const auto uintVec2Type = astContext.getExtVectorType(uintType, 2);
      auto *operand = spvBuilder.createCompositeConstruct(
          uintVec2Type, {lowbits, highbits}, loc);
      return spvBuilder.createUnaryOp(spv::Op::OpBitcast, doubleType, operand,
                                      loc);
    }
    // Handling Method 5
    else {
      const auto uintVec4Type = astContext.getExtVectorType(uintType, 4);
      const auto doubleVec2Type = astContext.getExtVectorType(doubleType, 2);
      auto *operand = spvBuilder.createVectorShuffle(
          uintVec4Type, lowbits, highbits, {0, 2, 1, 3}, loc);
      return spvBuilder.createUnaryOp(spv::Op::OpBitcast, doubleVec2Type,
                                      operand, loc);
    }
  }
  case 3: {
    // Handling Method 6.
    auto *value = doExpr(arg0);
    auto *lowbits = doExpr(callExpr->getArg(1));
    auto *highbits = doExpr(callExpr->getArg(2));
    const auto uintType = astContext.UnsignedIntTy;
    const auto uintVec2Type = astContext.getExtVectorType(uintType, 2);
    auto *vecResult =
        spvBuilder.createUnaryOp(spv::Op::OpBitcast, uintVec2Type, value, loc);
    spvBuilder.createStore(lowbits,
                           spvBuilder.createCompositeExtract(
                               uintType, vecResult, {0}, arg0->getLocStart()),
                           loc);
    spvBuilder.createStore(highbits,
                           spvBuilder.createCompositeExtract(
                               uintType, vecResult, {1}, arg0->getLocStart()),
                           loc);
    return nullptr;
  }
  default:
    emitError("unrecognized signature for %0 intrinsic function", loc)
        << callExpr->getDirectCallee()->getName();
    return nullptr;
  }
}

SpirvInstruction *
SpirvEmitter::processD3DCOLORtoUBYTE4(const CallExpr *callExpr) {
  // Should take a float4 and return an int4 by doing:
  // int4 result = input.zyxw * 255.001953;
  // Maximum float precision makes the scaling factor 255.002.
  const auto arg = callExpr->getArg(0);
  auto *argId = doExpr(arg);
  const auto argType = arg->getType();
  auto loc = callExpr->getLocStart();
  auto *swizzle =
      spvBuilder.createVectorShuffle(argType, argId, argId, {2, 1, 0, 3}, loc);
  auto *scaled = spvBuilder.createBinaryOp(
      spv::Op::OpVectorTimesScalar, argType, swizzle,
      spvBuilder.getConstantFloat(astContext.FloatTy, llvm::APFloat(255.002f)),
      loc);
  return castToInt(scaled, arg->getType(), callExpr->getType(), loc);
}

SpirvInstruction *
SpirvEmitter::processIntrinsicIsFinite(const CallExpr *callExpr) {
  // Since OpIsFinite needs the Kernel capability, translation is instead done
  // using OpIsNan and OpIsInf:
  // isFinite = !(isNan || isInf)
  const auto arg = doExpr(callExpr->getArg(0));
  const auto returnType = callExpr->getType();
  const auto loc = callExpr->getExprLoc();
  const auto isNan =
      spvBuilder.createUnaryOp(spv::Op::OpIsNan, returnType, arg, loc);
  const auto isInf =
      spvBuilder.createUnaryOp(spv::Op::OpIsInf, returnType, arg, loc);
  const auto isNanOrInf = spvBuilder.createBinaryOp(
      spv::Op::OpLogicalOr, returnType, isNan, isInf, loc);
  return spvBuilder.createUnaryOp(spv::Op::OpLogicalNot, returnType, isNanOrInf,
                                  loc);
}

SpirvInstruction *
SpirvEmitter::processIntrinsicSinCos(const CallExpr *callExpr) {
  // Since there is no sincos equivalent in SPIR-V, we need to perform Sin
  // once and Cos once. We can reuse existing Sine/Cosine handling functions.
  CallExpr *sincosExpr =
      new (astContext) CallExpr(astContext, Stmt::StmtClass::NoStmtClass, {});
  sincosExpr->setType(callExpr->getArg(0)->getType());
  sincosExpr->setNumArgs(astContext, 1);
  sincosExpr->setArg(0, const_cast<Expr *>(callExpr->getArg(0)));
  const auto srcLoc = callExpr->getExprLoc();

  // Perform Sin and store results in argument 1.
  auto *sin =
      processIntrinsicUsingGLSLInst(sincosExpr, GLSLstd450::GLSLstd450Sin,
                                    /*actPerRowForMatrices*/ true, srcLoc);
  spvBuilder.createStore(doExpr(callExpr->getArg(1)), sin, srcLoc);

  // Perform Cos and store results in argument 2.
  auto *cos =
      processIntrinsicUsingGLSLInst(sincosExpr, GLSLstd450::GLSLstd450Cos,
                                    /*actPerRowForMatrices*/ true, srcLoc);
  spvBuilder.createStore(doExpr(callExpr->getArg(2)), cos, srcLoc);
  return nullptr;
}

SpirvInstruction *
SpirvEmitter::processIntrinsicSaturate(const CallExpr *callExpr) {
  const auto *arg = callExpr->getArg(0);
  const auto loc = callExpr->getExprLoc();
  auto *argId = doExpr(arg);
  const auto argType = arg->getType();
  const QualType returnType = callExpr->getType();
  auto *glslInstSet = spvBuilder.getGLSLExtInstSet();

  QualType elemType = {};
  uint32_t vecSize = 0;
  if (isScalarType(argType, &elemType)) {
    auto *floatZero = getValueZero(elemType);
    auto *floatOne = getValueOne(elemType);
    return spvBuilder.createExtInst(returnType, glslInstSet,
                                    GLSLstd450::GLSLstd450FClamp,
                                    {argId, floatZero, floatOne}, loc);
  }

  if (isVectorType(argType, &elemType, &vecSize)) {
    auto *vecZero = getVecValueZero(elemType, vecSize);
    auto *vecOne = getVecValueOne(elemType, vecSize);
    return spvBuilder.createExtInst(returnType, glslInstSet,
                                    GLSLstd450::GLSLstd450FClamp,
                                    {argId, vecZero, vecOne}, loc);
  }

  uint32_t numRows = 0, numCols = 0;
  if (isMxNMatrix(argType, &elemType, &numRows, &numCols)) {
    auto *vecZero = getVecValueZero(elemType, numCols);
    auto *vecOne = getVecValueOne(elemType, numCols);
    const auto actOnEachVec = [this, loc, vecZero, vecOne, glslInstSet](
                                  uint32_t /*index*/, QualType vecType,
                                  SpirvInstruction *curRow) {
      return spvBuilder.createExtInst(vecType, glslInstSet,
                                      GLSLstd450::GLSLstd450FClamp,
                                      {curRow, vecZero, vecOne}, loc);
    };
    return processEachVectorInMatrix(arg, argId, actOnEachVec, loc);
  }

  emitError("invalid argument type passed to saturate intrinsic function",
            callExpr->getExprLoc());
  return nullptr;
}

SpirvInstruction *
SpirvEmitter::processIntrinsicFloatSign(const CallExpr *callExpr) {
  // Import the GLSL.std.450 extended instruction set.
  auto *glslInstSet = spvBuilder.getGLSLExtInstSet();
  const Expr *arg = callExpr->getArg(0);
  const auto loc = callExpr->getExprLoc();
  const QualType returnType = callExpr->getType();
  const QualType argType = arg->getType();
  assert(isFloatOrVecMatOfFloatType(argType));
  auto *argId = doExpr(arg);
  SpirvInstruction *floatSign = nullptr;

  // For matrices, we can perform the instruction on each vector of the matrix.
  if (isMxNMatrix(argType)) {
    const auto actOnEachVec =
        [this, loc, glslInstSet](uint32_t /*index*/, QualType vecType,
                                 SpirvInstruction *curRow) {
          return spvBuilder.createExtInst(
              vecType, glslInstSet, GLSLstd450::GLSLstd450FSign, {curRow}, loc);
        };
    floatSign = processEachVectorInMatrix(arg, argId, actOnEachVec, loc);
  } else {
    floatSign = spvBuilder.createExtInst(
        argType, glslInstSet, GLSLstd450::GLSLstd450FSign, {argId}, loc);
  }

  return castToInt(floatSign, arg->getType(), returnType, arg->getLocStart());
}

SpirvInstruction *
SpirvEmitter::processIntrinsicF16ToF32(const CallExpr *callExpr) {
  // f16tof32() takes in (vector of) uint and returns (vector of) float.
  // The frontend should guarantee that by inserting implicit casts.
  auto *glsl = spvBuilder.getGLSLExtInstSet();
  const QualType f32Type = astContext.FloatTy;
  const QualType u32Type = astContext.UnsignedIntTy;
  const QualType v2f32Type = astContext.getExtVectorType(f32Type, 2);

  const auto loc = callExpr->getExprLoc();
  const auto *arg = callExpr->getArg(0);
  auto *argId = doExpr(arg);

  uint32_t elemCount = {};

  if (isVectorType(arg->getType(), nullptr, &elemCount)) {
    // The input is a vector. We need to handle each element separately.
    llvm::SmallVector<SpirvInstruction *, 4> elements;

    for (uint32_t i = 0; i < elemCount; ++i) {
      auto *srcElem = spvBuilder.createCompositeExtract(u32Type, argId, {i},
                                                        arg->getLocStart());
      auto *convert = spvBuilder.createExtInst(
          v2f32Type, glsl, GLSLstd450::GLSLstd450UnpackHalf2x16, srcElem, loc);
      elements.push_back(
          spvBuilder.createCompositeExtract(f32Type, convert, {0}, loc));
    }
    return spvBuilder.createCompositeConstruct(
        astContext.getExtVectorType(f32Type, elemCount), elements, loc);
  }

  auto *convert = spvBuilder.createExtInst(
      v2f32Type, glsl, GLSLstd450::GLSLstd450UnpackHalf2x16, argId, loc);
  // f16tof32() converts the float16 stored in the low-half of the uint to
  // a float. So just need to return the first component.
  return spvBuilder.createCompositeExtract(f32Type, convert, {0}, loc);
}

SpirvInstruction *
SpirvEmitter::processIntrinsicF32ToF16(const CallExpr *callExpr) {
  // f32tof16() takes in (vector of) float and returns (vector of) uint.
  // The frontend should guarantee that by inserting implicit casts.
  auto *glsl = spvBuilder.getGLSLExtInstSet();
  const QualType f32Type = astContext.FloatTy;
  const QualType u32Type = astContext.UnsignedIntTy;
  const QualType v2f32Type = astContext.getExtVectorType(f32Type, 2);
  auto *zero = spvBuilder.getConstantFloat(f32Type, llvm::APFloat(0.0f));

  const auto loc = callExpr->getExprLoc();
  const auto *arg = callExpr->getArg(0);
  auto *argId = doExpr(arg);
  uint32_t elemCount = {};

  if (isVectorType(arg->getType(), nullptr, &elemCount)) {
    // The input is a vector. We need to handle each element separately.
    llvm::SmallVector<SpirvInstruction *, 4> elements;

    for (uint32_t i = 0; i < elemCount; ++i) {
      auto *srcElem = spvBuilder.createCompositeExtract(f32Type, argId, {i},
                                                        arg->getLocStart());
      auto *srcVec =
          spvBuilder.createCompositeConstruct(v2f32Type, {srcElem, zero}, loc);

      elements.push_back(spvBuilder.createExtInst(
          u32Type, glsl, GLSLstd450::GLSLstd450PackHalf2x16, srcVec, loc));
    }
    return spvBuilder.createCompositeConstruct(
        astContext.getExtVectorType(u32Type, elemCount), elements, loc);
  }

  // f16tof32() stores the float into the low-half of the uint. So we need
  // to supply another zero to take the other half.
  auto *srcVec =
      spvBuilder.createCompositeConstruct(v2f32Type, {argId, zero}, loc);
  return spvBuilder.createExtInst(
      u32Type, glsl, GLSLstd450::GLSLstd450PackHalf2x16, srcVec, loc);
}

SpirvInstruction *SpirvEmitter::processIntrinsicUsingSpirvInst(
    const CallExpr *callExpr, spv::Op opcode, bool actPerRowForMatrices) {
  // Certain opcodes are only allowed in pixel shader
  if (!spvContext.isPS())
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

  const auto loc = callExpr->getExprLoc();
  const QualType returnType = callExpr->getType();
  if (callExpr->getNumArgs() == 1u) {
    const Expr *arg = callExpr->getArg(0);
    auto *argId = doExpr(arg);

    // If the instruction does not operate on matrices, we can perform the
    // instruction on each vector of the matrix.
    if (actPerRowForMatrices && isMxNMatrix(arg->getType())) {
      const auto actOnEachVec = [this, opcode, loc](uint32_t /*index*/,
                                                    QualType vecType,
                                                    SpirvInstruction *curRow) {
        return spvBuilder.createUnaryOp(opcode, vecType, curRow, loc);
      };
      return processEachVectorInMatrix(arg, argId, actOnEachVec, loc);
    }
    return spvBuilder.createUnaryOp(opcode, returnType, argId, loc);
  } else if (callExpr->getNumArgs() == 2u) {
    const Expr *arg0 = callExpr->getArg(0);
    auto *arg0Id = doExpr(arg0);
    auto *arg1Id = doExpr(callExpr->getArg(1));
    const auto arg1Loc = callExpr->getArg(1)->getLocStart();
    // If the instruction does not operate on matrices, we can perform the
    // instruction on each vector of the matrix.
    if (actPerRowForMatrices && isMxNMatrix(arg0->getType())) {
      const auto actOnEachVec = [this, opcode, arg1Id, loc,
                                 arg1Loc](uint32_t index, QualType vecType,
                                          SpirvInstruction *arg0Row) {
        auto *arg1Row = spvBuilder.createCompositeExtract(vecType, arg1Id,
                                                          {index}, arg1Loc);
        return spvBuilder.createBinaryOp(opcode, vecType, arg0Row, arg1Row,
                                         loc);
      };
      return processEachVectorInMatrix(arg0, arg0Id, actOnEachVec, loc);
    }
    return spvBuilder.createBinaryOp(opcode, returnType, arg0Id, arg1Id, loc);
  }

  emitError("unsupported %0 intrinsic function", loc)
      << cast<DeclRefExpr>(callExpr->getCallee())->getNameInfo().getAsString();
  return nullptr;
}

SpirvInstruction *SpirvEmitter::processIntrinsicUsingGLSLInst(
    const CallExpr *callExpr, GLSLstd450 opcode, bool actPerRowForMatrices,
    SourceLocation loc) {
  // Import the GLSL.std.450 extended instruction set.
  auto *glslInstSet = spvBuilder.getGLSLExtInstSet();
  const QualType returnType = callExpr->getType();

  if (callExpr->getNumArgs() == 1u) {
    const Expr *arg = callExpr->getArg(0);
    auto *argInstr = doExpr(arg);

    // If the instruction does not operate on matrices, we can perform the
    // instruction on each vector of the matrix.
    if (actPerRowForMatrices && isMxNMatrix(arg->getType())) {
      const auto actOnEachVec = [this, loc, glslInstSet,
                                 opcode](uint32_t /*index*/, QualType vecType,
                                         SpirvInstruction *curRowInstr) {
        return spvBuilder.createExtInst(vecType, glslInstSet, opcode,
                                        {curRowInstr}, loc);
      };
      return processEachVectorInMatrix(arg, argInstr, actOnEachVec, loc);
    }
    return spvBuilder.createExtInst(returnType, glslInstSet, opcode, {argInstr},
                                    loc);
  } else if (callExpr->getNumArgs() == 2u) {
    const Expr *arg0 = callExpr->getArg(0);
    auto *arg0Instr = doExpr(arg0);
    auto *arg1Instr = doExpr(callExpr->getArg(1));
    const auto arg1Loc = callExpr->getArg(1)->getLocStart();
    // If the instruction does not operate on matrices, we can perform the
    // instruction on each vector of the matrix.
    if (actPerRowForMatrices && isMxNMatrix(arg0->getType())) {
      const auto actOnEachVec = [this, loc, glslInstSet, opcode, arg1Instr,
                                 arg1Loc](uint32_t index, QualType vecType,
                                          SpirvInstruction *arg0RowInstr) {
        auto *arg1RowInstr = spvBuilder.createCompositeExtract(
            vecType, arg1Instr, {index}, arg1Loc);
        return spvBuilder.createExtInst(vecType, glslInstSet, opcode,
                                        {arg0RowInstr, arg1RowInstr}, loc);
      };
      return processEachVectorInMatrix(arg0, arg0Instr, actOnEachVec, loc);
    }
    return spvBuilder.createExtInst(returnType, glslInstSet, opcode,
                                    {arg0Instr, arg1Instr}, loc);
  } else if (callExpr->getNumArgs() == 3u) {
    const Expr *arg0 = callExpr->getArg(0);
    auto *arg0Instr = doExpr(arg0);
    auto *arg1Instr = doExpr(callExpr->getArg(1));
    auto *arg2Instr = doExpr(callExpr->getArg(2));
    auto arg1Loc = callExpr->getArg(1)->getLocStart();
    auto arg2Loc = callExpr->getArg(2)->getLocStart();
    // If the instruction does not operate on matrices, we can perform the
    // instruction on each vector of the matrix.
    if (actPerRowForMatrices && isMxNMatrix(arg0->getType())) {
      const auto actOnEachVec = [this, loc, glslInstSet, opcode, arg1Instr,
                                 arg2Instr, arg1Loc,
                                 arg2Loc](uint32_t index, QualType vecType,
                                          SpirvInstruction *arg0RowInstr) {
        auto *arg1RowInstr = spvBuilder.createCompositeExtract(
            vecType, arg1Instr, {index}, arg1Loc);
        auto *arg2RowInstr = spvBuilder.createCompositeExtract(
            vecType, arg2Instr, {index}, arg2Loc);
        return spvBuilder.createExtInst(
            vecType, glslInstSet, opcode,
            {arg0RowInstr, arg1RowInstr, arg2RowInstr}, loc);
      };
      return processEachVectorInMatrix(arg0, arg0Instr, actOnEachVec, loc);
    }
    return spvBuilder.createExtInst(returnType, glslInstSet, opcode,
                                    {arg0Instr, arg1Instr, arg2Instr}, loc);
  }

  emitError("unsupported %0 intrinsic function", callExpr->getExprLoc())
      << cast<DeclRefExpr>(callExpr->getCallee())->getNameInfo().getAsString();
  return nullptr;
}

SpirvInstruction *
SpirvEmitter::processIntrinsicLog10(const CallExpr *callExpr) {
  // Since there is no log10 instruction in SPIR-V, we can use:
  // log10(x) = log2(x) * ( 1 / log2(10) )
  // 1 / log2(10) = 0.30103
  auto loc = callExpr->getExprLoc();
  auto *scale =
      spvBuilder.getConstantFloat(astContext.FloatTy, llvm::APFloat(0.30103f));
  auto *log2 = processIntrinsicUsingGLSLInst(
      callExpr, GLSLstd450::GLSLstd450Log2, true, loc);
  const auto returnType = callExpr->getType();
  spv::Op scaleOp = isScalarType(returnType)
                        ? spv::Op::OpFMul
                        : isVectorType(returnType)
                              ? spv::Op::OpVectorTimesScalar
                              : spv::Op::OpMatrixTimesScalar;
  return spvBuilder.createBinaryOp(scaleOp, returnType, log2, scale, loc);
}

SpirvInstruction *SpirvEmitter::processRayBuiltins(const CallExpr *callExpr,
                                                   hlsl::IntrinsicOp op) {
  spv::BuiltIn builtin = spv::BuiltIn::Max;
  bool transposeMatrix = false;
  const auto loc = callExpr->getExprLoc();
  switch (op) {
  case hlsl::IntrinsicOp::IOP_DispatchRaysDimensions:
    builtin = spv::BuiltIn::LaunchSizeNV;
    break;
  case hlsl::IntrinsicOp::IOP_DispatchRaysIndex:
    builtin = spv::BuiltIn::LaunchIdNV;
    break;
  case hlsl::IntrinsicOp::IOP_RayTCurrent:
    builtin = spv::BuiltIn::HitTNV;
    break;
  case hlsl::IntrinsicOp::IOP_RayTMin:
    builtin = spv::BuiltIn::RayTminNV;
    break;
  case hlsl::IntrinsicOp::IOP_HitKind:
    builtin = spv::BuiltIn::HitKindNV;
    break;
  case hlsl::IntrinsicOp::IOP_WorldRayDirection:
    builtin = spv::BuiltIn::WorldRayDirectionNV;
    break;
  case hlsl::IntrinsicOp::IOP_WorldRayOrigin:
    builtin = spv::BuiltIn::WorldRayOriginNV;
    break;
  case hlsl::IntrinsicOp::IOP_ObjectRayDirection:
    builtin = spv::BuiltIn::ObjectRayDirectionNV;
    break;
  case hlsl::IntrinsicOp::IOP_ObjectRayOrigin:
    builtin = spv::BuiltIn::ObjectRayOriginNV;
    break;
  case hlsl::IntrinsicOp::IOP_InstanceIndex:
    builtin = spv::BuiltIn::InstanceId;
    break;
  case hlsl::IntrinsicOp::IOP_PrimitiveIndex:
    builtin = spv::BuiltIn::PrimitiveId;
    break;
  case hlsl::IntrinsicOp::IOP_InstanceID:
    builtin = spv::BuiltIn::InstanceCustomIndexNV;
    break;
  case hlsl::IntrinsicOp::IOP_RayFlags:
    builtin = spv::BuiltIn::IncomingRayFlagsNV;
    break;
  case hlsl::IntrinsicOp::IOP_ObjectToWorld3x4:
    transposeMatrix = true;
  case hlsl::IntrinsicOp::IOP_ObjectToWorld4x3:
    builtin = spv::BuiltIn::ObjectToWorldNV;
    break;
  case hlsl::IntrinsicOp::IOP_WorldToObject3x4:
    transposeMatrix = true;
  case hlsl::IntrinsicOp::IOP_WorldToObject4x3:
    builtin = spv::BuiltIn::WorldToObjectNV;
    break;
  default:
    emitError("ray intrinsic function unimplemented", loc);
    return nullptr;
  }

  QualType builtinType = callExpr->getType();
  if (transposeMatrix) {
    // DXR defines ObjectToWorld3x4, WorldToObject3x4 as transposed matrices.
    // SPIR-V has only non tranposed variant defined as a builtin
    // So perform read of original non transposed builtin and perform transpose.
    assert(hlsl::IsHLSLMatType(builtinType) && "Builtin should be matrix");
    const clang::Type *type = builtinType.getCanonicalType().getTypePtr();
    const RecordType *RT = cast<RecordType>(type);
    const ClassTemplateSpecializationDecl *templateSpecDecl =
        cast<ClassTemplateSpecializationDecl>(RT->getDecl());
    ClassTemplateDecl *templateDecl =
        templateSpecDecl->getSpecializedTemplate();
    builtinType = getHLSLMatrixType(astContext, theCompilerInstance.getSema(),
                                    templateDecl, astContext.FloatTy, 4, 3);
  }
  SpirvInstruction *retVal =
      declIdMapper.getBuiltinVar(builtin, builtinType, loc);
  retVal = spvBuilder.createLoad(builtinType, retVal, loc);
  if (transposeMatrix)
    retVal = spvBuilder.createUnaryOp(spv::Op::OpTranspose, callExpr->getType(),
                                      retVal, loc);
  return retVal;
}

SpirvInstruction *SpirvEmitter::processReportHit(const CallExpr *callExpr) {
  SpirvInstruction *hitAttributeStageVar = nullptr;
  const VarDecl *hitAttributeArg = nullptr;
  QualType hitAttributeType;
  const auto args = callExpr->getArgs();

  if (callExpr->getNumArgs() != 3) {
    emitError("invalid number of arguments to ReportHit",
              callExpr->getExprLoc());
  }

  // HLSL Function :
  // template<typename hitAttr>
  // ReportHit(in float, in uint, in hitAttr)
  if (const auto *implCastExpr = dyn_cast<CastExpr>(callExpr->getArg(2))) {
    if (const auto *arg = dyn_cast<DeclRefExpr>(implCastExpr->getSubExpr())) {
      if (const auto *varDecl = dyn_cast<VarDecl>(arg->getDecl())) {
        hitAttributeType = varDecl->getType();
        hitAttributeArg = varDecl;
        // Check if same type of hit attribute stage variable was already
        // created, if so re-use
        const auto iter = hitAttributeMap.find(hitAttributeType);
        if (iter == hitAttributeMap.end()) {
          hitAttributeStageVar = declIdMapper.createRayTracingNVStageVar(
              spv::StorageClass::HitAttributeNV, varDecl);
          hitAttributeMap[hitAttributeType] = hitAttributeStageVar;
        } else {
          hitAttributeStageVar = iter->second;
        }
      }
    }
  }

  assert(hitAttributeStageVar && hitAttributeArg);

  // Copy argument to stage variable
  const auto hitAttributeArgInst =
      declIdMapper.getDeclEvalInfo(hitAttributeArg, callExpr->getExprLoc());
  auto tempLoad =
      spvBuilder.createLoad(hitAttributeArg->getType(), hitAttributeArgInst,
                            hitAttributeArg->getLocStart());
  spvBuilder.createStore(hitAttributeStageVar, tempLoad,
                         callExpr->getExprLoc());

  // SPIR-V Instruction :
  // bool OpReportIntersection(<id> float Hit, <id> uint HitKind)
  llvm::SmallVector<SpirvInstruction *, 4> reportHitArgs;
  reportHitArgs.push_back(doExpr(args[0])); // Hit
  reportHitArgs.push_back(doExpr(args[1])); // HitKind
  return spvBuilder.createRayTracingOpsNV(spv::Op::OpReportIntersectionNV,
                                          astContext.BoolTy, reportHitArgs,
                                          callExpr->getExprLoc());
}

void SpirvEmitter::processCallShader(const CallExpr *callExpr) {
  SpirvInstruction *callDataLocInst = nullptr;
  SpirvInstruction *callDataStageVar = nullptr;
  const VarDecl *callDataArg = nullptr;
  QualType callDataType;
  const auto args = callExpr->getArgs();

  if (callExpr->getNumArgs() != 2) {
    emitError("invalid number of arguments to CallShader",
              callExpr->getExprLoc());
  }

  // HLSL Func :
  // template<typename CallData>
  // void CallShader(in int sbtIndex, inout CallData arg)
  if (const auto *implCastExpr = dyn_cast<CastExpr>(args[1])) {
    if (const auto *arg = dyn_cast<DeclRefExpr>(implCastExpr->getSubExpr())) {
      if (const auto *varDecl = dyn_cast<VarDecl>(arg->getDecl())) {
        callDataType = varDecl->getType();
        callDataArg = varDecl;
        // Check if same type of callable data stage variable was already
        // created, if so re-use
        const auto callDataPair = callDataMap.find(callDataType);
        if (callDataPair == callDataMap.end()) {
          int numCallDataVars = callDataMap.size();
          callDataStageVar = declIdMapper.createRayTracingNVStageVar(
              spv::StorageClass::CallableDataNV, varDecl);
          // Decorate unique location id for each created stage var
          spvBuilder.decorateLocation(callDataStageVar, numCallDataVars);
          callDataLocInst = spvBuilder.getConstantInt(
              astContext.UnsignedIntTy, llvm::APInt(32, numCallDataVars));
          callDataMap[callDataType] =
              std::make_pair(callDataStageVar, callDataLocInst);
        } else {
          callDataStageVar = callDataPair->second.first;
          callDataLocInst = callDataPair->second.second;
        }
      }
    }
  }

  assert(callDataStageVar && callDataArg);

  // Copy argument to stage variable
  const auto callDataArgInst =
      declIdMapper.getDeclEvalInfo(callDataArg, callExpr->getExprLoc());
  auto tempLoad = spvBuilder.createLoad(callDataArg->getType(), callDataArgInst,
                                        callDataArg->getLocStart());
  spvBuilder.createStore(callDataStageVar, tempLoad, callExpr->getExprLoc());

  // SPIR-V Instruction
  // void OpExecuteCallable(<id> int SBT Index, <id> uint Callable Data Location
  // Id)
  llvm::SmallVector<SpirvInstruction *, 2> callShaderArgs;
  callShaderArgs.push_back(doExpr(args[0]));
  callShaderArgs.push_back(callDataLocInst);

  spvBuilder.createRayTracingOpsNV(spv::Op::OpExecuteCallableNV, QualType(),
                                   callShaderArgs, callExpr->getExprLoc());

  // Copy data back to argument
  tempLoad = spvBuilder.createLoad(callDataArg->getType(), callDataStageVar,
                                   callDataArg->getLocStart());
  spvBuilder.createStore(callDataArgInst, tempLoad, callExpr->getExprLoc());
  return;
}

void SpirvEmitter::processTraceRay(const CallExpr *callExpr) {
  SpirvInstruction *rayPayloadLocInst = nullptr;
  SpirvInstruction *rayPayloadStageVar = nullptr;
  const VarDecl *rayPayloadArg = nullptr;
  QualType rayPayloadType;

  const auto args = callExpr->getArgs();

  if (callExpr->getNumArgs() != 8) {
    emitError("invalid number of arguments to TraceRay",
              callExpr->getExprLoc());
  }

  // HLSL Func
  // template<typename RayPayload>
  // void TraceRay(RaytracingAccelerationStructure rs,
  //              uint rayflags,
  //              uint InstanceInclusionMask
  //              uint RayContributionToHitGroupIndex,
  //              uint MultiplierForGeometryContributionToHitGroupIndex,
  //              uint MissShaderIndex,
  //              RayDesc ray,
  //              inout RayPayload p)
  // where RayDesc = {float3 origin, float tMin, float3 direction, float tMax}

  if (const auto *implCastExpr = dyn_cast<CastExpr>(args[7])) {
    if (const auto *arg = dyn_cast<DeclRefExpr>(implCastExpr->getSubExpr())) {
      if (const auto *varDecl = dyn_cast<VarDecl>(arg->getDecl())) {
        rayPayloadType = varDecl->getType();
        rayPayloadArg = varDecl;
        const auto rayPayloadPair = rayPayloadMap.find(rayPayloadType);
        // Check if same type of rayPayload stage variable was already
        // created, if so re-use
        if (rayPayloadPair == rayPayloadMap.end()) {
          int numPayloadVars = rayPayloadMap.size();
          rayPayloadStageVar = declIdMapper.createRayTracingNVStageVar(
              spv::StorageClass::RayPayloadNV, varDecl);
          // Decorate unique location id for each created stage var
          spvBuilder.decorateLocation(rayPayloadStageVar, numPayloadVars);
          rayPayloadLocInst = spvBuilder.getConstantInt(
              astContext.UnsignedIntTy, llvm::APInt(32, numPayloadVars));
          rayPayloadMap[rayPayloadType] =
              std::make_pair(rayPayloadStageVar, rayPayloadLocInst);
        } else {
          rayPayloadStageVar = rayPayloadPair->second.first;
          rayPayloadLocInst = rayPayloadPair->second.second;
        }
      }
    }
  }

  assert(rayPayloadStageVar && rayPayloadArg);

  const auto floatType = astContext.FloatTy;
  const auto vecType = astContext.getExtVectorType(astContext.FloatTy, 3);

  // Extract the ray description to match SPIR-V
  SpirvInstruction *rayDescArg = doExpr(args[6]);
  const auto loc = args[6]->getLocStart();
  const auto origin =
      spvBuilder.createCompositeExtract(vecType, rayDescArg, {0}, loc);
  const auto tMin =
      spvBuilder.createCompositeExtract(floatType, rayDescArg, {1}, loc);
  const auto direction =
      spvBuilder.createCompositeExtract(vecType, rayDescArg, {2}, loc);
  const auto tMax =
      spvBuilder.createCompositeExtract(floatType, rayDescArg, {3}, loc);

  // Copy argument to stage variable
  const auto rayPayloadArgInst =
      declIdMapper.getDeclEvalInfo(rayPayloadArg, rayPayloadArg->getLocStart());
  auto tempLoad =
      spvBuilder.createLoad(rayPayloadArg->getType(), rayPayloadArgInst,
                            rayPayloadArg->getLocStart());
  spvBuilder.createStore(rayPayloadStageVar, tempLoad, callExpr->getExprLoc());

  // SPIR-V Instruction
  // void OpTraceNV ( <id> AccelerationStructureNV acStruct,
  //                 <id> uint Ray Flags,
  //                 <id> uint Cull Mask,
  //                 <id> uint SBT Offset,
  //                 <id> uint SBT Stride,
  //                 <id> uint Miss Index,
  //                 <id> vec4 Ray Origin,
  //                 <id> float Ray Tmin,
  //                 <id> vec3 Ray Direction,
  //                 <id> float Ray Tmax,
  //                 <id> uint RayPayload number)

  llvm::SmallVector<SpirvInstruction *, 8> traceArgs;
  for (int ii = 0; ii < 6; ii++) {
    traceArgs.push_back(doExpr(args[ii]));
  }

  traceArgs.push_back(origin);
  traceArgs.push_back(tMin);
  traceArgs.push_back(direction);
  traceArgs.push_back(tMax);
  traceArgs.push_back(rayPayloadLocInst);

  spvBuilder.createRayTracingOpsNV(spv::Op::OpTraceNV, QualType(), traceArgs,
                                   callExpr->getExprLoc());

  // Copy arguments back to stage variable
  tempLoad = spvBuilder.createLoad(rayPayloadArg->getType(), rayPayloadStageVar,
                                   rayPayloadArg->getLocStart());
  spvBuilder.createStore(rayPayloadArgInst, tempLoad, callExpr->getExprLoc());
  return;
}

void SpirvEmitter::processDispatchMesh(const CallExpr *callExpr) {
  // HLSL Func - void DispatchMesh(uint ThreadGroupCountX,
  //                               uint ThreadGroupCountY,
  //                               uint ThreadGroupCountZ,
  //                               groupshared <structType> MeshPayload);
  assert(callExpr->getNumArgs() == 4);
  const auto args = callExpr->getArgs();
  const auto loc = callExpr->getExprLoc();

  // 1) create a barrier GroupMemoryBarrierWithGroupSync().
  processIntrinsicMemoryBarrier(callExpr,
                                /*isDevice*/ false,
                                /*groupSync*/ true,
                                /*isAllBarrier*/ false);

  // 2) set TaskCountNV = threadX * threadY * threadZ.
  auto *threadX = doExpr(args[0]);
  auto *threadY = doExpr(args[1]);
  auto *threadZ = doExpr(args[2]);
  auto *var = declIdMapper.getBuiltinVar(spv::BuiltIn::TaskCountNV,
                                         astContext.UnsignedIntTy, loc);
  auto *taskCount = spvBuilder.createBinaryOp(
      spv::Op::OpIMul, astContext.UnsignedIntTy, threadX,
      spvBuilder.createBinaryOp(spv::Op::OpIMul, astContext.UnsignedIntTy,
                                threadY, threadZ, loc),
      loc);
  spvBuilder.createStore(var, taskCount, loc);

  // 3) create PerTaskNV out attribute block and store MeshPayload info.
  const auto *sigPoint =
      hlsl::SigPoint::GetSigPoint(hlsl::DXIL::SigPointKind::MSOut);
  spv::StorageClass sc = spv::StorageClass::Output;
  auto *payloadArg = doExpr(args[3]);
  bool isValid = false;
  if (const auto *implCastExpr = dyn_cast<CastExpr>(args[3])) {
    if (const auto *arg = dyn_cast<DeclRefExpr>(implCastExpr->getSubExpr())) {
      if (const auto *paramDecl = dyn_cast<VarDecl>(arg->getDecl())) {
        if (paramDecl->hasAttr<HLSLGroupSharedAttr>()) {
          isValid = declIdMapper.createPayloadStageVars(
              sigPoint, sc, paramDecl, /*asInput=*/false, paramDecl->getType(),
              "out.var", &payloadArg);
        }
      }
    }
  }
  if (!isValid) {
    emitError("expected groupshared object as argument to DispatchMesh()",
              args[3]->getExprLoc());
  }
}

void SpirvEmitter::processMeshOutputCounts(const CallExpr *callExpr) {
  // HLSL Func - void SetMeshOutputCounts(uint numVertices, uint numPrimitives);
  assert(callExpr->getNumArgs() == 2);
  const auto args = callExpr->getArgs();
  const auto loc = callExpr->getExprLoc();
  auto *var = declIdMapper.getBuiltinVar(spv::BuiltIn::PrimitiveCountNV,
                                         astContext.UnsignedIntTy, loc);
  spvBuilder.createStore(var, doExpr(args[1]), loc);
}

SpirvConstant *SpirvEmitter::getValueZero(QualType type) {
  {
    QualType scalarType = {};
    if (isScalarType(type, &scalarType)) {
      if (scalarType->isBooleanType()) {
        return spvBuilder.getConstantBool(false);
      }
      if (scalarType->isIntegerType()) {
        return spvBuilder.getConstantInt(scalarType, llvm::APInt(32, 0));
      }
      if (scalarType->isFloatingType()) {
        return spvBuilder.getConstantFloat(scalarType, llvm::APFloat(0.0f));
      }
    }
  }

  {
    QualType elemType = {};
    uint32_t size = {};
    if (isVectorType(type, &elemType, &size)) {
      return getVecValueZero(elemType, size);
    }
  }

  {
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount)) {
      auto *row = getVecValueZero(elemType, colCount);
      llvm::SmallVector<SpirvConstant *, 4> rows((size_t)rowCount, row);
      return spvBuilder.getConstantComposite(type, rows);
    }
  }

  emitError("getting value 0 for type %0 unimplemented", {})
      << type.getAsString();
  return nullptr;
}

SpirvConstant *SpirvEmitter::getVecValueZero(QualType elemType, uint32_t size) {
  auto *elemZeroId = getValueZero(elemType);

  if (size == 1)
    return elemZeroId;

  llvm::SmallVector<SpirvConstant *, 4> elements(size_t(size), elemZeroId);
  const QualType vecType = astContext.getExtVectorType(elemType, size);
  return spvBuilder.getConstantComposite(vecType, elements);
}

SpirvConstant *SpirvEmitter::getValueOne(QualType type) {
  {
    QualType scalarType = {};
    if (isScalarType(type, &scalarType)) {
      if (scalarType->isBooleanType()) {
        return spvBuilder.getConstantBool(true);
      }
      if (scalarType->isIntegerType()) {
        return spvBuilder.getConstantInt(scalarType, llvm::APInt(32, 1));
      }
      if (scalarType->isFloatingType()) {
        return spvBuilder.getConstantFloat(scalarType, llvm::APFloat(1.0f));
      }
    }
  }

  {
    QualType elemType = {};
    uint32_t size = {};
    if (isVectorType(type, &elemType, &size)) {
      return getVecValueOne(elemType, size);
    }
  }

  emitError("getting value 1 for type %0 unimplemented", {}) << type;
  return 0;
}

SpirvConstant *SpirvEmitter::getVecValueOne(QualType elemType, uint32_t size) {
  auto *elemOne = getValueOne(elemType);

  if (size == 1)
    return elemOne;

  llvm::SmallVector<SpirvConstant *, 4> elements(size_t(size), elemOne);
  const QualType vecType = astContext.getExtVectorType(elemType, size);
  return spvBuilder.getConstantComposite(vecType, elements);
}

SpirvConstant *SpirvEmitter::getMatElemValueOne(QualType type) {
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

SpirvConstant *SpirvEmitter::getMaskForBitwidthValue(QualType type) {
  QualType elemType = {};
  uint32_t count = 1;

  if (isScalarType(type, &elemType) || isVectorType(type, &elemType, &count)) {
    const auto bitwidth = getElementSpirvBitwidth(
        astContext, elemType, spirvOptions.enable16BitTypes);
    SpirvConstant *mask = spvBuilder.getConstantInt(
        elemType,
        llvm::APInt(bitwidth, bitwidth - 1, elemType->isSignedIntegerType()));

    if (count == 1)
      return mask;

    const QualType resultType = astContext.getExtVectorType(elemType, count);
    llvm::SmallVector<SpirvConstant *, 4> elements(size_t(count), mask);
    return spvBuilder.getConstantComposite(resultType, elements);
  }

  assert(false && "this method only supports scalars and vectors");
  return nullptr;
}

SpirvConstant *SpirvEmitter::translateAPValue(const APValue &value,
                                              const QualType targetType) {
  SpirvConstant *result = nullptr;

  if (targetType->isBooleanType()) {
    result = spvBuilder.getConstantBool(value.getInt().getBoolValue(),
                                        isSpecConstantMode);
  } else if (targetType->isIntegerType()) {
    result = translateAPInt(value.getInt(), targetType);
  } else if (targetType->isFloatingType()) {
    result = translateAPFloat(value.getFloat(), targetType);
  } else if (hlsl::IsHLSLVecType(targetType)) {
    const QualType elemType = hlsl::GetHLSLVecElementType(targetType);
    const auto numElements = value.getVectorLength();
    // Special case for vectors of size 1. SPIR-V doesn't support this vector
    // size so we need to translate it to scalar values.
    if (numElements == 1) {
      result = translateAPValue(value.getVectorElt(0), elemType);
    } else {
      llvm::SmallVector<SpirvConstant *, 4> elements;
      for (uint32_t i = 0; i < numElements; ++i) {
        elements.push_back(translateAPValue(value.getVectorElt(i), elemType));
      }
      result = spvBuilder.getConstantComposite(targetType, elements);
    }
  }

  if (result)
    return result;

  emitError("APValue of type %0 unimplemented", {}) << value.getKind();
  value.dump();
  return 0;
}

SpirvConstant *SpirvEmitter::translateAPInt(const llvm::APInt &intValue,
                                            QualType targetType) {
  return spvBuilder.getConstantInt(targetType, intValue, isSpecConstantMode);
}

bool SpirvEmitter::isLiteralLargerThan32Bits(const Expr *expr) {
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

SpirvConstant *SpirvEmitter::tryToEvaluateAsInt32(const llvm::APInt &intValue,
                                                  bool isSigned) {
  if (isSigned && intValue.isSignedIntN(32)) {
    return spvBuilder.getConstantInt(astContext.IntTy, intValue);
  }
  if (!isSigned && intValue.isIntN(32)) {
    return spvBuilder.getConstantInt(astContext.UnsignedIntTy, intValue);
  }

  // Couldn't evaluate as a 32-bit int without losing information.
  return nullptr;
}

SpirvConstant *
SpirvEmitter::tryToEvaluateAsFloat32(const llvm::APFloat &floatValue) {
  const auto &semantics = floatValue.getSemantics();
  // If the given value is already a 32-bit float, there is no need to convert.
  if (&semantics == &llvm::APFloat::IEEEsingle) {
    return spvBuilder.getConstantFloat(astContext.FloatTy, floatValue,
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
    return spvBuilder.getConstantFloat(astContext.FloatTy,
                                       llvm::APFloat(eval.convertToFloat()));

  // Couldn't evaluate as a 32-bit float without losing information.
  return nullptr;
}

SpirvConstant *SpirvEmitter::translateAPFloat(llvm::APFloat floatValue,
                                              QualType targetType) {
  return spvBuilder.getConstantFloat(targetType, floatValue,
                                     isSpecConstantMode);
}

SpirvConstant *SpirvEmitter::tryToEvaluateAsConst(const Expr *expr) {
  Expr::EvalResult evalResult;
  if (expr->EvaluateAsRValue(evalResult, astContext) &&
      !evalResult.HasSideEffects) {
    return translateAPValue(evalResult.Val, expr->getType());
  }

  return nullptr;
}

hlsl::ShaderModel::Kind SpirvEmitter::getShaderModelKind(StringRef stageName) {
  hlsl::ShaderModel::Kind smk;
  switch (stageName[0]) {
  case 'c':
    switch (stageName[1]) {
    case 'o':
      smk = hlsl::ShaderModel::Kind::Compute;
      break;
    case 'l':
      smk = hlsl::ShaderModel::Kind::ClosestHit;
      break;
    case 'a':
      smk = hlsl::ShaderModel::Kind::Callable;
      break;
    default:
      smk = hlsl::ShaderModel::Kind::Invalid;
      break;
    }
    break;
  case 'v':
    smk = hlsl::ShaderModel::Kind::Vertex;
    break;
  case 'h':
    smk = hlsl::ShaderModel::Kind::Hull;
    break;
  case 'd':
    smk = hlsl::ShaderModel::Kind::Domain;
    break;
  case 'g':
    smk = hlsl::ShaderModel::Kind::Geometry;
    break;
  case 'p':
    smk = hlsl::ShaderModel::Kind::Pixel;
    break;
  case 'r':
    smk = hlsl::ShaderModel::Kind::RayGeneration;
    break;
  case 'i':
    smk = hlsl::ShaderModel::Kind::Intersection;
    break;
  case 'a':
    switch (stageName[1]) {
    case 'm':
      smk = hlsl::ShaderModel::Kind::Amplification;
      break;
    case 'n':
      smk = hlsl::ShaderModel::Kind::AnyHit;
      break;
    }
    break;
  case 'm':
    switch (stageName[1]) {
    case 'e':
      smk = hlsl::ShaderModel::Kind::Mesh;
      break;
    case 'i':
      smk = hlsl::ShaderModel::Kind::Miss;
      break;
    }
    break;
  default:
    smk = hlsl::ShaderModel::Kind::Invalid;
    break;
  }
  if (smk == hlsl::ShaderModel::Kind::Invalid) {
    llvm_unreachable("unknown stage name");
  }
  return smk;
}

spv::ExecutionModel
SpirvEmitter::getSpirvShaderStage(hlsl::ShaderModel::Kind smk) {
  switch (smk) {
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
  case hlsl::ShaderModel::Kind::RayGeneration:
    return spv::ExecutionModel::RayGenerationNV;
  case hlsl::ShaderModel::Kind::Intersection:
    return spv::ExecutionModel::IntersectionNV;
  case hlsl::ShaderModel::Kind::AnyHit:
    return spv::ExecutionModel::AnyHitNV;
  case hlsl::ShaderModel::Kind::ClosestHit:
    return spv::ExecutionModel::ClosestHitNV;
  case hlsl::ShaderModel::Kind::Miss:
    return spv::ExecutionModel::MissNV;
  case hlsl::ShaderModel::Kind::Callable:
    return spv::ExecutionModel::CallableNV;
  case hlsl::ShaderModel::Kind::Mesh:
    return spv::ExecutionModel::MeshNV;
  case hlsl::ShaderModel::Kind::Amplification:
    return spv::ExecutionModel::TaskNV;
  default:
    llvm_unreachable("invalid shader model kind");
    break;
  }
}

bool SpirvEmitter::processGeometryShaderAttributes(const FunctionDecl *decl,
                                                   uint32_t *arraySize) {
  bool success = true;
  assert(spvContext.isGS());
  if (auto *vcAttr = decl->getAttr<HLSLMaxVertexCountAttr>()) {
    spvBuilder.addExecutionMode(
        entryFunction, spv::ExecutionMode::OutputVertices,
        {static_cast<uint32_t>(vcAttr->getCount())}, decl->getLocation());
  }

  uint32_t invocations = 1;
  if (auto *instanceAttr = decl->getAttr<HLSLInstanceAttr>()) {
    invocations = static_cast<uint32_t>(instanceAttr->getCount());
  }
  spvBuilder.addExecutionMode(entryFunction, spv::ExecutionMode::Invocations,
                              {invocations}, decl->getLocation());

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
        spvBuilder.addExecutionMode(entryFunction,
                                    spv::ExecutionMode::OutputTriangleStrip, {},
                                    param->getLocation());
        outTriangle = true;
      } else if (hlsl::IsHLSLLineStreamType(paramType) && !outLine) {
        spvBuilder.addExecutionMode(entryFunction,
                                    spv::ExecutionMode::OutputLineStrip, {},
                                    param->getLocation());
        outLine = true;
      } else if (hlsl::IsHLSLPointStreamType(paramType) && !outPoint) {
        spvBuilder.addExecutionMode(entryFunction,
                                    spv::ExecutionMode::OutputPoints, {},
                                    param->getLocation());
        outPoint = true;
      }
      // An output stream parameter will not have the input primitive type
      // attributes, so we can continue to the next parameter.
      continue;
    }

    // Add an execution mode based on the input primitive type. Do not add an
    // execution mode more than once.
    if (param->hasAttr<HLSLPointAttr>() && !inPoint) {
      spvBuilder.addExecutionMode(entryFunction,
                                  spv::ExecutionMode::InputPoints, {},
                                  param->getLocation());
      *arraySize = 1;
      inPoint = true;
    } else if (param->hasAttr<HLSLLineAttr>() && !inLine) {
      spvBuilder.addExecutionMode(entryFunction, spv::ExecutionMode::InputLines,
                                  {}, param->getLocation());
      *arraySize = 2;
      inLine = true;
    } else if (param->hasAttr<HLSLTriangleAttr>() && !inTriangle) {
      spvBuilder.addExecutionMode(entryFunction, spv::ExecutionMode::Triangles,
                                  {}, param->getLocation());
      *arraySize = 3;
      inTriangle = true;
    } else if (param->hasAttr<HLSLLineAdjAttr>() && !inLineAdj) {
      spvBuilder.addExecutionMode(entryFunction,
                                  spv::ExecutionMode::InputLinesAdjacency, {},
                                  param->getLocation());
      *arraySize = 4;
      inLineAdj = true;
    } else if (param->hasAttr<HLSLTriangleAdjAttr>() && !inTriangleAdj) {
      spvBuilder.addExecutionMode(entryFunction,
                                  spv::ExecutionMode::InputTrianglesAdjacency,
                                  {}, param->getLocation());
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

void SpirvEmitter::processPixelShaderAttributes(const FunctionDecl *decl) {
  spvBuilder.addExecutionMode(entryFunction,
                              spv::ExecutionMode::OriginUpperLeft, {},
                              decl->getLocation());
  if (decl->getAttr<HLSLEarlyDepthStencilAttr>()) {
    spvBuilder.addExecutionMode(entryFunction,
                                spv::ExecutionMode::EarlyFragmentTests, {},
                                decl->getLocation());
  }
  if (decl->getAttr<VKPostDepthCoverageAttr>()) {
    spvBuilder.addExecutionMode(entryFunction,
                                spv::ExecutionMode::PostDepthCoverage, {},
                                decl->getLocation());
  }
}

void SpirvEmitter::processComputeShaderAttributes(const FunctionDecl *decl) {
  // If not explicitly specified, x, y, and z should be defaulted to 1.
  uint32_t x = 1, y = 1, z = 1;

  if (auto *numThreadsAttr = decl->getAttr<HLSLNumThreadsAttr>()) {
    x = static_cast<uint32_t>(numThreadsAttr->getX());
    y = static_cast<uint32_t>(numThreadsAttr->getY());
    z = static_cast<uint32_t>(numThreadsAttr->getZ());
  }

  spvBuilder.addExecutionMode(entryFunction, spv::ExecutionMode::LocalSize,
                              {x, y, z}, decl->getLocation());
}

bool SpirvEmitter::processTessellationShaderAttributes(
    const FunctionDecl *decl, uint32_t *numOutputControlPoints) {
  assert(spvContext.isHS() || spvContext.isDS());
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
    spvBuilder.addExecutionMode(entryFunction, hsExecMode, {},
                                decl->getLocation());
  }

  // Early return for domain shaders as domain shaders only takes the 'domain'
  // attribute.
  if (spvContext.isDS())
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
    spvBuilder.addExecutionMode(entryFunction, hsExecMode, {},
                                decl->getLocation());
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
        spvBuilder.addExecutionMode(entryFunction, hsExecMode, {},
                                    decl->getLocation());
      } else {
        emitError("unknown output topology in hull shader",
                  outputTopology->getLocation());
        return false;
      }
    }
  }
  if (auto *controlPoints = decl->getAttr<HLSLOutputControlPointsAttr>()) {
    *numOutputControlPoints = controlPoints->getCount();
    spvBuilder.addExecutionMode(entryFunction,
                                spv::ExecutionMode::OutputVertices,
                                {*numOutputControlPoints}, decl->getLocation());
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

bool SpirvEmitter::emitEntryFunctionWrapperForRayTracing(
    const FunctionDecl *decl, SpirvFunction *entryFuncInstr) {
  // The entry basic block.
  auto *entryLabel = spvBuilder.createBasicBlock();
  spvBuilder.setInsertPoint(entryLabel);

  // Initialize all global variables at the beginning of the wrapper
  for (const VarDecl *varDecl : toInitGloalVars) {
    const auto varInfo =
        declIdMapper.getDeclEvalInfo(varDecl, varDecl->getLocation());
    if (const auto *init = varDecl->getInit()) {
      storeValue(varInfo, loadIfGLValue(init), varDecl->getType(),
                 init->getLocStart());

      // Update counter variable associated with global variables
      tryToAssignCounterVar(varDecl, init);
    }
    // If not explicitly initialized, initialize with their zero values if not
    // resource objects
    else if (!hlsl::IsHLSLResourceType(varDecl->getType())) {
      auto *nullValue = spvBuilder.getConstantNull(varDecl->getType());
      spvBuilder.createStore(varInfo, nullValue, varDecl->getLocation());
    }
  }

  // Create temporary variables for holding function call arguments
  llvm::SmallVector<SpirvInstruction *, 4> params;
  llvm::SmallVector<QualType, 4> paramTypes;
  llvm::SmallVector<SpirvInstruction *, 4> stageVars;
  hlsl::ShaderModel::Kind sKind = spvContext.getCurrentShaderModelKind();
  for (uint32_t i = 0; i < decl->getNumParams(); i++) {
    const auto param = decl->getParamDecl(i);
    const auto paramType = param->getType();
    std::string tempVarName = "param.var." + param->getNameAsString();
    auto *tempVar =
        spvBuilder.addFnVar(paramType, param->getLocation(), tempVarName,
                            param->hasAttr<HLSLPreciseAttr>());

    SpirvVariable *curStageVar = nullptr;

    params.push_back(tempVar);
    paramTypes.push_back(paramType);

    // Order of arguments is fixed
    // Any-Hit/Closest-Hit : Arg 0 = rayPayload(inout), Arg1 = attribute(in)
    // Miss : Arg 0 = rayPayload(inout)
    // Callable : Arg 0 = callable data(inout)
    // Raygeneration/Intersection : No Args allowed
    if (sKind == hlsl::ShaderModel::Kind::RayGeneration) {
      assert("Raygeneration shaders have no arguments of entry function");
    } else if (sKind == hlsl::ShaderModel::Kind::Intersection) {
      assert("Intersection shaders have no arguments of entry function");
    } else if (sKind == hlsl::ShaderModel::Kind::ClosestHit ||
               sKind == hlsl::ShaderModel::Kind::AnyHit) {
      // Generate rayPayloadInNV and hitAttributeNV stage variables
      if (i == 0) {
        // First argument is always rayPayload
        curStageVar = declIdMapper.createRayTracingNVStageVar(
            spv::StorageClass::IncomingRayPayloadNV, param);
        currentRayPayload = curStageVar;
      } else {
        // Second argument is always attribute
        curStageVar = declIdMapper.createRayTracingNVStageVar(
            spv::StorageClass::HitAttributeNV, param);
      }
    } else if (sKind == hlsl::ShaderModel::Kind::Miss) {
      // Generate rayPayloadInNV stage variable
      // First and only argument is rayPayload
      curStageVar = declIdMapper.createRayTracingNVStageVar(
          spv::StorageClass::IncomingRayPayloadNV, param);
    } else if (sKind == hlsl::ShaderModel::Kind::Callable) {
      curStageVar = declIdMapper.createRayTracingNVStageVar(
          spv::StorageClass::IncomingCallableDataNV, param);
    }

    if (curStageVar != nullptr) {
      stageVars.push_back(curStageVar);
      // Copy data to temporary
      auto *tempLoadInst =
          spvBuilder.createLoad(paramType, curStageVar, param->getLocation());
      spvBuilder.createStore(tempVar, tempLoadInst, param->getLocation());
    }
  }

  // Call the original entry function
  const QualType retType = decl->getReturnType();
  spvBuilder.createFunctionCall(retType, entryFuncInstr, params,
                                decl->getLocStart());

  // Write certain output variables back
  if (sKind == hlsl::ShaderModel::Kind::ClosestHit ||
      sKind == hlsl::ShaderModel::Kind::AnyHit ||
      sKind == hlsl::ShaderModel::Kind::Miss ||
      sKind == hlsl::ShaderModel::Kind::Callable) {
    // Write back results to IncomingRayPayloadNV/IncomingCallableDataNV
    auto *tempLoad = spvBuilder.createLoad(paramTypes[0], params[0],
                                           decl->getBody()->getLocEnd());
    spvBuilder.createStore(stageVars[0], tempLoad,
                           decl->getBody()->getLocEnd());
  }

  spvBuilder.createReturn(decl->getBody()->getLocEnd());
  spvBuilder.endFunction();

  return true;
}

bool SpirvEmitter::processMeshOrAmplificationShaderAttributes(
    const FunctionDecl *decl, uint32_t *outVerticesArraySize) {
  if (auto *numThreadsAttr = decl->getAttr<HLSLNumThreadsAttr>()) {
    uint32_t x, y, z;
    x = static_cast<uint32_t>(numThreadsAttr->getX());
    y = static_cast<uint32_t>(numThreadsAttr->getY());
    z = static_cast<uint32_t>(numThreadsAttr->getZ());
    spvBuilder.addExecutionMode(entryFunction, spv::ExecutionMode::LocalSize,
                                {x, y, z}, decl->getLocation());
  }

  // Early return for amplification shaders as they only take the 'numthreads'
  // attribute.
  if (spvContext.isAS())
    return true;

  spv::ExecutionMode outputPrimitive = spv::ExecutionMode::Max;
  if (auto *outputTopology = decl->getAttr<HLSLOutputTopologyAttr>()) {
    const auto topology = outputTopology->getTopology().lower();
    outputPrimitive =
        llvm::StringSwitch<spv::ExecutionMode>(topology)
            .Case("point", spv::ExecutionMode::OutputPoints)
            .Case("line", spv::ExecutionMode::OutputLinesNV)
            .Case("triangle", spv::ExecutionMode::OutputTrianglesNV);
    if (outputPrimitive != spv::ExecutionMode::Max) {
      spvBuilder.addExecutionMode(entryFunction, outputPrimitive, {},
                                  decl->getLocation());
    } else {
      emitError("unknown output topology in mesh shader",
                outputTopology->getLocation());
      return false;
    }
  }

  uint32_t numVertices = 0;
  uint32_t numIndices = 0;
  uint32_t numPrimitives = 0;
  bool payloadDeclSeen = false;

  for (uint32_t i = 0; i < decl->getNumParams(); i++) {
    const auto param = decl->getParamDecl(i);
    const auto paramType = param->getType();
    const auto paramLoc = param->getLocation();
    if (param->hasAttr<HLSLVerticesAttr>() ||
        param->hasAttr<HLSLIndicesAttr>() ||
        param->hasAttr<HLSLPrimitivesAttr>()) {
      uint32_t arraySize = 0;
      if (const auto *arrayType =
              astContext.getAsConstantArrayType(paramType)) {
        const auto eleType =
            arrayType->getElementType()->getCanonicalTypeUnqualified();
        if (param->hasAttr<HLSLIndicesAttr>()) {
          switch (outputPrimitive) {
          case spv::ExecutionMode::OutputPoints:
            if (eleType != astContext.UnsignedIntTy) {
              emitError("expected 1D array of uint type", paramLoc);
              return false;
            }
            break;
          case spv::ExecutionMode::OutputLinesNV: {
            QualType baseType;
            uint32_t length;
            if (!isVectorType(eleType, &baseType, &length) ||
                baseType != astContext.UnsignedIntTy || length != 2) {
              emitError("expected 1D array of uint2 type", paramLoc);
              return false;
            }
            break;
          }
          case spv::ExecutionMode::OutputTrianglesNV: {
            QualType baseType;
            uint32_t length;
            if (!isVectorType(eleType, &baseType, &length) ||
                baseType != astContext.UnsignedIntTy || length != 3) {
              emitError("expected 1D array of uint3 type", paramLoc);
              return false;
            }
            break;
          }
          default:
            assert(false && "unexpected spirv execution mode");
          }
        } else if (!eleType->isStructureType()) {
          // vertices/primitives objects
          emitError("expected 1D array of struct type", paramLoc);
          return false;
        }
        arraySize = static_cast<uint32_t>(arrayType->getSize().getZExtValue());
      } else {
        emitError("expected 1D array of indices/vertices/primitives object",
                  paramLoc);
        return false;
      }
      if (param->hasAttr<HLSLVerticesAttr>()) {
        if (numVertices != 0) {
          emitError("only one object with 'vertices' modifier is allowed",
                    paramLoc);
          return false;
        }
        numVertices = arraySize;
      } else if (param->hasAttr<HLSLIndicesAttr>()) {
        if (numIndices != 0) {
          emitError("only one object with 'indices' modifier is allowed",
                    paramLoc);
          return false;
        }
        numIndices = arraySize;
      } else if (param->hasAttr<HLSLPrimitivesAttr>()) {
        if (numPrimitives != 0) {
          emitError("only one object with 'primitives' modifier is allowed",
                    paramLoc);
          return false;
        }
        numPrimitives = arraySize;
      }
    } else if (param->hasAttr<HLSLPayloadAttr>()) {
      if (payloadDeclSeen) {
        emitError("only one object with 'payload' modifier is allowed",
                  paramLoc);
        return false;
      }
      payloadDeclSeen = true;
      if (!paramType->isStructureType()) {
        emitError("expected payload of struct type", paramLoc);
        return false;
      }
    }
  }

  // Vertex attribute array is a mandatory param to mesh entry function.
  if (numVertices != 0) {
    *outVerticesArraySize = numVertices;
    spvBuilder.addExecutionMode(
        entryFunction, spv::ExecutionMode::OutputVertices,
        {static_cast<uint32_t>(numVertices)}, decl->getLocation());
  } else {
    emitError("expected vertices object declaration", decl->getLocation());
    return false;
  }

  // Vertex indices array is a mandatory param to mesh entry function.
  if (numIndices != 0) {
    spvBuilder.addExecutionMode(
        entryFunction, spv::ExecutionMode::OutputPrimitivesNV,
        {static_cast<uint32_t>(numIndices)}, decl->getLocation());
    // Primitive attribute array is an optional param to mesh entry function,
    // but the array size should match the indices array.
    if (numPrimitives != 0 && numPrimitives != numIndices) {
      emitError("array size of primitives object should match 'indices' object",
                decl->getLocation());
      return false;
    }
  } else {
    emitError("expected indices object declaration", decl->getLocation());
    return false;
  }

  return true;
}

bool SpirvEmitter::emitEntryFunctionWrapper(const FunctionDecl *decl,
                                            SpirvFunction *entryFuncInstr) {
  // HS specific attributes
  uint32_t numOutputControlPoints = 0;
  SpirvInstruction *outputControlPointIdVal =
      nullptr;                                // SV_OutputControlPointID value
  SpirvInstruction *primitiveIdVar = nullptr; // SV_PrimitiveID variable
  SpirvInstruction *viewIdVar = nullptr;      // SV_ViewID variable
  SpirvInstruction *hullMainInputPatchParam =
      nullptr; // Temporary parameter for InputPatch<>

  // The array size of per-vertex input/output variables
  // Used by HS/DS/GS for the additional arrayness, zero means not an array.
  uint32_t inputArraySize = 0;
  uint32_t outputArraySize = 0;

  // The wrapper entry function surely does not have pre-assigned <result-id>
  // for it like other functions that got added to the work queue following
  // function calls. And the wrapper is the entry function.
  entryFunction =
      spvBuilder.beginFunction(astContext.VoidTy, /* param QualTypes */ {},
                               decl->getLocStart(), decl->getName());
  // Note this should happen before using declIdMapper for other tasks.
  declIdMapper.setEntryFunction(entryFunction);

  // Set entryFunction for current entry point.
  auto iter = functionInfoMap.find(decl);
  assert(iter != functionInfoMap.end());
  auto &entryInfo = iter->second;
  assert(entryInfo->isEntryFunction);
  entryInfo->entryFunction = entryFunction;

  if (spvContext.isRay()) {
    return emitEntryFunctionWrapperForRayTracing(decl, entryFuncInstr);
  }
  // Handle attributes specific to each shader stage
  if (spvContext.isPS()) {
    processPixelShaderAttributes(decl);
  } else if (spvContext.isCS()) {
    processComputeShaderAttributes(decl);
  } else if (spvContext.isHS()) {
    if (!processTessellationShaderAttributes(decl, &numOutputControlPoints))
      return false;

    // The input array size for HS is specified in the InputPatch parameter.
    for (const auto *param : decl->params())
      if (hlsl::IsHLSLInputPatchType(param->getType())) {
        inputArraySize = hlsl::GetHLSLInputPatchCount(param->getType());
        break;
      }

    outputArraySize = numOutputControlPoints;
  } else if (spvContext.isDS()) {
    if (!processTessellationShaderAttributes(decl, &numOutputControlPoints))
      return false;

    // The input array size for HS is specified in the OutputPatch parameter.
    for (const auto *param : decl->params())
      if (hlsl::IsHLSLOutputPatchType(param->getType())) {
        inputArraySize = hlsl::GetHLSLOutputPatchCount(param->getType());
        break;
      }
    // The per-vertex output of DS is not an array.
  } else if (spvContext.isGS()) {
    if (!processGeometryShaderAttributes(decl, &inputArraySize))
      return false;
    // The per-vertex output of GS is not an array.
  } else if (spvContext.isMS() || spvContext.isAS()) {
    if (!processMeshOrAmplificationShaderAttributes(decl, &outputArraySize))
      return false;
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

  if (!spvContext.isCS() && !spvContext.isAS()) {
    // Generate stand-alone builtins of Position, ClipDistance, and
    // CullDistance, which belongs to gl_PerVertex.
    declIdMapper.glPerVertex.generateVars(inputArraySize, outputArraySize);
  }

  // The entry basic block.
  auto *entryLabel = spvBuilder.createBasicBlock();
  spvBuilder.setInsertPoint(entryLabel);

  // Initialize all global variables at the beginning of the wrapper
  for (const VarDecl *varDecl : toInitGloalVars) {
    const auto varInfo =
        declIdMapper.getDeclEvalInfo(varDecl, varDecl->getLocation());
    if (const auto *init = varDecl->getInit()) {
      storeValue(varInfo, loadIfGLValue(init), varDecl->getType(),
                 init->getLocStart());

      // Update counter variable associated with global variables
      tryToAssignCounterVar(varDecl, init);
    }
    // If not explicitly initialized, initialize with their zero values if not
    // resource objects
    else if (!hlsl::IsHLSLResourceType(varDecl->getType())) {
      auto *nullValue = spvBuilder.getConstantNull(varDecl->getType());
      spvBuilder.createStore(varInfo, nullValue, varDecl->getLocation());
    }
  }

  // Create temporary variables for holding function call arguments
  llvm::SmallVector<SpirvInstruction *, 4> params;
  for (const auto *param : decl->params()) {
    const auto paramType = param->getType();
    std::string tempVarName = "param.var." + param->getNameAsString();
    auto *tempVar =
        spvBuilder.addFnVar(paramType, param->getLocation(), tempVarName,
                            param->hasAttr<HLSLPreciseAttr>());

    params.push_back(tempVar);

    // Create the stage input variable for parameter not marked as pure out and
    // initialize the corresponding temporary variable
    // Also do not create input variables for output stream objects of geometry
    // shaders (e.g. TriangleStream) which are required to be marked as 'inout'.
    if (canActAsInParmVar(param)) {
      if (spvContext.isHS() && hlsl::IsHLSLInputPatchType(paramType)) {
        // Record the temporary variable holding InputPatch. It may be used
        // later in the patch constant function.
        hullMainInputPatchParam = tempVar;
      }

      SpirvInstruction *loadedValue = nullptr;

      if (!declIdMapper.createStageInputVar(param, &loadedValue, false))
        return false;

      // Only initialize the temporary variable if the parameter is indeed used.
      if (param->isUsed()) {
        spvBuilder.createStore(tempVar, loadedValue, param->getLocation());
      }

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
  const QualType retType = decl->getReturnType();
  auto *retVal = spvBuilder.createFunctionCall(retType, entryFuncInstr, params,
                                               decl->getLocStart());

  // Create and write stage output variables for return value. Special case for
  // Hull shaders since they operate differently in 2 ways:
  // 1- Their return value is in fact an array and each invocation should write
  //    to the proper offset in the array.
  // 2- The patch constant function must be called *once* after all invocations
  //    of the main entry point function is done.
  if (spvContext.isHS()) {
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
      SpirvInstruction *loadedParam = nullptr;

      // No need to write back the value if the parameter is not used at all in
      // the original entry function.
      //
      // Write back of stage output variables in GS is manually controlled by
      // .Append() intrinsic method. No need to load the parameter since we
      // won't need to write back here.
      if (param->isUsed() && !spvContext.isGS())
        loadedParam = spvBuilder.createLoad(param->getType(), params[i],
                                            param->getLocStart());

      if (!declIdMapper.createStageOutputVar(param, loadedParam, false))
        return false;
    }
  }

  // For wrapper of entry point, it is better not to specify SourceLocation
  // for return statement, because it is not the location of the actual
  // return and emitting the location of the end of entry function makes
  // us confused. It is better to emit debug line just before OpFunctionEnd.
  spvBuilder.createReturn(/* SourceLocation */ {});
  spvBuilder.endFunction();

  // For Hull shaders, there is no explicit call to the PCF in the HLSL source.
  // We should invoke a translation of the PCF manually.
  if (spvContext.isHS())
    doDecl(patchConstFunc);

  return true;
}

bool SpirvEmitter::processHSEntryPointOutputAndPCF(
    const FunctionDecl *hullMainFuncDecl, QualType retType,
    SpirvInstruction *retVal, uint32_t numOutputControlPoints,
    SpirvInstruction *outputControlPointId, SpirvInstruction *primitiveId,
    SpirvInstruction *viewId, SpirvInstruction *hullMainInputPatch) {
  // This method may only be called for Hull shaders.
  assert(spvContext.isHS());

  auto loc = hullMainFuncDecl->getLocation();
  auto locEnd = hullMainFuncDecl->getLocEnd();

  // For Hull shaders, the real output is an array of size
  // numOutputControlPoints. The results of the main should be written to the
  // correct offset in the array (based on InvocationID).
  if (!numOutputControlPoints) {
    emitError("number of output control points cannot be zero", loc);
    return false;
  }
  // TODO: We should be able to handle cases where the SV_OutputControlPointID
  // is not provided.
  if (!outputControlPointId) {
    emitError(
        "SV_OutputControlPointID semantic must be provided in hull shader",
        loc);
    return false;
  }
  if (!patchConstFunc) {
    emitError("patch constant function not defined in hull shader", loc);
    return false;
  }

  SpirvInstruction *hullMainOutputPatch = nullptr;
  // If the patch constant function (PCF) takes the result of the Hull main
  // entry point, create a temporary function-scope variable and write the
  // results to it, so it can be passed to the PCF.
  if (patchConstFuncTakesHullOutputPatch(patchConstFunc)) {
    const QualType hullMainRetType = astContext.getConstantArrayType(
        retType, llvm::APInt(32, numOutputControlPoints),
        clang::ArrayType::Normal, 0);
    hullMainOutputPatch =
        spvBuilder.addFnVar(hullMainRetType, locEnd, "temp.var.hullMainRetVal");
    auto *tempLocation = spvBuilder.createAccessChain(
        retType, hullMainOutputPatch, {outputControlPointId}, locEnd);
    spvBuilder.createStore(tempLocation, retVal, locEnd);
  }

  // Now create a barrier before calling the Patch Constant Function (PCF).
  // Flags are:
  // Execution Barrier scope = Workgroup (2)
  // Memory Barrier scope = Invocation (4)
  // Memory Semantics Barrier scope = None (0)
  spvBuilder.createBarrier(spv::Scope::Invocation,
                           spv::MemorySemanticsMask::MaskNone,
                           spv::Scope::Workgroup, {});

  // The PCF should be called only once. Therefore, we check the invocationID,
  // and we only allow ID 0 to call the PCF.
  auto *condition = spvBuilder.createBinaryOp(
      spv::Op::OpIEqual, astContext.BoolTy, outputControlPointId,
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0)),
      loc);
  auto *thenBB = spvBuilder.createBasicBlock("if.true");
  auto *mergeBB = spvBuilder.createBasicBlock("if.merge");
  spvBuilder.createConditionalBranch(condition, thenBB, mergeBB, loc, mergeBB);
  spvBuilder.addSuccessor(thenBB);
  spvBuilder.addSuccessor(mergeBB);
  spvBuilder.setMergeTarget(mergeBB);

  spvBuilder.setInsertPoint(thenBB);

  // Call the PCF. Since the function is not explicitly called, we must first
  // register an ID for it.
  SpirvFunction *pcfId = declIdMapper.getOrRegisterFn(patchConstFunc);
  const QualType pcfRetType = patchConstFunc->getReturnType();

  std::vector<SpirvInstruction *> pcfParams;

  // A lambda for creating a stage input variable and its associated temporary
  // variable for function call. Also initializes the temporary variable using
  // the contents loaded from the stage input variable. Returns the <result-id>
  // of the temporary variable.
  const auto createParmVarAndInitFromStageInputVar =
      [this](const ParmVarDecl *param) {
        const QualType type = param->getType();
        std::string tempVarName = "param.var." + param->getNameAsString();
        auto paramLoc = param->getLocation();
        auto *tempVar = spvBuilder.addFnVar(type, paramLoc, tempVarName,
                                            param->hasAttr<HLSLPreciseAttr>());
        SpirvInstruction *loadedValue = nullptr;
        declIdMapper.createStageInputVar(param, &loadedValue, /*forPCF*/ true);
        spvBuilder.createStore(tempVar, loadedValue, paramLoc);
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
  auto *pcfResultId = spvBuilder.createFunctionCall(
      pcfRetType, pcfId, {pcfParams}, hullMainFuncDecl->getLocStart());
  if (!declIdMapper.createStageOutputVar(patchConstFunc, pcfResultId,
                                         /*forPCF*/ true))
    return false;

  spvBuilder.createBranch(mergeBB, locEnd);
  spvBuilder.addSuccessor(mergeBB);
  spvBuilder.setInsertPoint(mergeBB);
  return true;
}

bool SpirvEmitter::allSwitchCasesAreIntegerLiterals(const Stmt *root) {
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

void SpirvEmitter::discoverAllCaseStmtInSwitchStmt(
    const Stmt *root, SpirvBasicBlock **defaultBB,
    std::vector<std::pair<uint32_t, SpirvBasicBlock *>> *targets) {
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
  auto *caseBB = spvBuilder.createBasicBlock(caseLabel);
  spvBuilder.addSuccessor(caseBB);
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

void SpirvEmitter::flattenSwitchStmtAST(const Stmt *root,
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

void SpirvEmitter::processCaseStmtOrDefaultStmt(const Stmt *stmt) {
  auto *caseStmt = dyn_cast<CaseStmt>(stmt);
  auto *defaultStmt = dyn_cast<DefaultStmt>(stmt);
  assert(caseStmt || defaultStmt);

  auto *caseBB = stmtBasicBlock[stmt];
  if (!spvBuilder.isCurrentBasicBlockTerminated()) {
    // We are about to handle the case passed in as parameter. If the current
    // basic block is not terminated, it means the previous case is a fall
    // through case. We need to link it to the case to be processed.
    spvBuilder.createBranch(caseBB, stmt->getLocStart());
    spvBuilder.addSuccessor(caseBB);
  }
  spvBuilder.setInsertPoint(caseBB);
  doStmt(caseStmt ? caseStmt->getSubStmt() : defaultStmt->getSubStmt());
}

void SpirvEmitter::processSwitchStmtUsingSpirvOpSwitch(
    const SwitchStmt *switchStmt) {
  const SourceLocation srcLoc = switchStmt->getSwitchLoc();

  // First handle the condition variable DeclStmt if one exists.
  // For example: handle 'int a = b' in the following:
  // switch (int a = b) {...}
  if (const auto *condVarDeclStmt = switchStmt->getConditionVariableDeclStmt())
    doDeclStmt(condVarDeclStmt);

  auto *selector = doExpr(switchStmt->getCond());

  // We need a merge block regardless of the number of switch cases.
  // Since OpSwitch always requires a default label, if the switch statement
  // does not have a default branch, we use the merge block as the default
  // target.
  auto *mergeBB = spvBuilder.createBasicBlock("switch.merge");
  spvBuilder.setMergeTarget(mergeBB);
  breakStack.push(mergeBB);
  auto *defaultBB = mergeBB;

  // (literal, labelId) pairs to pass to the OpSwitch instruction.
  std::vector<std::pair<uint32_t, SpirvBasicBlock *>> targets;
  discoverAllCaseStmtInSwitchStmt(switchStmt->getBody(), &defaultBB, &targets);

  // Create the OpSelectionMerge and OpSwitch.
  spvBuilder.createSwitch(mergeBB, selector, defaultBB, targets, srcLoc);

  // Handle the switch body.
  doStmt(switchStmt->getBody());

  if (!spvBuilder.isCurrentBasicBlockTerminated())
    spvBuilder.createBranch(mergeBB, switchStmt->getLocEnd());
  spvBuilder.setInsertPoint(mergeBB);
  breakStack.pop();
}

void SpirvEmitter::processSwitchStmtUsingIfStmts(const SwitchStmt *switchStmt) {
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

SpirvInstruction *SpirvEmitter::extractVecFromVec4(SpirvInstruction *from,
                                                   uint32_t targetVecSize,
                                                   QualType targetElemType,
                                                   SourceLocation loc) {
  assert(targetVecSize > 0 && targetVecSize < 5);
  const QualType retType =
      targetVecSize == 1
          ? targetElemType
          : astContext.getExtVectorType(targetElemType, targetVecSize);
  switch (targetVecSize) {
  case 1:
    return spvBuilder.createCompositeExtract(retType, from, {0}, loc);
    break;
  case 2:
    return spvBuilder.createVectorShuffle(retType, from, from, {0, 1}, loc);
    break;
  case 3:
    return spvBuilder.createVectorShuffle(retType, from, from, {0, 1, 2}, loc);
    break;
  case 4:
    return from;
  default:
    llvm_unreachable("vector element count must be 1, 2, 3, or 4");
  }
}

void SpirvEmitter::addFunctionToWorkQueue(hlsl::DXIL::ShaderKind shaderKind,
                                          const clang::FunctionDecl *fnDecl,
                                          bool isEntryFunction) {
  // Only update the workQueue and the function info map if the given
  // FunctionDecl hasn't been added already.
  if (functionInfoMap.find(fnDecl) == functionInfoMap.end()) {
    // Note: The function is just discovered and is being added to the
    // workQueue, therefore it does not have the entryFunction SPIR-V
    // instruction yet (use nullptr).
    auto *fnInfo = new (spvContext) FunctionInfo(
        shaderKind, fnDecl, /*entryFunction*/ nullptr, isEntryFunction);
    functionInfoMap[fnDecl] = fnInfo;
    workQueue.push_back(fnInfo);
  }
}

} // end namespace spirv
} // end namespace clang
