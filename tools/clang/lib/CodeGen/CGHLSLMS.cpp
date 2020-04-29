//===----- CGHLSLMS.cpp - Interface to HLSL Runtime ----------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CGHLSLMS.cpp                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//  This provides a class for HLSL code generation.                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "CGHLSLRuntime.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "CGRecordLayout.h"
#include "dxc/HlslIntrinsicOp.h"
#include "dxc/HLSL/HLMatrixType.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilTypeSystem.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/HlslTypes.h"
#include "clang/Frontend/CodeGenOptions.h"
#include "clang/Lex/HLSLMacroExpander.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/InstIterator.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <set>

#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include "dxc/DXIL/DxilCBuffer.h"
#include "clang/Parse/ParseHLSL.h"      // root sig would be in Parser if part of lang
#include "dxc/Support/WinIncludes.h"    // stream support
#include "dxc/dxcapi.h"                 // stream support
#include "dxc/HLSL/HLSLExtensionsCodegenHelper.h"
#include "dxc/HLSL/DxilGenerationPass.h" // support pause/resume passes
#include "dxc/HLSL/DxilExportMap.h"
#include "dxc/DXIL/DxilResourceProperties.h"

#include "CGHLSLMSHelper.h"

using namespace clang;
using namespace CodeGen;
using namespace hlsl;
using namespace llvm;
using std::unique_ptr;
using namespace CGHLSLMSHelper;

static const bool KeepUndefinedTrue = true; // Keep interpolation mode undefined if not set explicitly.

namespace {

class CGMSHLSLRuntime : public CGHLSLRuntime {

private:
  /// Convenience reference to LLVM Context
  llvm::LLVMContext &Context;
  /// Convenience reference to the current module
  llvm::Module &TheModule;

  HLModule *m_pHLModule;
  llvm::Type *CBufferType;
  uint32_t globalCBIndex;
  // TODO: make sure how minprec works
  llvm::DataLayout dataLayout;
  // decl map to constant id for program
  llvm::DenseMap<HLSLBufferDecl *, uint32_t> constantBufMap;
  // Map for resource type to resource metadata value.
  std::unordered_map<llvm::Type *, MDNode*> resMetadataMap;
  // Map from Constant to register bindings.
  llvm::DenseMap<llvm::Constant *,
                 llvm::SmallVector<std::pair<DXIL::ResourceClass, unsigned>, 1>>
      constantRegBindingMap;

  // Map from value to resource properties.
  // This only collect object variables(global/local/parameter), not object fields inside struct.
  // Object fields inside struct is saved by TypeAnnotation.
  DenseMap<Value *, DxilResourceProperties> valToResPropertiesMap;

  bool  m_bDebugInfo;
  bool  m_bIsLib;

  // For library, m_ExportMap maps from internal name to zero or more renames
  dxilutil::ExportMap m_ExportMap;

  HLCBuffer &GetGlobalCBuffer() {
    return *static_cast<HLCBuffer*>(&(m_pHLModule->GetCBuffer(globalCBIndex)));
  }
  void AddConstant(VarDecl *constDecl, HLCBuffer &CB);
  uint32_t AddSampler(VarDecl *samplerDecl);
  uint32_t AddUAVSRV(VarDecl *decl, hlsl::DxilResourceBase::Class resClass);
  bool SetUAVSRV(SourceLocation loc, hlsl::DxilResourceBase::Class resClass,
                 DxilResource *hlslRes, QualType QualTy);
  uint32_t AddCBuffer(HLSLBufferDecl *D);
  hlsl::DxilResourceBase::Class TypeToClass(clang::QualType Ty);

  void CreateSubobject(DXIL::SubobjectKind kind, const StringRef name, clang::Expr **args,
                       unsigned int argCount, DXIL::HitGroupType hgType = (DXIL::HitGroupType)(-1));
  bool GetAsConstantString(clang::Expr *expr, StringRef *value, bool failWhenEmpty = false);
  bool GetAsConstantUInt32(clang::Expr *expr, uint32_t *value);
  std::vector<StringRef> ParseSubobjectExportsAssociations(StringRef exports);

  EntryFunctionInfo Entry;

  StringMap<PatchConstantInfo> patchConstantFunctionMap;
  std::unordered_map<Function *, std::unique_ptr<DxilFunctionProps>>
      patchConstantFunctionPropsMap;

  std::unordered_map<Function *, const clang::HLSLPatchConstantFuncAttr *>
      HSEntryPatchConstantFuncAttr;

  // Map to save entry functions.
  StringMap<EntryFunctionInfo> entryFunctionMap;

  // Map to save static global init exp.
  std::unordered_map<Expr *, GlobalVariable *> staticConstGlobalInitMap;
  std::unordered_map<GlobalVariable *, std::vector<Constant *>>
      staticConstGlobalInitListMap;
  std::unordered_map<GlobalVariable *, Function *> staticConstGlobalCtorMap;
  // List for functions with clip plane.
  std::vector<Function *> clipPlaneFuncList;
  std::unordered_map<Value *, DebugLoc> debugInfoMap;

  DxilRootSignatureVersion  rootSigVer;

  Value *EmitHLSLMatrixLoad(CGBuilderTy &Builder, Value *Ptr, QualType Ty);
  void EmitHLSLMatrixStore(CGBuilderTy &Builder, Value *Val, Value *DestPtr,
                           QualType Ty);
  // Flatten the val into scalar val and push into elts and eltTys.
  void FlattenValToInitList(CodeGenFunction &CGF, SmallVector<Value *, 4> &elts,
                       SmallVector<QualType, 4> &eltTys, QualType Ty,
                       Value *val);
  // Push every value on InitListExpr into EltValList and EltTyList.
  void ScanInitList(CodeGenFunction &CGF, InitListExpr *E,
                    SmallVector<Value *, 4> &EltValList,
                    SmallVector<QualType, 4> &EltTyList);

  void FlattenAggregatePtrToGepList(CodeGenFunction &CGF, Value *Ptr,
                                    SmallVector<Value *, 4> &idxList,
                                    clang::QualType Type, llvm::Type *Ty,
                                    SmallVector<Value *, 4> &GepList,
                                    SmallVector<QualType, 4> &EltTyList);
  void LoadElements(CodeGenFunction &CGF,
    ArrayRef<Value *> Ptrs, ArrayRef<QualType> QualTys,
    SmallVector<Value *, 4> &Vals);
  void ConvertAndStoreElements(CodeGenFunction &CGF,
    ArrayRef<Value *> SrcVals, ArrayRef<QualType> SrcQualTys,
    ArrayRef<Value *> DstPtrs, ArrayRef<QualType> DstQualTys);

  void EmitHLSLAggregateCopy(CodeGenFunction &CGF, llvm::Value *SrcPtr,
                                   llvm::Value *DestPtr,
                                   SmallVector<Value *, 4> &idxList,
                                   clang::QualType SrcType,
                                   clang::QualType DestType,
                                   llvm::Type *Ty);

  void EmitHLSLSplat(CodeGenFunction &CGF, Value *SrcVal,
                              llvm::Value *DestPtr,
                              SmallVector<Value *, 4> &idxList,
                              QualType Type, QualType SrcType,
                              llvm::Type *Ty);

  void EmitHLSLRootSignature(CodeGenFunction &CGF, HLSLRootSignatureAttr *RSA,
                             llvm::Function *Fn) override;

  void CheckParameterAnnotation(SourceLocation SLoc,
                                const DxilParameterAnnotation &paramInfo,
                                bool isPatchConstantFunction);
  void CheckParameterAnnotation(SourceLocation SLoc,
                                DxilParamInputQual paramQual,
                                llvm::StringRef semFullName,
                                bool isPatchConstantFunction);

  void RemapObsoleteSemantic(DxilParameterAnnotation &paramInfo,
                             bool isPatchConstantFunction);
  SourceLocation SetSemantic(const NamedDecl *decl,
                             DxilParameterAnnotation &paramInfo);

  hlsl::InterpolationMode GetInterpMode(const Decl *decl, CompType compType,
                                        bool bKeepUndefined);
  hlsl::CompType GetCompType(const BuiltinType *BT);
  // save intrinsic opcode
  std::vector<std::pair<Function *, unsigned>> m_IntrinsicMap;
  void AddHLSLIntrinsicOpcodeToFunction(Function *, unsigned opcode);

  // Type annotation related.
  unsigned ConstructStructAnnotation(DxilStructAnnotation *annotation,
                                     const RecordDecl *RD,
                                     DxilTypeSystem &dxilTypeSys);
  unsigned AddTypeAnnotation(QualType Ty, DxilTypeSystem &dxilTypeSys,
                             unsigned &arrayEltSize);
  MDNode *GetOrAddResTypeMD(QualType resTy, bool bCreate);
  DxilResourceProperties BuildResourceProperty(QualType resTy);
  void ConstructFieldAttributedAnnotation(DxilFieldAnnotation &fieldAnnotation,
                                          QualType fieldTy,
                                          bool bDefaultRowMajor);

  std::unordered_map<Constant*, DxilFieldAnnotation> m_ConstVarAnnotationMap;
  StringSet<> m_PreciseOutputSet;

public:
  CGMSHLSLRuntime(CodeGenModule &CGM);

  /// Add resouce to the program
  void addResource(Decl *D) override;

  void addSubobject(Decl *D) override;

  void FinishCodeGen() override;
  bool IsTrivalInitListExpr(CodeGenFunction &CGF, InitListExpr *E) override;
  Value *EmitHLSLInitListExpr(CodeGenFunction &CGF, InitListExpr *E, Value *DestPtr) override;
  Constant *EmitHLSLConstInitListExpr(CodeGenModule &CGM, InitListExpr *E) override;

  RValue EmitHLSLBuiltinCallExpr(CodeGenFunction &CGF, const FunctionDecl *FD,
                                 const CallExpr *E,
                                 ReturnValueSlot ReturnValue) override;
  void EmitHLSLOutParamConversionInit(
      CodeGenFunction &CGF, const FunctionDecl *FD, const CallExpr *E,
      llvm::SmallVector<LValue, 8> &castArgList,
      llvm::SmallVector<const Stmt *, 8> &argList,
      const std::function<void(const VarDecl *, llvm::Value *)> &TmpArgMap)
      override;
  void EmitHLSLOutParamConversionCopyBack(
      CodeGenFunction &CGF, llvm::SmallVector<LValue, 8> &castArgList) override;

  Value *EmitHLSLMatrixOperationCall(CodeGenFunction &CGF, const clang::Expr *E,
                                     llvm::Type *RetType,
                                     ArrayRef<Value *> paramList) override;

  void EmitHLSLDiscard(CodeGenFunction &CGF) override;
  void EmitHLSLCondBreak(CodeGenFunction &CGF, llvm::Function *F, llvm::BasicBlock *DestBB, llvm::BasicBlock *AltBB) override;

  Value *EmitHLSLMatrixSubscript(CodeGenFunction &CGF, llvm::Type *RetType,
                                 Value *Ptr, Value *Idx, QualType Ty) override;

  Value *EmitHLSLMatrixElement(CodeGenFunction &CGF, llvm::Type *RetType,
                               ArrayRef<Value *> paramList,
                               QualType Ty) override;

  Value *EmitHLSLMatrixLoad(CodeGenFunction &CGF, Value *Ptr,
                            QualType Ty) override;
  void EmitHLSLMatrixStore(CodeGenFunction &CGF, Value *Val, Value *DestPtr,
                           QualType Ty) override;

  void EmitHLSLAggregateCopy(CodeGenFunction &CGF, llvm::Value *SrcPtr,
                                   llvm::Value *DestPtr,
                                   clang::QualType Ty) override;

  void EmitHLSLAggregateStore(CodeGenFunction &CGF, llvm::Value *Val,
                                   llvm::Value *DestPtr,
                                   clang::QualType Ty) override;

  void EmitHLSLFlatConversion(CodeGenFunction &CGF, Value *Val,
                              Value *DestPtr,
                              QualType Ty,
                              QualType SrcTy) override;
  Value *EmitHLSLLiteralCast(CodeGenFunction &CGF, Value *Src, QualType SrcType,
                             QualType DstType) override;

  void EmitHLSLFlatConversionAggregateCopy(CodeGenFunction &CGF, llvm::Value *SrcPtr,
                                   clang::QualType SrcTy,
                                   llvm::Value *DestPtr,
                                   clang::QualType DestTy) override;
  void AddHLSLFunctionInfo(llvm::Function *, const FunctionDecl *FD) override;
  void EmitHLSLFunctionProlog(llvm::Function *, const FunctionDecl *FD) override;

  void AddControlFlowHint(CodeGenFunction &CGF, const Stmt &S,
                          llvm::TerminatorInst *TI,
                          ArrayRef<const Attr *> Attrs) override;
  void MarkRetTemp(CodeGenFunction &CGF, llvm::Value *V,
                  clang::QualType QaulTy) override;
  void FinishAutoVar(CodeGenFunction &CGF, const VarDecl &D, llvm::Value *V) override;

  /// Get or add constant to the program
  HLCBuffer &GetOrCreateCBuffer(HLSLBufferDecl *D);
};
}

//------------------------------------------------------------------------------
//
// CGMSHLSLRuntime methods.
//
CGMSHLSLRuntime::CGMSHLSLRuntime(CodeGenModule &CGM)
    : CGHLSLRuntime(CGM), Context(CGM.getLLVMContext()),
      TheModule(CGM.getModule()),
      CBufferType(
          llvm::StructType::create(TheModule.getContext(), "ConstantBuffer")),
      dataLayout(CGM.getLangOpts().UseMinPrecision
                       ? hlsl::DXIL::kLegacyLayoutString
                       : hlsl::DXIL::kNewLayoutString),  Entry() {

  const hlsl::ShaderModel *SM =
      hlsl::ShaderModel::GetByName(CGM.getCodeGenOpts().HLSLProfile.c_str());
  // Only accept valid, 6.0 shader model.
  if (!SM->IsValid() || SM->GetMajor() != 6) {
    DiagnosticsEngine &Diags = CGM.getDiags();
    unsigned DiagID =
        Diags.getCustomDiagID(DiagnosticsEngine::Error, "invalid profile %0");
    Diags.Report(DiagID) << CGM.getCodeGenOpts().HLSLProfile;
    return;
  }
  if (CGM.getCodeGenOpts().HLSLValidatorMajorVer != 0) {
    // Check validator version against minimum for target profile:
    unsigned MinMajor, MinMinor;
    SM->GetMinValidatorVersion(MinMajor, MinMinor);
    if (DXIL::CompareVersions(CGM.getCodeGenOpts().HLSLValidatorMajorVer,
                              CGM.getCodeGenOpts().HLSLValidatorMinorVer,
                              MinMajor, MinMinor) < 0) {
      DiagnosticsEngine &Diags = CGM.getDiags();
      unsigned DiagID =
          Diags.getCustomDiagID(DiagnosticsEngine::Error,
            "validator version %0,%1 does not support target profile.");
      Diags.Report(DiagID) << CGM.getCodeGenOpts().HLSLValidatorMajorVer
                           << CGM.getCodeGenOpts().HLSLValidatorMinorVer;
      return;
    }
  }
  m_bIsLib = SM->IsLib();
  // TODO: add AllResourceBound.
  if (CGM.getCodeGenOpts().HLSLAvoidControlFlow && !CGM.getCodeGenOpts().HLSLAllResourcesBound) {
    if (SM->IsSM51Plus()) {
      DiagnosticsEngine &Diags = CGM.getDiags();
      unsigned DiagID =
          Diags.getCustomDiagID(DiagnosticsEngine::Error,
                                "Gfa option cannot be used in SM_5_1+ unless "
                                "all_resources_bound flag is specified");
      Diags.Report(DiagID);
    }
  }
  // Create HLModule.
  const bool skipInit = true;
  m_pHLModule = &TheModule.GetOrCreateHLModule(skipInit);

  // Precise Output.
  for (auto &preciseOutput : CGM.getCodeGenOpts().HLSLPreciseOutputs) {
    m_PreciseOutputSet.insert(StringRef(preciseOutput).lower());
  }

  // Set Option.
  HLOptions opts;
  opts.bIEEEStrict = CGM.getCodeGenOpts().UnsafeFPMath;
  opts.bDefaultRowMajor = CGM.getCodeGenOpts().HLSLDefaultRowMajor;
  opts.bDisableOptimizations = CGM.getCodeGenOpts().DisableLLVMOpts;
  opts.bLegacyCBufferLoad = !CGM.getCodeGenOpts().HLSLNotUseLegacyCBufLoad;
  opts.bAllResourcesBound = CGM.getCodeGenOpts().HLSLAllResourcesBound;
  opts.PackingStrategy = CGM.getCodeGenOpts().HLSLSignaturePackingStrategy;
  opts.bLegacyResourceReservation = CGM.getCodeGenOpts().HLSLLegacyResourceReservation;

  opts.bUseMinPrecision = CGM.getLangOpts().UseMinPrecision;
  opts.bDX9CompatMode = CGM.getLangOpts().EnableDX9CompatMode;
  opts.bFXCCompatMode = CGM.getLangOpts().EnableFXCCompatMode;

  m_pHLModule->SetHLOptions(opts);
  m_pHLModule->GetOP()->SetMinPrecision(opts.bUseMinPrecision);
  m_pHLModule->GetTypeSystem().SetMinPrecision(opts.bUseMinPrecision);

  m_pHLModule->SetAutoBindingSpace(CGM.getCodeGenOpts().HLSLDefaultSpace);

  m_pHLModule->SetValidatorVersion(CGM.getCodeGenOpts().HLSLValidatorMajorVer, CGM.getCodeGenOpts().HLSLValidatorMinorVer);

  m_bDebugInfo = CGM.getCodeGenOpts().getDebugInfo() == CodeGenOptions::FullDebugInfo;

  // set profile
  m_pHLModule->SetShaderModel(SM);
  // set entry name
  if (!SM->IsLib())
    m_pHLModule->SetEntryFunctionName(CGM.getCodeGenOpts().HLSLEntryFunction);

  // set root signature version.
  if (CGM.getLangOpts().RootSigMinor == 0) {
    rootSigVer = hlsl::DxilRootSignatureVersion::Version_1_0;
  }
  else {
    DXASSERT(CGM.getLangOpts().RootSigMinor == 1,
      "else CGMSHLSLRuntime Constructor needs to be updated");
    rootSigVer = hlsl::DxilRootSignatureVersion::Version_1_1;
  }

  DXASSERT(CGM.getLangOpts().RootSigMajor == 1,
           "else CGMSHLSLRuntime Constructor needs to be updated");

  // add globalCB
  unique_ptr<HLCBuffer> CB = llvm::make_unique<HLCBuffer>();
  std::string globalCBName = "$Globals";
  CB->SetGlobalSymbol(nullptr);
  CB->SetGlobalName(globalCBName);
  globalCBIndex = m_pHLModule->GetCBuffers().size();
  CB->SetID(globalCBIndex);
  CB->SetRangeSize(1);
  CB->SetLowerBound(UINT_MAX);
  DXVERIFY_NOMSG(globalCBIndex == m_pHLModule->AddCBuffer(std::move(CB)));

  // set Float Denorm Mode
  m_pHLModule->SetFloat32DenormMode(CGM.getCodeGenOpts().HLSLFloat32DenormMode);

  // set DefaultLinkage
  m_pHLModule->SetDefaultLinkage(CGM.getCodeGenOpts().DefaultLinkage);

  // Fill in m_ExportMap, which maps from internal name to zero or more renames
  m_ExportMap.clear();
  std::string errors;
  llvm::raw_string_ostream os(errors);
  if (!m_ExportMap.ParseExports(CGM.getCodeGenOpts().HLSLLibraryExports, os)) {
    DiagnosticsEngine &Diags = CGM.getDiags();
    unsigned DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error, "Error parsing -exports options: %0");
    Diags.Report(DiagID) << os.str();
  }
}


void CGMSHLSLRuntime::AddHLSLIntrinsicOpcodeToFunction(Function *F,
                                                       unsigned opcode) {
  m_IntrinsicMap.emplace_back(F,opcode);
}

void CGMSHLSLRuntime::CheckParameterAnnotation(
    SourceLocation SLoc, const DxilParameterAnnotation &paramInfo,
    bool isPatchConstantFunction) {
  if (!paramInfo.HasSemanticString()) {
    return;
  }
  llvm::StringRef semFullName = paramInfo.GetSemanticStringRef();
  DxilParamInputQual paramQual = paramInfo.GetParamInputQual();
  if (paramQual == DxilParamInputQual::Inout) {
    CheckParameterAnnotation(SLoc, DxilParamInputQual::In, semFullName, isPatchConstantFunction);
    CheckParameterAnnotation(SLoc, DxilParamInputQual::Out, semFullName, isPatchConstantFunction);
    return;
  }
  CheckParameterAnnotation(SLoc, paramQual, semFullName, isPatchConstantFunction);
}

void CGMSHLSLRuntime::CheckParameterAnnotation(
    SourceLocation SLoc, DxilParamInputQual paramQual, llvm::StringRef semFullName,
    bool isPatchConstantFunction) {
  const ShaderModel *SM = m_pHLModule->GetShaderModel();

  DXIL::SigPointKind sigPoint = SigPointFromInputQual(
    paramQual, SM->GetKind(), isPatchConstantFunction);

  llvm::StringRef semName;
  unsigned semIndex;
  Semantic::DecomposeNameAndIndex(semFullName, &semName, &semIndex);

  const Semantic *pSemantic =
      Semantic::GetByName(semName, sigPoint, SM->GetMajor(), SM->GetMinor());
  if (pSemantic->IsInvalid()) {
    DiagnosticsEngine &Diags = CGM.getDiags();
    unsigned DiagID =
      Diags.getCustomDiagID(DiagnosticsEngine::Error, "invalid semantic '%0' for %1 %2.%3");
    Diags.Report(SLoc, DiagID) << semName << SM->GetKindName() << SM->GetMajor() << SM->GetMinor();
  }
}

SourceLocation
CGMSHLSLRuntime::SetSemantic(const NamedDecl *decl,
                             DxilParameterAnnotation &paramInfo) {
  for (const hlsl::UnusualAnnotation *it : decl->getUnusualAnnotations()) {
    if (it->getKind() == hlsl::UnusualAnnotation::UA_SemanticDecl) {
      const hlsl::SemanticDecl *sd = cast<hlsl::SemanticDecl>(it);
      paramInfo.SetSemanticString(sd->SemanticName);
      if (m_PreciseOutputSet.count(StringRef(sd->SemanticName).lower()))
        paramInfo.SetPrecise();
      return it->Loc;
    }
  }
  return SourceLocation();
}

static DXIL::TessellatorDomain StringToDomain(StringRef domain) {
  if (domain == "isoline")
    return DXIL::TessellatorDomain::IsoLine;
  if (domain == "tri")
    return DXIL::TessellatorDomain::Tri;
  if (domain == "quad")
    return DXIL::TessellatorDomain::Quad;
  return DXIL::TessellatorDomain::Undefined;
}

static DXIL::TessellatorPartitioning StringToPartitioning(StringRef partition) {
  if (partition == "integer")
    return DXIL::TessellatorPartitioning::Integer;
  if (partition == "pow2")
    return DXIL::TessellatorPartitioning::Pow2;
  if (partition == "fractional_even")
    return DXIL::TessellatorPartitioning::FractionalEven;
  if (partition == "fractional_odd")
    return DXIL::TessellatorPartitioning::FractionalOdd;
  return DXIL::TessellatorPartitioning::Undefined;
}

static DXIL::TessellatorOutputPrimitive
StringToTessOutputPrimitive(StringRef primitive) {
  if (primitive == "point")
    return DXIL::TessellatorOutputPrimitive::Point;
  if (primitive == "line")
    return DXIL::TessellatorOutputPrimitive::Line;
  if (primitive == "triangle_cw")
    return DXIL::TessellatorOutputPrimitive::TriangleCW;
  if (primitive == "triangle_ccw")
    return DXIL::TessellatorOutputPrimitive::TriangleCCW;
  return DXIL::TessellatorOutputPrimitive::Undefined;
}

static DXIL::MeshOutputTopology
StringToMeshOutputTopology(StringRef topology) {
  if (topology == "line")
    return DXIL::MeshOutputTopology::Line;
  if (topology == "triangle")
    return DXIL::MeshOutputTopology::Triangle;
  return DXIL::MeshOutputTopology::Undefined;
}

static unsigned GetMatrixSizeInCB(QualType Ty, bool defaultRowMajor,
                                  bool b64Bit) {
  bool bRowMajor;
  if (!hlsl::HasHLSLMatOrientation(Ty, &bRowMajor))
    bRowMajor = defaultRowMajor;

  unsigned row, col;
  hlsl::GetHLSLMatRowColCount(Ty, row, col);

  unsigned EltSize = b64Bit ? 8 : 4;

  // Align to 4 * 4bytes.
  unsigned alignment = 4 * 4;

  if (bRowMajor) {
    unsigned rowSize = EltSize * col;
    // 3x64bit or 4x64bit align to 32 bytes.
    if (rowSize > alignment)
      alignment <<= 1;
    return alignment * (row - 1) + col * EltSize;
  } else {
    unsigned rowSize = EltSize * row;
    // 3x64bit or 4x64bit align to 32 bytes.
    if (rowSize > alignment)
      alignment <<= 1;

    return alignment * (col - 1) + row * EltSize;
  }
}

static CompType::Kind BuiltinTyToCompTy(const BuiltinType *BTy, bool bSNorm,
                                        bool bUNorm) {
  CompType::Kind kind = CompType::Kind::Invalid;

  switch (BTy->getKind()) {
  case BuiltinType::UInt:
    kind = CompType::Kind::U32;
    break;
  case BuiltinType::Min16UInt: // HLSL Change
  case BuiltinType::UShort:
    kind = CompType::Kind::U16;
    break;
  case BuiltinType::ULongLong:
    kind = CompType::Kind::U64;
    break;
  case BuiltinType::Int:
    kind = CompType::Kind::I32;
    break;
  // HLSL Changes begin
  case BuiltinType::Min12Int:
  case BuiltinType::Min16Int:
  // HLSL Changes end
  case BuiltinType::Short:
    kind = CompType::Kind::I16;
    break;
  case BuiltinType::LongLong:
    kind = CompType::Kind::I64;
    break;
  // HLSL Changes begin
  case BuiltinType::Min10Float:
  case BuiltinType::Min16Float:
  // HLSL Changes end
  case BuiltinType::Half:
    if (bSNorm)
      kind = CompType::Kind::SNormF16;
    else if (bUNorm)
      kind = CompType::Kind::UNormF16;
    else
      kind = CompType::Kind::F16;
    break;
  case BuiltinType::HalfFloat: // HLSL Change
  case BuiltinType::Float:
    if (bSNorm)
      kind = CompType::Kind::SNormF32;
    else if (bUNorm)
      kind = CompType::Kind::UNormF32;
    else
      kind = CompType::Kind::F32;
    break;
  case BuiltinType::Double:
    if (bSNorm)
      kind = CompType::Kind::SNormF64;
    else if (bUNorm)
      kind = CompType::Kind::UNormF64;
    else
      kind = CompType::Kind::F64;
    break;
  case BuiltinType::Bool:
    kind = CompType::Kind::I1;
    break;
  default:
    // Other types not used by HLSL.
    break;
  }
  return kind;
}

static DxilSampler::SamplerKind KeywordToSamplerKind(llvm::StringRef keyword) {
  // TODO: refactor for faster search (switch by 1/2/3 first letters, then
  // compare)
  return llvm::StringSwitch<DxilSampler::SamplerKind>(keyword)
    .Case("SamplerState", DxilSampler::SamplerKind::Default)
    .Case("SamplerComparisonState", DxilSampler::SamplerKind::Comparison)
    .Default(DxilSampler::SamplerKind::Invalid);
}

MDNode *CGMSHLSLRuntime::GetOrAddResTypeMD(QualType resTy, bool bCreate) {
  const RecordType *RT = resTy->getAs<RecordType>();
  if (!RT)
    return nullptr;
  RecordDecl *RD = RT->getDecl();
  SourceLocation loc = RD->getLocation();

  hlsl::DxilResourceBase::Class resClass = TypeToClass(resTy);
  llvm::Type *Ty = CGM.getTypes().ConvertType(resTy);
  auto it = resMetadataMap.find(Ty);
  if (!bCreate && it != resMetadataMap.end())
    return it->second;

  // Save resource type metadata.
  switch (resClass) {
  case DXIL::ResourceClass::UAV: {
    DxilResource UAV;
    // TODO: save globalcoherent to variable in EmitHLSLBuiltinCallExpr.
    SetUAVSRV(loc, resClass, &UAV, resTy);
    // Set global symbol to save type.
    UAV.SetGlobalSymbol(UndefValue::get(Ty->getPointerTo()));
    MDNode *MD = m_pHLModule->DxilUAVToMDNode(UAV);
    resMetadataMap[Ty] = MD;
    return MD;
  } break;
  case DXIL::ResourceClass::SRV: {
    DxilResource SRV;
    SetUAVSRV(loc, resClass, &SRV, resTy);
    // Set global symbol to save type.
    SRV.SetGlobalSymbol(UndefValue::get(Ty->getPointerTo()));
    MDNode *MD = m_pHLModule->DxilSRVToMDNode(SRV);
    resMetadataMap[Ty] = MD;
    return MD;
  } break;
  case DXIL::ResourceClass::Sampler: {
    DxilSampler S;
    DxilSampler::SamplerKind kind = KeywordToSamplerKind(RD->getName());
    S.SetSamplerKind(kind);
    // Set global symbol to save type.
    S.SetGlobalSymbol(UndefValue::get(Ty->getPointerTo()));
    MDNode *MD = m_pHLModule->DxilSamplerToMDNode(S);
    resMetadataMap[Ty] = MD;
    return MD;
  }
  default:
    // Skip OutputStream for GS.
    return nullptr;
  }
}


namespace {
MatrixOrientation GetMatrixMajor(QualType Ty, bool bDefaultRowMajor) {
  DXASSERT(hlsl::IsHLSLMatType(Ty), "");
  bool bIsRowMajor = bDefaultRowMajor;
  HasHLSLMatOrientation(Ty, &bIsRowMajor);
  return bIsRowMajor ? MatrixOrientation::RowMajor
                     : MatrixOrientation::ColumnMajor;
}

QualType GetArrayEltType(ASTContext &Context, QualType Ty) {
  while (const clang::ArrayType *ArrayTy = Context.getAsArrayType(Ty))
    Ty = ArrayTy->getElementType();
  return Ty;
}

} // namespace

DxilResourceProperties CGMSHLSLRuntime::BuildResourceProperty(QualType resTy) {
  resTy = GetArrayEltType(CGM.getContext(), resTy);
  const RecordType *RT = resTy->getAs<RecordType>();
  DxilResourceProperties RP;
  if (!RT) {
    RP.Class = DXIL::ResourceClass::Invalid;
    return RP;
  }
  RecordDecl *RD = RT->getDecl();
  SourceLocation loc = RD->getLocation();

  hlsl::DxilResourceBase::Class resClass = TypeToClass(resTy);
  RP.Class = resClass;
  if (resClass == DXIL::ResourceClass::Invalid)
    return RP;

  llvm::Type *Ty = CGM.getTypes().ConvertType(resTy);
  switch (resClass) {
  case DXIL::ResourceClass::UAV: {
    DxilResource UAV;
    // TODO: save globalcoherent to variable in EmitHLSLBuiltinCallExpr.
    SetUAVSRV(loc, resClass, &UAV, resTy);
    UAV.SetGlobalSymbol(UndefValue::get(Ty->getPointerTo()));
    RP = resource_helper::loadFromResourceBase(&UAV);
  } break;
  case DXIL::ResourceClass::SRV: {
    DxilResource SRV;
    SetUAVSRV(loc, resClass, &SRV, resTy);
    SRV.SetGlobalSymbol(UndefValue::get(Ty->getPointerTo()));
    RP = resource_helper::loadFromResourceBase(&SRV);
  } break;
  case DXIL::ResourceClass::Sampler: {
    DxilSampler::SamplerKind kind = KeywordToSamplerKind(RD->getName());
    DxilSampler Sampler;
    Sampler.SetSamplerKind(kind);
    RP = resource_helper::loadFromResourceBase(&Sampler);
  }
  default:
    break;
  }
  return RP;
}

void CGMSHLSLRuntime::ConstructFieldAttributedAnnotation(
    DxilFieldAnnotation &fieldAnnotation, QualType fieldTy,
    bool bDefaultRowMajor) {
  QualType Ty = fieldTy;
  if (Ty->isReferenceType())
    Ty = Ty.getNonReferenceType();

  // Get element type.
  Ty = GetArrayEltType(CGM.getContext(), Ty);

  QualType EltTy = Ty;
  if (hlsl::IsHLSLMatType(Ty)) {
    DxilMatrixAnnotation Matrix;

    Matrix.Orientation = GetMatrixMajor(Ty, bDefaultRowMajor);

    hlsl::GetHLSLMatRowColCount(Ty, Matrix.Rows, Matrix.Cols);
    fieldAnnotation.SetMatrixAnnotation(Matrix);
    EltTy = hlsl::GetHLSLMatElementType(Ty);
  }

  if (hlsl::IsHLSLVecType(Ty))
    EltTy = hlsl::GetHLSLVecElementType(Ty);

  if (IsHLSLResourceType(Ty)) {
    // Always create for llvm::Type could be same for different QualType.
    // TODO: change to DxilProperties.
    MDNode *MD = GetOrAddResTypeMD(Ty, /*bCreate*/ true);
    fieldAnnotation.SetResourceAttribute(MD);
  }

  bool bSNorm = false;
  bool bUNorm = false;
  if (HasHLSLUNormSNorm(Ty, &bSNorm) && !bSNorm)
    bUNorm = true;

  if (EltTy->isBuiltinType()) {
    const BuiltinType *BTy = EltTy->getAs<BuiltinType>();
    CompType::Kind kind = BuiltinTyToCompTy(BTy, bSNorm, bUNorm);
    fieldAnnotation.SetCompType(kind);
  } else if (EltTy->isEnumeralType()) {
    const EnumType *ETy = EltTy->getAs<EnumType>();
    QualType type = ETy->getDecl()->getIntegerType();
    if (const BuiltinType *BTy =
            dyn_cast<BuiltinType>(type->getCanonicalTypeInternal()))
      fieldAnnotation.SetCompType(BuiltinTyToCompTy(BTy, bSNorm, bUNorm));
  } else {
    DXASSERT(!bSNorm && !bUNorm,
             "snorm/unorm on invalid type, validate at handleHLSLTypeAttr");
  }
}

static void ConstructFieldInterpolation(DxilFieldAnnotation &fieldAnnotation,
                                      FieldDecl *fieldDecl) {
  // Keep undefined for interpMode here.
  InterpolationMode InterpMode = {fieldDecl->hasAttr<HLSLNoInterpolationAttr>(),
                                  fieldDecl->hasAttr<HLSLLinearAttr>(),
                                  fieldDecl->hasAttr<HLSLNoPerspectiveAttr>(),
                                  fieldDecl->hasAttr<HLSLCentroidAttr>(),
                                  fieldDecl->hasAttr<HLSLSampleAttr>()};
  if (InterpMode.GetKind() != InterpolationMode::Kind::Undefined)
    fieldAnnotation.SetInterpolationMode(InterpMode);
}

static unsigned AlignBaseOffset(unsigned baseOffset, unsigned size, QualType Ty,
                                bool bDefaultRowMajor) {
  bool needNewAlign = Ty->isArrayType();

  if (IsHLSLMatType(Ty)) {
    bool bRowMajor = false;
    if (!hlsl::HasHLSLMatOrientation(Ty, &bRowMajor))
      bRowMajor = bDefaultRowMajor;

    unsigned row, col;
    hlsl::GetHLSLMatRowColCount(Ty, row, col);

    needNewAlign |= !bRowMajor && col > 1;
    needNewAlign |= bRowMajor && row > 1;
  } else if (Ty->isStructureOrClassType() && !hlsl::IsHLSLVecType(Ty)) {
    needNewAlign = true;
  }

  unsigned scalarSizeInBytes = 4;
  const clang::BuiltinType *BT = Ty->getAs<clang::BuiltinType>();
  if (hlsl::IsHLSLVecMatType(Ty)) {
    BT = hlsl::GetElementTypeOrType(Ty)->getAs<clang::BuiltinType>();
  }
  if (BT) {
    if (BT->getKind() == clang::BuiltinType::Kind::Double ||
        BT->getKind() == clang::BuiltinType::Kind::LongLong)
      scalarSizeInBytes = 8;
    else if (BT->getKind() == clang::BuiltinType::Kind::Half ||
             BT->getKind() == clang::BuiltinType::Kind::Short ||
             BT->getKind() == clang::BuiltinType::Kind::UShort)
      scalarSizeInBytes = 2;
  }

  return AlignBufferOffsetInLegacy(baseOffset, size, scalarSizeInBytes,
                                   needNewAlign);
}

static unsigned AlignBaseOffset(QualType Ty, unsigned baseOffset,
                                bool bDefaultRowMajor,
                                CodeGen::CodeGenModule &CGM,
                                llvm::DataLayout &layout) {
  QualType paramTy = Ty.getCanonicalType();
  if (const ReferenceType *RefType = dyn_cast<ReferenceType>(paramTy))
    paramTy = RefType->getPointeeType();

  // Get size.
  llvm::Type *Type = CGM.getTypes().ConvertType(paramTy);
  unsigned size = layout.getTypeAllocSize(Type);
  return AlignBaseOffset(baseOffset, size, paramTy, bDefaultRowMajor);
}

unsigned CGMSHLSLRuntime::ConstructStructAnnotation(DxilStructAnnotation *annotation,
                                      const RecordDecl *RD,
                                      DxilTypeSystem &dxilTypeSys) {
  unsigned fieldIdx = 0;
  unsigned offset = 0;
  bool bDefaultRowMajor = m_pHLModule->GetHLOptions().bDefaultRowMajor;
  if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {

    // If template, save template args
    if (const ClassTemplateSpecializationDecl *templateSpecializationDecl =
          dyn_cast<ClassTemplateSpecializationDecl>(CXXRD)) {
      const clang::TemplateArgumentList &args = templateSpecializationDecl->getTemplateInstantiationArgs();
      for (unsigned i = 0; i < args.size(); ++i) {
        DxilTemplateArgAnnotation &argAnnotation = annotation->GetTemplateArgAnnotation(i);
        const clang::TemplateArgument &arg = args[i];
        switch (arg.getKind()) {
        case clang::TemplateArgument::ArgKind::Type:
          argAnnotation.SetType(CGM.getTypes().ConvertType(arg.getAsType()));
        break;
        case clang::TemplateArgument::ArgKind::Integral:
          argAnnotation.SetIntegral(arg.getAsIntegral().getExtValue());
          break;
        default:
          break;
        }
      }
    }

    if (CXXRD->getNumBases()) {
      // Add base as field.
      for (const auto &I : CXXRD->bases()) {
        const CXXRecordDecl *BaseDecl =
            cast<CXXRecordDecl>(I.getType()->castAs<RecordType>()->getDecl());
        std::string fieldSemName = "";

        QualType parentTy = QualType(BaseDecl->getTypeForDecl(), 0);

        // Align offset.
        offset = AlignBaseOffset(parentTy, offset, bDefaultRowMajor, CGM,
                                 dataLayout);

        unsigned CBufferOffset = offset;

        unsigned arrayEltSize = 0;
        // Process field to make sure the size of field is ready.
        unsigned size =
            AddTypeAnnotation(parentTy, dxilTypeSys, arrayEltSize);

        // Update offset.
        offset += size;

        if (size > 0) {
          DxilFieldAnnotation &fieldAnnotation =
              annotation->GetFieldAnnotation(fieldIdx++);

          fieldAnnotation.SetCBufferOffset(CBufferOffset);
          fieldAnnotation.SetFieldName(BaseDecl->getNameAsString());
        }
      }
    }
  }

  for (auto fieldDecl : RD->fields()) {
    std::string fieldSemName = "";

    QualType fieldTy = fieldDecl->getType();
    DXASSERT(!fieldDecl->isBitField(), "We should have already ensured we have no bitfields.");
    
    // Align offset.
    offset = AlignBaseOffset(fieldTy, offset, bDefaultRowMajor, CGM, dataLayout);

    unsigned CBufferOffset = offset;

    // Try to get info from fieldDecl.
    for (const hlsl::UnusualAnnotation *it :
         fieldDecl->getUnusualAnnotations()) {
      switch (it->getKind()) {
      case hlsl::UnusualAnnotation::UA_SemanticDecl: {
        const hlsl::SemanticDecl *sd = cast<hlsl::SemanticDecl>(it);
        fieldSemName = sd->SemanticName;
      } break;
      case hlsl::UnusualAnnotation::UA_ConstantPacking: {
        const hlsl::ConstantPacking *cp = cast<hlsl::ConstantPacking>(it);
        CBufferOffset = cp->Subcomponent << 2;
        CBufferOffset += cp->ComponentOffset;
        // Change to byte.
        CBufferOffset <<= 2;
      } break;
      case hlsl::UnusualAnnotation::UA_RegisterAssignment: {
        // register assignment only works on global constant.
        DiagnosticsEngine &Diags = CGM.getDiags();
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "location semantics cannot be specified on members.");
        Diags.Report(it->Loc, DiagID);
        return 0;
      } break;
      default:
        llvm_unreachable("only semantic for input/output");
        break;
      }
    }

    unsigned arrayEltSize = 0;
    // Process field to make sure the size of field is ready.
    unsigned size = AddTypeAnnotation(fieldDecl->getType(), dxilTypeSys, arrayEltSize);

    // Update offset.
    offset += size;
    
    DxilFieldAnnotation &fieldAnnotation = annotation->GetFieldAnnotation(fieldIdx++);

    ConstructFieldAttributedAnnotation(fieldAnnotation, fieldTy, bDefaultRowMajor);
    ConstructFieldInterpolation(fieldAnnotation, fieldDecl);
    if (fieldDecl->hasAttr<HLSLPreciseAttr>())
      fieldAnnotation.SetPrecise();

    fieldAnnotation.SetCBufferOffset(CBufferOffset);
    fieldAnnotation.SetFieldName(fieldDecl->getName());
    if (!fieldSemName.empty()) {
      fieldAnnotation.SetSemanticString(fieldSemName);

      if (m_PreciseOutputSet.count(StringRef(fieldSemName).lower()))
        fieldAnnotation.SetPrecise();
    }
  }

  annotation->SetCBufferSize(offset);
  if (offset == 0) {
    annotation->MarkEmptyStruct();
  }
  return offset;
}

static bool IsElementInputOutputType(QualType Ty) {
  return Ty->isBuiltinType() || hlsl::IsHLSLVecMatType(Ty) || Ty->isEnumeralType();
}

static unsigned GetNumTemplateArgsForRecordDecl(const RecordDecl *RD) {
  if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
    if (const ClassTemplateSpecializationDecl *templateSpecializationDecl =
          dyn_cast<ClassTemplateSpecializationDecl>(CXXRD)) {
      const clang::TemplateArgumentList &args = templateSpecializationDecl->getTemplateInstantiationArgs();
      return args.size();
    }
  }
  return 0;
}

// Return the size for constant buffer of each decl.
unsigned CGMSHLSLRuntime::AddTypeAnnotation(QualType Ty,
                                            DxilTypeSystem &dxilTypeSys,
                                            unsigned &arrayEltSize) {
  QualType paramTy = Ty.getCanonicalType();
  if (const ReferenceType *RefType = dyn_cast<ReferenceType>(paramTy))
    paramTy = RefType->getPointeeType();

  // Get size.
  llvm::Type *Type = CGM.getTypes().ConvertType(paramTy);
  unsigned size = dataLayout.getTypeAllocSize(Type);

  if (IsHLSLMatType(Ty)) {
    llvm::Type *EltTy = HLMatrixType::cast(Type).getElementTypeForReg();
    bool b64Bit = dataLayout.getTypeAllocSize(EltTy) == 8;
    size = GetMatrixSizeInCB(Ty, m_pHLModule->GetHLOptions().bDefaultRowMajor,
                             b64Bit);
  }
  // Skip element types.
  if (IsElementInputOutputType(paramTy))
    return size;
  else if (IsHLSLStreamOutputType(Ty)) {
    return AddTypeAnnotation(GetHLSLOutputPatchElementType(Ty), dxilTypeSys,
                             arrayEltSize);
  } else if (IsHLSLInputPatchType(Ty))
    return AddTypeAnnotation(GetHLSLInputPatchElementType(Ty), dxilTypeSys,
                             arrayEltSize);
  else if (IsHLSLOutputPatchType(Ty))
    return AddTypeAnnotation(GetHLSLOutputPatchElementType(Ty), dxilTypeSys,
                             arrayEltSize);
  else if (const RecordType *RT = paramTy->getAsStructureType()) {
    RecordDecl *RD = RT->getDecl();
    llvm::StructType *ST = CGM.getTypes().ConvertRecordDeclType(RD);
    // Skip if already created.
    if (DxilStructAnnotation *annotation = dxilTypeSys.GetStructAnnotation(ST)) {
      unsigned structSize = annotation->GetCBufferSize();
      return structSize;
    }
    DxilStructAnnotation *annotation = dxilTypeSys.AddStructAnnotation(ST,
      GetNumTemplateArgsForRecordDecl(RT->getDecl()));

    return ConstructStructAnnotation(annotation, RD, dxilTypeSys);
  } else if (const RecordType *RT = dyn_cast<RecordType>(paramTy)) {
    // For this pointer.
    RecordDecl *RD = RT->getDecl();
    llvm::StructType *ST = CGM.getTypes().ConvertRecordDeclType(RD);
    // Skip if already created.
    if (DxilStructAnnotation *annotation = dxilTypeSys.GetStructAnnotation(ST)) {
      unsigned structSize = annotation->GetCBufferSize();
      return structSize;
    }
    DxilStructAnnotation *annotation = dxilTypeSys.AddStructAnnotation(ST,
      GetNumTemplateArgsForRecordDecl(RT->getDecl()));

    return ConstructStructAnnotation(annotation, RD, dxilTypeSys);
  } else if (IsHLSLResourceType(Ty)) {
    // Save result type info.
    AddTypeAnnotation(GetHLSLResourceResultType(Ty), dxilTypeSys, arrayEltSize);
    // Resource don't count for cbuffer size.
    return 0;
  } else if (IsStringType(Ty)) {
    // string won't be included in cbuffer
    return 0;
  } else {
    unsigned arraySize = 0;
    QualType arrayElementTy = Ty;
    if (Ty->isConstantArrayType()) {
      const ConstantArrayType *arrayTy =
        CGM.getContext().getAsConstantArrayType(Ty);
      DXASSERT(arrayTy != nullptr, "Must array type here");

      arraySize = arrayTy->getSize().getLimitedValue();
      arrayElementTy = arrayTy->getElementType();
    }
    else if (Ty->isIncompleteArrayType()) {
      const IncompleteArrayType *arrayTy = CGM.getContext().getAsIncompleteArrayType(Ty);
      arrayElementTy = arrayTy->getElementType();
    } else {
      DXASSERT(0, "Must array type here");
    }

    unsigned elementSize = AddTypeAnnotation(arrayElementTy, dxilTypeSys, arrayEltSize);
    // Only set arrayEltSize once.
    if (arrayEltSize == 0)
      arrayEltSize = elementSize;
    // Align to 4 * 4bytes.
    unsigned alignedSize = (elementSize + 15) & 0xfffffff0;
    return alignedSize * (arraySize - 1) + elementSize;
  }
}


static DxilResource::Kind KeywordToKind(StringRef keyword) {
  // TODO: refactor for faster search (switch by 1/2/3 first letters, then
  // compare)
  if (keyword == "Texture1D" || keyword == "RWTexture1D" || keyword == "RasterizerOrderedTexture1D")
    return DxilResource::Kind::Texture1D;
  if (keyword == "Texture2D" || keyword == "RWTexture2D" || keyword == "RasterizerOrderedTexture2D")
    return DxilResource::Kind::Texture2D;
  if (keyword == "Texture2DMS" || keyword == "RWTexture2DMS")
    return DxilResource::Kind::Texture2DMS;
  if (keyword == "FeedbackTexture2D")
    return DxilResource::Kind::FeedbackTexture2D;
  if (keyword == "Texture3D" || keyword == "RWTexture3D" || keyword == "RasterizerOrderedTexture3D")
    return DxilResource::Kind::Texture3D;
  if (keyword == "TextureCube" || keyword == "RWTextureCube")
    return DxilResource::Kind::TextureCube;

  if (keyword == "Texture1DArray" || keyword == "RWTexture1DArray" || keyword == "RasterizerOrderedTexture1DArray")
    return DxilResource::Kind::Texture1DArray;
  if (keyword == "Texture2DArray" || keyword == "RWTexture2DArray" || keyword == "RasterizerOrderedTexture2DArray")
    return DxilResource::Kind::Texture2DArray;
  if (keyword == "FeedbackTexture2DArray")
    return DxilResource::Kind::FeedbackTexture2DArray;
  if (keyword == "Texture2DMSArray" || keyword == "RWTexture2DMSArray")
    return DxilResource::Kind::Texture2DMSArray;
  if (keyword == "TextureCubeArray" || keyword == "RWTextureCubeArray")
    return DxilResource::Kind::TextureCubeArray;

  if (keyword == "ByteAddressBuffer" || keyword == "RWByteAddressBuffer" || keyword == "RasterizerOrderedByteAddressBuffer")
    return DxilResource::Kind::RawBuffer;

  if (keyword == "StructuredBuffer" || keyword == "RWStructuredBuffer" || keyword == "RasterizerOrderedStructuredBuffer")
    return DxilResource::Kind::StructuredBuffer;

  if (keyword == "AppendStructuredBuffer" || keyword == "ConsumeStructuredBuffer")
    return DxilResource::Kind::StructuredBuffer;

  // TODO: this is not efficient.
  bool isBuffer = keyword == "Buffer";
  isBuffer |= keyword == "RWBuffer";
  isBuffer |= keyword == "RasterizerOrderedBuffer";
  if (isBuffer)
    return DxilResource::Kind::TypedBuffer;
  if (keyword == "RaytracingAccelerationStructure")
    return DxilResource::Kind::RTAccelerationStructure;
  return DxilResource::Kind::Invalid;
}

void CGMSHLSLRuntime::AddHLSLFunctionInfo(Function *F, const FunctionDecl *FD) {
  // Add hlsl intrinsic attr
  unsigned intrinsicOpcode;
  StringRef intrinsicGroup;


  if (hlsl::GetIntrinsicOp(FD, intrinsicOpcode, intrinsicGroup)) {
    AddHLSLIntrinsicOpcodeToFunction(F, intrinsicOpcode);
    F->addFnAttr(hlsl::HLPrefix, intrinsicGroup);

    StringRef lower;
    if (hlsl::GetIntrinsicLowering(FD, lower))
      hlsl::SetHLLowerStrategy(F, lower);


    if (FD->hasAttr<HLSLWaveSensitiveAttr>())
      hlsl::SetHLWaveSensitive(F);

    // Don't need to add FunctionQual for intrinsic function.
    return;
  }

  if (m_pHLModule->GetFloat32DenormMode() == DXIL::Float32DenormMode::FTZ) {
    F->addFnAttr(DXIL::kFP32DenormKindString, DXIL::kFP32DenormValueFtzString);
  }
  else if (m_pHLModule->GetFloat32DenormMode() == DXIL::Float32DenormMode::Preserve) {
    F->addFnAttr(DXIL::kFP32DenormKindString, DXIL::kFP32DenormValuePreserveString);
  }
  else if (m_pHLModule->GetFloat32DenormMode() == DXIL::Float32DenormMode::Any) {
    F->addFnAttr(DXIL::kFP32DenormKindString, DXIL::kFP32DenormValueAnyString);
  }
  // Set entry function
  const std::string &entryName = m_pHLModule->GetEntryFunctionName();
  bool isEntry = FD->getNameAsString() == entryName;
  if (isEntry) {
    Entry.Func = F;
    Entry.SL = FD->getLocation();
  }

  DiagnosticsEngine &Diags = CGM.getDiags();

  std::unique_ptr<DxilFunctionProps> funcProps =
      llvm::make_unique<DxilFunctionProps>();
  funcProps->shaderKind = DXIL::ShaderKind::Invalid;
  bool isCS = false;
  bool isGS = false;
  bool isHS = false;
  bool isDS = false;
  bool isVS = false;
  bool isPS = false;
  bool isRay = false;
  bool isMS = false;
  bool isAS = false;
  if (const HLSLShaderAttr *Attr = FD->getAttr<HLSLShaderAttr>()) {
    // Stage is already validate in HandleDeclAttributeForHLSL.
    // Here just check first letter (or two).
    switch (Attr->getStage()[0]) {
    case 'c':
      switch (Attr->getStage()[1]) {
      case 'o':
        isCS = true;
        funcProps->shaderKind = DXIL::ShaderKind::Compute;
        break;
      case 'l':
        isRay = true;
        funcProps->shaderKind = DXIL::ShaderKind::ClosestHit;
        break;
      case 'a':
        isRay = true;
        funcProps->shaderKind = DXIL::ShaderKind::Callable;
        break;
      default:
        break;
      }
      break;
    case 'v':
      isVS = true;
      funcProps->shaderKind = DXIL::ShaderKind::Vertex;
      break;
    case 'h':
      isHS = true;
      funcProps->shaderKind = DXIL::ShaderKind::Hull;
      break;
    case 'd':
      isDS = true;
      funcProps->shaderKind = DXIL::ShaderKind::Domain;
      break;
    case 'g':
      isGS = true;
      funcProps->shaderKind = DXIL::ShaderKind::Geometry;
      break;
    case 'p':
      isPS = true;
      funcProps->shaderKind = DXIL::ShaderKind::Pixel;
      break;
    case 'r':
      isRay = true;
      funcProps->shaderKind = DXIL::ShaderKind::RayGeneration;
      break;
    case 'i':
      isRay = true;
      funcProps->shaderKind = DXIL::ShaderKind::Intersection;
      break;
    case 'a':
      switch (Attr->getStage()[1]) {
      case 'm':
        isAS = true;
        funcProps->shaderKind = DXIL::ShaderKind::Amplification;
        break;
      case 'n':
        isRay = true;
        funcProps->shaderKind = DXIL::ShaderKind::AnyHit;
        break;
      default:
        break;
      }
      break;
    case 'm':
      switch (Attr->getStage()[1]) {
      case 'e':
        isMS = true;
        funcProps->shaderKind = DXIL::ShaderKind::Mesh;
        break;
      case 'i':
        isRay = true;
        funcProps->shaderKind = DXIL::ShaderKind::Miss;
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
    if (funcProps->shaderKind == DXIL::ShaderKind::Invalid) {
      unsigned DiagID = Diags.getCustomDiagID(
        DiagnosticsEngine::Error, "Invalid profile for shader attribute");
      Diags.Report(Attr->getLocation(), DiagID);
    }
    if (isEntry && isRay) {
      unsigned DiagID = Diags.getCustomDiagID(
        DiagnosticsEngine::Error, "Ray function cannot be used as a global entry point");
      Diags.Report(Attr->getLocation(), DiagID);
    }
  }

  // Save patch constant function to patchConstantFunctionMap.
  bool isPatchConstantFunction = false;
  if (!isEntry && CGM.getContext().IsPatchConstantFunctionDecl(FD)) {
    isPatchConstantFunction = true;
    auto &PCI = patchConstantFunctionMap[FD->getName()];
    PCI.SL = FD->getLocation();
    PCI.Func = F;
    ++PCI.NumOverloads;

    for (ParmVarDecl *parmDecl : FD->parameters()) {
      QualType Ty = parmDecl->getType();
      if (IsHLSLOutputPatchType(Ty)) {
        funcProps->ShaderProps.HS.outputControlPoints =
            GetHLSLOutputPatchCount(parmDecl->getType());
      } else if (IsHLSLInputPatchType(Ty)) {
        funcProps->ShaderProps.HS.inputControlPoints =
            GetHLSLInputPatchCount(parmDecl->getType());
      }
    }

    // Mark patch constant functions that cannot be linked as exports
    // InternalLinkage.  Patch constant functions that are actually used
    // will be set back to ExternalLinkage in FinishCodeGen.
    if (funcProps->ShaderProps.HS.outputControlPoints ||
        funcProps->ShaderProps.HS.inputControlPoints) {
      PCI.Func->setLinkage(GlobalValue::InternalLinkage);
    }

    funcProps->shaderKind = DXIL::ShaderKind::Hull;
  }

  const ShaderModel *SM = m_pHLModule->GetShaderModel();
  if (isEntry) {
    funcProps->shaderKind = SM->GetKind();
    if (funcProps->shaderKind == DXIL::ShaderKind::Mesh) {
      isMS = true;
    }
    else if (funcProps->shaderKind == DXIL::ShaderKind::Amplification) {
      isAS = true;
    }
  }

  // Geometry shader.
  if (const HLSLMaxVertexCountAttr *Attr =
          FD->getAttr<HLSLMaxVertexCountAttr>()) {
    isGS = true;
    funcProps->shaderKind = DXIL::ShaderKind::Geometry;
    funcProps->ShaderProps.GS.maxVertexCount = Attr->getCount();
    funcProps->ShaderProps.GS.inputPrimitive = DXIL::InputPrimitive::Undefined;

    if (isEntry && !SM->IsGS()) {
      unsigned DiagID =
          Diags.getCustomDiagID(DiagnosticsEngine::Error,
                                "attribute maxvertexcount only valid for GS.");
      Diags.Report(Attr->getLocation(), DiagID);
      return;
    }
  }
  if (const HLSLInstanceAttr *Attr = FD->getAttr<HLSLInstanceAttr>()) {
    unsigned instanceCount = Attr->getCount();
    funcProps->ShaderProps.GS.instanceCount = instanceCount;
    if (isEntry && !SM->IsGS()) {
      unsigned DiagID =
          Diags.getCustomDiagID(DiagnosticsEngine::Error,
                                "attribute maxvertexcount only valid for GS.");
      Diags.Report(Attr->getLocation(), DiagID);
      return;
    }
  } else {
    // Set default instance count.
    if (isGS)
      funcProps->ShaderProps.GS.instanceCount = 1;
  }

  // Compute shader
  if (const HLSLNumThreadsAttr *Attr = FD->getAttr<HLSLNumThreadsAttr>()) {
    if (isMS) {
      funcProps->ShaderProps.MS.numThreads[0] = Attr->getX();
      funcProps->ShaderProps.MS.numThreads[1] = Attr->getY();
      funcProps->ShaderProps.MS.numThreads[2] = Attr->getZ();
    } else if (isAS) {
      funcProps->ShaderProps.AS.numThreads[0] = Attr->getX();
      funcProps->ShaderProps.AS.numThreads[1] = Attr->getY();
      funcProps->ShaderProps.AS.numThreads[2] = Attr->getZ();
    } else {
      isCS = true;
      funcProps->shaderKind = DXIL::ShaderKind::Compute;

      funcProps->ShaderProps.CS.numThreads[0] = Attr->getX();
      funcProps->ShaderProps.CS.numThreads[1] = Attr->getY();
      funcProps->ShaderProps.CS.numThreads[2] = Attr->getZ();
    }

    if (isEntry && !SM->IsCS() && !SM->IsMS() && !SM->IsAS()) {
      unsigned DiagID = Diags.getCustomDiagID(
          DiagnosticsEngine::Error, "attribute numthreads only valid for CS/MS/AS.");
      Diags.Report(Attr->getLocation(), DiagID);
      return;
    }
  }

  // Hull shader.
  if (const HLSLPatchConstantFuncAttr *Attr =
          FD->getAttr<HLSLPatchConstantFuncAttr>()) {
    if (isEntry && !SM->IsHS()) {
      unsigned DiagID = Diags.getCustomDiagID(
          DiagnosticsEngine::Error,
          "attribute patchconstantfunc only valid for HS.");
      Diags.Report(Attr->getLocation(), DiagID);
      return;
    }

    isHS = true;
    funcProps->shaderKind = DXIL::ShaderKind::Hull;
    HSEntryPatchConstantFuncAttr[F] = Attr;
  } else {
    // TODO: This is a duplicate check. We also have this check in
    // hlsl::DiagnoseTranslationUnit(clang::Sema*).
    if (isEntry && SM->IsHS()) {
      unsigned DiagID = Diags.getCustomDiagID(
          DiagnosticsEngine::Error,
          "HS entry point must have the patchconstantfunc attribute");
      Diags.Report(FD->getLocation(), DiagID);
      return;
    }
  }

  if (const HLSLOutputControlPointsAttr *Attr =
          FD->getAttr<HLSLOutputControlPointsAttr>()) {
    if (isHS) {
      funcProps->ShaderProps.HS.outputControlPoints = Attr->getCount();
    } else if (isEntry && !SM->IsHS()) {
      unsigned DiagID = Diags.getCustomDiagID(
          DiagnosticsEngine::Error,
          "attribute outputcontrolpoints only valid for HS.");
      Diags.Report(Attr->getLocation(), DiagID);
      return;
    }
  }

  if (const HLSLPartitioningAttr *Attr = FD->getAttr<HLSLPartitioningAttr>()) {
    if (isHS) {
      DXIL::TessellatorPartitioning partition =
          StringToPartitioning(Attr->getScheme());
      funcProps->ShaderProps.HS.partition = partition;
    } else if (isEntry && !SM->IsHS()) {
      unsigned DiagID =
          Diags.getCustomDiagID(DiagnosticsEngine::Warning,
                                "attribute partitioning only valid for HS.");
      Diags.Report(Attr->getLocation(), DiagID);
    }
  }

  if (const HLSLOutputTopologyAttr *Attr =
          FD->getAttr<HLSLOutputTopologyAttr>()) {
    if (isHS) {
      DXIL::TessellatorOutputPrimitive primitive =
          StringToTessOutputPrimitive(Attr->getTopology());
      funcProps->ShaderProps.HS.outputPrimitive = primitive;
    }
    else if (isMS) {
      DXIL::MeshOutputTopology topology =
          StringToMeshOutputTopology(Attr->getTopology());
      funcProps->ShaderProps.MS.outputTopology = topology;
    }
    else if (isEntry && !SM->IsHS() && !SM->IsMS()) {
      unsigned DiagID =
          Diags.getCustomDiagID(DiagnosticsEngine::Warning,
                                "attribute outputtopology only valid for HS and MS.");
      Diags.Report(Attr->getLocation(), DiagID);
    }
  }

  if (isHS) {
    funcProps->ShaderProps.HS.maxTessFactor = DXIL::kHSMaxTessFactorUpperBound;
    funcProps->ShaderProps.HS.inputControlPoints = DXIL::kHSDefaultInputControlPointCount;
  }

  if (const HLSLMaxTessFactorAttr *Attr =
          FD->getAttr<HLSLMaxTessFactorAttr>()) {
    if (isHS) {
      // TODO: change getFactor to return float.
      llvm::APInt intV(32, Attr->getFactor());
      funcProps->ShaderProps.HS.maxTessFactor = intV.bitsToFloat();
    } else if (isEntry && !SM->IsHS()) {
      unsigned DiagID =
          Diags.getCustomDiagID(DiagnosticsEngine::Error,
                                "attribute maxtessfactor only valid for HS.");
      Diags.Report(Attr->getLocation(), DiagID);
      return;
    }
  }

  // Hull or domain shader.
  if (const HLSLDomainAttr *Attr = FD->getAttr<HLSLDomainAttr>()) {
    if (isEntry && !SM->IsHS() && !SM->IsDS()) {
      unsigned DiagID =
          Diags.getCustomDiagID(DiagnosticsEngine::Error,
                                "attribute domain only valid for HS or DS.");
      Diags.Report(Attr->getLocation(), DiagID);
      return;
    }

    isDS = !isHS;
    if (isDS)
      funcProps->shaderKind = DXIL::ShaderKind::Domain;

    DXIL::TessellatorDomain domain = StringToDomain(Attr->getDomainType());
    if (isHS)
      funcProps->ShaderProps.HS.domain = domain;
    else
      funcProps->ShaderProps.DS.domain = domain;
  }

  // Vertex shader.
  if (const HLSLClipPlanesAttr *Attr = FD->getAttr<HLSLClipPlanesAttr>()) {
    if (isEntry && !SM->IsVS()) {
      unsigned DiagID = Diags.getCustomDiagID(
          DiagnosticsEngine::Error, "attribute clipplane only valid for VS.");
      Diags.Report(Attr->getLocation(), DiagID);
      return;
    }

    isVS = true;
    // The real job is done at EmitHLSLFunctionProlog where debug info is
    // available. Only set shader kind here.
    funcProps->shaderKind = DXIL::ShaderKind::Vertex;
  }

  // Pixel shader.
  if (const HLSLEarlyDepthStencilAttr *Attr =
          FD->getAttr<HLSLEarlyDepthStencilAttr>()) {
    if (isEntry && !SM->IsPS()) {
      unsigned DiagID = Diags.getCustomDiagID(
          DiagnosticsEngine::Error,
          "attribute earlydepthstencil only valid for PS.");
      Diags.Report(Attr->getLocation(), DiagID);
      return;
    }

    isPS = true;
    funcProps->ShaderProps.PS.EarlyDepthStencil = true;
    funcProps->shaderKind = DXIL::ShaderKind::Pixel;
  }

  const unsigned profileAttributes = isCS + isHS + isDS + isGS + isVS + isPS + isRay + isMS + isAS;

  // TODO: check this in front-end and report error.
  DXASSERT(profileAttributes < 2, "profile attributes are mutual exclusive");

  if (isEntry) {
    switch (funcProps->shaderKind) {
    case ShaderModel::Kind::Compute:
    case ShaderModel::Kind::Hull:
    case ShaderModel::Kind::Domain:
    case ShaderModel::Kind::Geometry:
    case ShaderModel::Kind::Vertex:
    case ShaderModel::Kind::Pixel:
    case ShaderModel::Kind::Mesh:
    case ShaderModel::Kind::Amplification:
      DXASSERT(funcProps->shaderKind == SM->GetKind(),
               "attribute profile not match entry function profile");
      break;
    case ShaderModel::Kind::Library:
    case ShaderModel::Kind::Invalid:
      // Non-shader stage shadermodels don't have entry points.
      break;
    }
  }

  DxilFunctionAnnotation *FuncAnnotation =
      m_pHLModule->AddFunctionAnnotation(F);
  bool bDefaultRowMajor = m_pHLModule->GetHLOptions().bDefaultRowMajor;

  // Param Info
  unsigned streamIndex = 0;
  unsigned inputPatchCount = 0;
  unsigned outputPatchCount = 0;

  unsigned ArgNo = 0;
  unsigned ParmIdx = 0;

  auto ArgIt = F->arg_begin();

  if (const CXXMethodDecl *MethodDecl = dyn_cast<CXXMethodDecl>(FD)) {
    if (MethodDecl->isInstance()) {
      QualType ThisTy = MethodDecl->getThisType(FD->getASTContext());
      DxilParameterAnnotation &paramAnnotation =
          FuncAnnotation->GetParameterAnnotation(ArgNo++);
      ++ArgIt;
      // Construct annoation for this pointer.
      ConstructFieldAttributedAnnotation(paramAnnotation, ThisTy,
                                         bDefaultRowMajor);
      if (MethodDecl->isConst()) {
        paramAnnotation.SetParamInputQual(DxilParamInputQual::In);
      } else {
        paramAnnotation.SetParamInputQual(DxilParamInputQual::Inout);
      }
    }
  }

  // Ret Info
  QualType retTy = FD->getReturnType();
  DxilParameterAnnotation *pRetTyAnnotation = nullptr;
  if (F->getReturnType()->isVoidTy() && !retTy->isVoidType()) {
    // SRet.
    pRetTyAnnotation = &FuncAnnotation->GetParameterAnnotation(ArgNo++);
    // Save resource properties for parameters.
    DxilResourceProperties RP = BuildResourceProperty(retTy);
    if (RP.Class != DXIL::ResourceClass::Invalid)
      valToResPropertiesMap[ArgIt] = RP;
    ++ArgIt;
  } else {
    pRetTyAnnotation = &FuncAnnotation->GetRetTypeAnnotation();
  }
  DxilParameterAnnotation &retTyAnnotation = *pRetTyAnnotation;
  // keep Undefined here, we cannot decide for struct
  retTyAnnotation.SetInterpolationMode(
      GetInterpMode(FD, CompType::Kind::Invalid, /*bKeepUndefined*/ true)
          .GetKind());
  SourceLocation retTySemanticLoc = SetSemantic(FD, retTyAnnotation);
  retTyAnnotation.SetParamInputQual(DxilParamInputQual::Out);
  if (isEntry) {
    if (CGM.getLangOpts().EnableDX9CompatMode && retTyAnnotation.HasSemanticString()) {
      RemapObsoleteSemantic(retTyAnnotation, /*isPatchConstantFunction*/ false);
    }
    CheckParameterAnnotation(retTySemanticLoc, retTyAnnotation,
                             /*isPatchConstantFunction*/ false);
  }
  if (isRay && !retTy->isVoidType()) {
    Diags.Report(FD->getLocation(), Diags.getCustomDiagID(
      DiagnosticsEngine::Error, "return type for ray tracing shaders must be void"));
  }

  ConstructFieldAttributedAnnotation(retTyAnnotation, retTy, bDefaultRowMajor);
  if (FD->hasAttr<HLSLPreciseAttr>())
    retTyAnnotation.SetPrecise();

  if (isRay) {
    funcProps->ShaderProps.Ray.payloadSizeInBytes = 0;
    funcProps->ShaderProps.Ray.attributeSizeInBytes = 0;
  }

  bool hasOutIndices = false;
  bool hasOutVertices = false;
  bool hasOutPrimitives = false;
  bool hasInPayload = false;
  for (; ArgNo < F->arg_size(); ++ArgNo, ++ParmIdx, ++ArgIt) {
    DxilParameterAnnotation &paramAnnotation =
        FuncAnnotation->GetParameterAnnotation(ArgNo);

    const ParmVarDecl *parmDecl = FD->getParamDecl(ParmIdx);

    QualType fieldTy = parmDecl->getType();
    // Save resource properties for parameters.
    DxilResourceProperties RP = BuildResourceProperty(fieldTy);
    if (RP.Class != DXIL::ResourceClass::Invalid)
      valToResPropertiesMap[ArgIt] = RP;

    // if parameter type is a typedef, try to desugar it first.
    if (isa<TypedefType>(fieldTy.getTypePtr()))
      fieldTy = fieldTy.getDesugaredType(FD->getASTContext());
    ConstructFieldAttributedAnnotation(paramAnnotation, fieldTy,
                                       bDefaultRowMajor);
    if (parmDecl->hasAttr<HLSLPreciseAttr>())
      paramAnnotation.SetPrecise();

    // keep Undefined here, we cannot decide for struct
    InterpolationMode paramIM =
        GetInterpMode(parmDecl, CompType::Kind::Invalid, KeepUndefinedTrue);
    paramAnnotation.SetInterpolationMode(paramIM);
    SourceLocation paramSemanticLoc = SetSemantic(parmDecl, paramAnnotation);

    DxilParamInputQual dxilInputQ = DxilParamInputQual::In;

    if (parmDecl->hasAttr<HLSLInOutAttr>())
      dxilInputQ = DxilParamInputQual::Inout;
    else if (parmDecl->hasAttr<HLSLOutAttr>())
      dxilInputQ = DxilParamInputQual::Out;
    if (parmDecl->hasAttr<HLSLOutAttr>() && parmDecl->hasAttr<HLSLInAttr>())
      dxilInputQ = DxilParamInputQual::Inout;

    if (parmDecl->hasAttr<HLSLOutAttr>() && parmDecl->hasAttr<HLSLIndicesAttr>()) {
      if (hasOutIndices) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "multiple out indices parameters not allowed");
        Diags.Report(parmDecl->getLocation(), DiagID);
        continue;
      }
      const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(fieldTy.getCanonicalType());
      if (CAT == nullptr) {
        unsigned DiagID = Diags.getCustomDiagID(
          DiagnosticsEngine::Error,
          "indices output is not an constant-length array");
        Diags.Report(parmDecl->getLocation(), DiagID);
        continue;
      }
      unsigned count = CAT->getSize().getZExtValue();
      if (count > DXIL::kMaxMSOutputPrimitiveCount) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "max primitive count should not exceed %0");
        Diags.Report(parmDecl->getLocation(), DiagID) << DXIL::kMaxMSOutputPrimitiveCount;
        continue;
      }
      if (funcProps->ShaderProps.MS.maxPrimitiveCount != 0 &&
        funcProps->ShaderProps.MS.maxPrimitiveCount != count) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "max primitive count mismatch");
        Diags.Report(parmDecl->getLocation(), DiagID);
        continue;
      }
      // Get element type.
      QualType arrayEleTy = CAT->getElementType();

      if (hlsl::IsHLSLVecType(arrayEleTy)) {
        QualType vecEltTy = hlsl::GetHLSLVecElementType(arrayEleTy);
        if (!vecEltTy->isUnsignedIntegerType() || CGM.getContext().getTypeSize(vecEltTy) != 32) {
          unsigned DiagID = Diags.getCustomDiagID(
              DiagnosticsEngine::Error,
              "the element of out_indices array must be uint2 for line output or uint3 for triangle output");
          Diags.Report(parmDecl->getLocation(), DiagID);
          continue;
        }
        unsigned vecEltCount = hlsl::GetHLSLVecSize(arrayEleTy);
        if (funcProps->ShaderProps.MS.outputTopology == DXIL::MeshOutputTopology::Line && vecEltCount != 2) {
          unsigned DiagID = Diags.getCustomDiagID(
              DiagnosticsEngine::Error,
              "the element of out_indices array in a mesh shader whose output topology is line must be uint2");
          Diags.Report(parmDecl->getLocation(), DiagID);
          continue;
        }
        if (funcProps->ShaderProps.MS.outputTopology == DXIL::MeshOutputTopology::Triangle && vecEltCount != 3) {
          unsigned DiagID = Diags.getCustomDiagID(
              DiagnosticsEngine::Error,
              "the element of out_indices array in a mesh shader whose output topology is triangle must be uint3");
          Diags.Report(parmDecl->getLocation(), DiagID);
          continue;
        }
      } else {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "the element of out_indices array must be uint2 for line output or uint3 for triangle output");
        Diags.Report(parmDecl->getLocation(), DiagID);
        continue;
      }

      dxilInputQ = DxilParamInputQual::OutIndices;
      funcProps->ShaderProps.MS.maxPrimitiveCount = count;
      hasOutIndices = true;
    }
    if (parmDecl->hasAttr<HLSLOutAttr>() && parmDecl->hasAttr<HLSLVerticesAttr>()) {
      if (hasOutVertices) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "multiple out vertices parameters not allowed");
        Diags.Report(parmDecl->getLocation(), DiagID);
        continue;
      }
      const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(fieldTy.getCanonicalType());
      if (CAT == nullptr) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "vertices output is not an constant-length array");
        Diags.Report(parmDecl->getLocation(), DiagID);
        continue;
      }
      unsigned count = CAT->getSize().getZExtValue();
      if (count > DXIL::kMaxMSOutputVertexCount) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "max vertex count should not exceed %0");
        Diags.Report(parmDecl->getLocation(), DiagID) << DXIL::kMaxMSOutputVertexCount;
        continue;
      }

      dxilInputQ = DxilParamInputQual::OutVertices;
      funcProps->ShaderProps.MS.maxVertexCount = count;
      hasOutVertices = true;
    }
    if (parmDecl->hasAttr<HLSLOutAttr>() && parmDecl->hasAttr<HLSLPrimitivesAttr>()) {
      if (hasOutPrimitives) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "multiple out primitives parameters not allowed");
        Diags.Report(parmDecl->getLocation(), DiagID);
        continue;
      }
      const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(fieldTy.getCanonicalType());
      if (CAT == nullptr) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "primitives output is not an constant-length array");
        Diags.Report(parmDecl->getLocation(), DiagID);
        continue;
      }
      unsigned count = CAT->getSize().getZExtValue();
      if (count > DXIL::kMaxMSOutputPrimitiveCount) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "max primitive count should not exceed %0");
        Diags.Report(parmDecl->getLocation(), DiagID) << DXIL::kMaxMSOutputPrimitiveCount;
        continue;
      }
      if (funcProps->ShaderProps.MS.maxPrimitiveCount != 0 &&
        funcProps->ShaderProps.MS.maxPrimitiveCount != count) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "max primitive count mismatch");
        Diags.Report(parmDecl->getLocation(), DiagID);
        continue;
      }

      dxilInputQ = DxilParamInputQual::OutPrimitives;
      funcProps->ShaderProps.MS.maxPrimitiveCount = count;
      hasOutPrimitives = true;
    }
    if (parmDecl->hasAttr<HLSLInAttr>() && parmDecl->hasAttr<HLSLPayloadAttr>()) {
      if (hasInPayload) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "multiple in payload parameters not allowed");
        Diags.Report(parmDecl->getLocation(), DiagID);
        continue;
      }
      dxilInputQ = DxilParamInputQual::InPayload;
      DataLayout DL(&this->TheModule);
      funcProps->ShaderProps.MS.payloadSizeInBytes = DL.getTypeAllocSize(
        F->getFunctionType()->getFunctionParamType(ArgNo)->getPointerElementType());
      hasInPayload = true;
    }

    DXIL::InputPrimitive inputPrimitive = DXIL::InputPrimitive::Undefined;

    if (IsHLSLOutputPatchType(parmDecl->getType())) {
      outputPatchCount++;
      if (dxilInputQ != DxilParamInputQual::In) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "OutputPatch should not be out/inout parameter");
        Diags.Report(parmDecl->getLocation(), DiagID);
        continue;
      }
      dxilInputQ = DxilParamInputQual::OutputPatch;
      if (isDS)
        funcProps->ShaderProps.DS.inputControlPoints =
            GetHLSLOutputPatchCount(parmDecl->getType());
    } else if (IsHLSLInputPatchType(parmDecl->getType())) {
      inputPatchCount++;
      if (dxilInputQ != DxilParamInputQual::In) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "InputPatch should not be out/inout parameter");
        Diags.Report(parmDecl->getLocation(), DiagID);
        continue;
      }
      dxilInputQ = DxilParamInputQual::InputPatch;
      if (isHS) {
        funcProps->ShaderProps.HS.inputControlPoints =
            GetHLSLInputPatchCount(parmDecl->getType());
      } else if (isGS) {
        inputPrimitive = (DXIL::InputPrimitive)(
            (unsigned)DXIL::InputPrimitive::ControlPointPatch1 +
            GetHLSLInputPatchCount(parmDecl->getType()) - 1);
      }
    } else if (IsHLSLStreamOutputType(parmDecl->getType())) {
      // TODO: validation this at ASTContext::getFunctionType in
      // AST/ASTContext.cpp
      DXASSERT(dxilInputQ == DxilParamInputQual::Inout,
               "stream output parameter must be inout");
      switch (streamIndex) {
      case 0:
        dxilInputQ = DxilParamInputQual::OutStream0;
        break;
      case 1:
        dxilInputQ = DxilParamInputQual::OutStream1;
        break;
      case 2:
        dxilInputQ = DxilParamInputQual::OutStream2;
        break;
      case 3:
      default:
        // TODO: validation this at ASTContext::getFunctionType in
        // AST/ASTContext.cpp
        DXASSERT(streamIndex == 3, "stream number out of bound");
        dxilInputQ = DxilParamInputQual::OutStream3;
        break;
      }
      DXIL::PrimitiveTopology &streamTopology =
          funcProps->ShaderProps.GS.streamPrimitiveTopologies[streamIndex];
      if (IsHLSLPointStreamType(parmDecl->getType()))
        streamTopology = DXIL::PrimitiveTopology::PointList;
      else if (IsHLSLLineStreamType(parmDecl->getType()))
        streamTopology = DXIL::PrimitiveTopology::LineStrip;
      else {
        DXASSERT(IsHLSLTriangleStreamType(parmDecl->getType()),
                 "invalid StreamType");
        streamTopology = DXIL::PrimitiveTopology::TriangleStrip;
      }

      if (streamIndex > 0) {
        bool bAllPoint =
            streamTopology == DXIL::PrimitiveTopology::PointList &&
            funcProps->ShaderProps.GS.streamPrimitiveTopologies[0] ==
                DXIL::PrimitiveTopology::PointList;
        if (!bAllPoint) {
          unsigned DiagID = Diags.getCustomDiagID(
              DiagnosticsEngine::Error, "when multiple GS output streams are "
                                        "used they must be pointlists.");
          Diags.Report(FD->getLocation(), DiagID);
        }
      }

      streamIndex++;
    }

    unsigned GsInputArrayDim = 0;
    if (parmDecl->hasAttr<HLSLTriangleAttr>()) {
      inputPrimitive = DXIL::InputPrimitive::Triangle;
      GsInputArrayDim = 3;
    } else if (parmDecl->hasAttr<HLSLTriangleAdjAttr>()) {
      inputPrimitive = DXIL::InputPrimitive::TriangleWithAdjacency;
      GsInputArrayDim = 6;
    } else if (parmDecl->hasAttr<HLSLPointAttr>()) {
      inputPrimitive = DXIL::InputPrimitive::Point;
      GsInputArrayDim = 1;
    } else if (parmDecl->hasAttr<HLSLLineAdjAttr>()) {
      inputPrimitive = DXIL::InputPrimitive::LineWithAdjacency;
      GsInputArrayDim = 4;
    } else if (parmDecl->hasAttr<HLSLLineAttr>()) {
      inputPrimitive = DXIL::InputPrimitive::Line;
      GsInputArrayDim = 2;
    }

    if (inputPrimitive != DXIL::InputPrimitive::Undefined) {
      // Set to InputPrimitive for GS.
      dxilInputQ = DxilParamInputQual::InputPrimitive;
      if (funcProps->ShaderProps.GS.inputPrimitive ==
          DXIL::InputPrimitive::Undefined) {
        funcProps->ShaderProps.GS.inputPrimitive = inputPrimitive;
      } else if (funcProps->ShaderProps.GS.inputPrimitive != inputPrimitive) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error, "input parameter conflicts with geometry "
                                      "specifier of previous input parameters");
        Diags.Report(parmDecl->getLocation(), DiagID);
      }
    }

    if (GsInputArrayDim != 0) {
      QualType Ty = parmDecl->getType();
      if (!Ty->isConstantArrayType()) {
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "input types for geometry shader must be constant size arrays");
        Diags.Report(parmDecl->getLocation(), DiagID);
      } else {
        const ConstantArrayType *CAT = cast<ConstantArrayType>(Ty);
        if (CAT->getSize().getLimitedValue() != GsInputArrayDim) {
          StringRef primtiveNames[] = {
              "invalid",     // 0
              "point",       // 1
              "line",        // 2
              "triangle",    // 3
              "lineadj",     // 4
              "invalid",     // 5
              "triangleadj", // 6
          };
          DXASSERT(GsInputArrayDim < llvm::array_lengthof(primtiveNames),
                   "Invalid array dim");
          unsigned DiagID = Diags.getCustomDiagID(
              DiagnosticsEngine::Error, "array dimension for %0 must be %1");
          Diags.Report(parmDecl->getLocation(), DiagID)
              << primtiveNames[GsInputArrayDim] << GsInputArrayDim;
        }
      }
    }

    // Validate Ray Tracing function parameter (some validation may be pushed into front end)
    if (isRay) {
      switch (funcProps->shaderKind) {
      case DXIL::ShaderKind::RayGeneration:
      case DXIL::ShaderKind::Intersection:
        // RayGeneration and Intersection shaders are not allowed to have any input parameters
        Diags.Report(parmDecl->getLocation(), Diags.getCustomDiagID(
          DiagnosticsEngine::Error, "parameters are not allowed for %0 shader"))
            << (funcProps->shaderKind == DXIL::ShaderKind::RayGeneration ?
                "raygeneration" : "intersection");
        break;
      case DXIL::ShaderKind::AnyHit:
      case DXIL::ShaderKind::ClosestHit:
        if (0 == ArgNo && dxilInputQ != DxilParamInputQual::Inout) {
          Diags.Report(parmDecl->getLocation(), Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "ray payload parameter must be inout"));
        } else if (1 == ArgNo && dxilInputQ != DxilParamInputQual::In) {
          Diags.Report(parmDecl->getLocation(), Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "intersection attributes parameter must be in"));
        } else if (ArgNo > 1) {
          Diags.Report(parmDecl->getLocation(), Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "too many parameters, expected payload and attributes parameters only."));
        }
        if (ArgNo < 2) {
          if (!IsHLSLNumericUserDefinedType(parmDecl->getType())) {
            Diags.Report(parmDecl->getLocation(), Diags.getCustomDiagID(
              DiagnosticsEngine::Error,
              "payload and attribute structures must be user defined types with only numeric contents."));
          } else {
            DataLayout DL(&this->TheModule);
            unsigned size = DL.getTypeAllocSize(F->getFunctionType()->getFunctionParamType(ArgNo)->getPointerElementType());
            if (0 == ArgNo)
              funcProps->ShaderProps.Ray.payloadSizeInBytes = size;
            else
              funcProps->ShaderProps.Ray.attributeSizeInBytes = size;
          }
        }
        break;
      case DXIL::ShaderKind::Miss:
        if (ArgNo > 0) {
          Diags.Report(parmDecl->getLocation(), Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "only one parameter (ray payload) allowed for miss shader"));
        } else if (dxilInputQ != DxilParamInputQual::Inout) {
          Diags.Report(parmDecl->getLocation(), Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "ray payload parameter must be declared inout"));
        }
        if (ArgNo < 1) {
          if (!IsHLSLNumericUserDefinedType(parmDecl->getType())) {
            Diags.Report(parmDecl->getLocation(), Diags.getCustomDiagID(
              DiagnosticsEngine::Error,
              "ray payload parameter must be a user defined type with only numeric contents."));
          } else {
            DataLayout DL(&this->TheModule);
            unsigned size = DL.getTypeAllocSize(F->getFunctionType()->getFunctionParamType(ArgNo)->getPointerElementType());
            funcProps->ShaderProps.Ray.payloadSizeInBytes = size;
          }
        }
        break;
      case DXIL::ShaderKind::Callable:
        if (ArgNo > 0) {
          Diags.Report(parmDecl->getLocation(), Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "only one parameter allowed for callable shader"));
        } else if (dxilInputQ != DxilParamInputQual::Inout) {
          Diags.Report(parmDecl->getLocation(), Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "callable parameter must be declared inout"));
        }
        if (ArgNo < 1) {
          if (!IsHLSLNumericUserDefinedType(parmDecl->getType())) {
            Diags.Report(parmDecl->getLocation(), Diags.getCustomDiagID(
              DiagnosticsEngine::Error,
              "callable parameter must be a user defined type with only numeric contents."));
          } else {
            DataLayout DL(&this->TheModule);
            unsigned size = DL.getTypeAllocSize(F->getFunctionType()->getFunctionParamType(ArgNo)->getPointerElementType());
            funcProps->ShaderProps.Ray.paramSizeInBytes = size;
          }
        }
        break;
      }
    }

    paramAnnotation.SetParamInputQual(dxilInputQ);
    if (isEntry) {
      if (CGM.getLangOpts().EnableDX9CompatMode && paramAnnotation.HasSemanticString()) {
        RemapObsoleteSemantic(paramAnnotation, /*isPatchConstantFunction*/ false);
      }
      CheckParameterAnnotation(paramSemanticLoc, paramAnnotation,
                               /*isPatchConstantFunction*/ false);
    }
  }

  if (inputPatchCount > 1) {
    unsigned DiagID = Diags.getCustomDiagID(
        DiagnosticsEngine::Error, "may only have one InputPatch parameter");
    Diags.Report(FD->getLocation(), DiagID);
  }
  if (outputPatchCount > 1) {
    unsigned DiagID = Diags.getCustomDiagID(
        DiagnosticsEngine::Error, "may only have one OutputPatch parameter");
    Diags.Report(FD->getLocation(), DiagID);
  }

  // If Shader is a ray shader that requires parameters, make sure size is non-zero
  if (isRay) {
    bool bNeedsAttributes = false;
    bool bNeedsPayload = false;
    switch (funcProps->shaderKind) {
    case DXIL::ShaderKind::AnyHit:
    case DXIL::ShaderKind::ClosestHit:
      bNeedsAttributes = true;
    case DXIL::ShaderKind::Miss:
      bNeedsPayload = true;
    case DXIL::ShaderKind::Callable:
      if (0 == funcProps->ShaderProps.Ray.payloadSizeInBytes) {
        unsigned DiagID = bNeedsPayload ?
          Diags.getCustomDiagID(DiagnosticsEngine::Error,
            "shader must include inout payload structure parameter.") :
          Diags.getCustomDiagID(DiagnosticsEngine::Error,
            "shader must include inout parameter structure.");
        Diags.Report(FD->getLocation(), DiagID);
      }
    }
    if (bNeedsAttributes &&
        0 == funcProps->ShaderProps.Ray.attributeSizeInBytes) {
      Diags.Report(FD->getLocation(), Diags.getCustomDiagID(
        DiagnosticsEngine::Error,
        "shader must include attributes structure parameter."));
    }
  }

  // Type annotation for parameters and return type.
  DxilTypeSystem &dxilTypeSys = m_pHLModule->GetTypeSystem();
  unsigned arrayEltSize = 0;
  AddTypeAnnotation(FD->getReturnType(), dxilTypeSys, arrayEltSize);

  // Type annotation for this pointer.
  if (const CXXMethodDecl *MFD = dyn_cast<CXXMethodDecl>(FD)) {
    const CXXRecordDecl *RD = MFD->getParent();
    QualType Ty = CGM.getContext().getTypeDeclType(RD);
    AddTypeAnnotation(Ty, dxilTypeSys, arrayEltSize);
  }

  for (const ValueDecl *param : FD->params()) {
    QualType Ty = param->getType();
    AddTypeAnnotation(Ty, dxilTypeSys, arrayEltSize);
  }

  // clear isExportedEntry if not exporting entry
  bool isExportedEntry = profileAttributes != 0;
  if (isExportedEntry) {
    // use unmangled or mangled name depending on which is used for final entry function
    StringRef name = isRay ? F->getName() : FD->getName();
    if (!m_ExportMap.IsExported(name)) {
      isExportedEntry = false;
    }
  }

  // Only add functionProps when exist.
  if (isExportedEntry || isEntry)
    m_pHLModule->AddDxilFunctionProps(F, funcProps);
  if (isPatchConstantFunction)
    patchConstantFunctionPropsMap[F] = std::move(funcProps);

  // Save F to entry map.
  if (isExportedEntry) {
    if (entryFunctionMap.count(FD->getName())) {
      DiagnosticsEngine &Diags = CGM.getDiags();
      unsigned DiagID = Diags.getCustomDiagID(
          DiagnosticsEngine::Error,
          "redefinition of %0");
      Diags.Report(FD->getLocStart(), DiagID) << FD->getName();
    }
    auto &Entry = entryFunctionMap[FD->getNameAsString()];
    Entry.SL = FD->getLocation();
    Entry.Func= F;
  }

  // Add target-dependent experimental function attributes
  for (const auto &Attr : FD->specific_attrs<HLSLExperimentalAttr>()) {
    F->addFnAttr(Twine("exp-", Attr->getName()).str(), Attr->getValue());
  }
}

void CGMSHLSLRuntime::RemapObsoleteSemantic(DxilParameterAnnotation &paramInfo, bool isPatchConstantFunction) {
  DXASSERT(CGM.getLangOpts().EnableDX9CompatMode, "should be used only in back-compat mode");

  const ShaderModel *SM = m_pHLModule->GetShaderModel();
  DXIL::SigPointKind sigPointKind = SigPointFromInputQual(paramInfo.GetParamInputQual(), SM->GetKind(), isPatchConstantFunction);

  hlsl::RemapObsoleteSemantic(paramInfo, sigPointKind, CGM.getLLVMContext());
}

void CGMSHLSLRuntime::EmitHLSLFunctionProlog(Function *F, const FunctionDecl *FD) {
  // Support clip plane need debug info which not available when create function attribute.
  if (const HLSLClipPlanesAttr *Attr = FD->getAttr<HLSLClipPlanesAttr>()) {
    DxilFunctionProps &funcProps = m_pHLModule->GetDxilFunctionProps(F);
    // Initialize to null.
    memset(funcProps.ShaderProps.VS.clipPlanes, 0, sizeof(funcProps.ShaderProps.VS.clipPlanes));
    // Create global for each clip plane, and use the clip plane val as init val.
    auto AddClipPlane = [&](Expr *clipPlane, unsigned idx) {
      if (DeclRefExpr *decl = dyn_cast<DeclRefExpr>(clipPlane)) {
        const VarDecl *VD = cast<VarDecl>(decl->getDecl());
        Constant *clipPlaneVal = CGM.GetAddrOfGlobalVar(VD);
        funcProps.ShaderProps.VS.clipPlanes[idx] = clipPlaneVal;
        if (m_bDebugInfo) {
          CodeGenFunction CGF(CGM);
          ApplyDebugLocation applyDebugLoc(CGF, clipPlane);
          debugInfoMap[clipPlaneVal] = CGF.Builder.getCurrentDebugLocation();
        }
      } else {
        // Must be a MemberExpr.
        const MemberExpr *ME = cast<MemberExpr>(clipPlane);
        CodeGenFunction CGF(CGM);
        CodeGen::LValue LV = CGF.EmitMemberExpr(ME);
        Value *addr = LV.getAddress();
        funcProps.ShaderProps.VS.clipPlanes[idx] = cast<Constant>(addr);
        if (m_bDebugInfo) {
          CodeGenFunction CGF(CGM);
          ApplyDebugLocation applyDebugLoc(CGF, clipPlane);
          debugInfoMap[addr] = CGF.Builder.getCurrentDebugLocation();
        }
      }
    };

    if (Expr *clipPlane = Attr->getClipPlane1())
      AddClipPlane(clipPlane, 0);
    if (Expr *clipPlane = Attr->getClipPlane2())
      AddClipPlane(clipPlane, 1);
    if (Expr *clipPlane = Attr->getClipPlane3())
      AddClipPlane(clipPlane, 2);
    if (Expr *clipPlane = Attr->getClipPlane4())
      AddClipPlane(clipPlane, 3);
    if (Expr *clipPlane = Attr->getClipPlane5())
      AddClipPlane(clipPlane, 4);
    if (Expr *clipPlane = Attr->getClipPlane6())
      AddClipPlane(clipPlane, 5);

    clipPlaneFuncList.emplace_back(F);
  }

  // Update function linkage based on DefaultLinkage
  // We will take care of patch constant functions later, once identified for certain.
  if (!m_pHLModule->HasDxilFunctionProps(F)) {
    if (F->getLinkage() == GlobalValue::LinkageTypes::ExternalLinkage) {
      if (!FD->hasAttr<HLSLExportAttr>()) {
        switch (CGM.getCodeGenOpts().DefaultLinkage) {
        case DXIL::DefaultLinkage::Default:
          if (m_pHLModule->GetShaderModel()->GetMinor() != ShaderModel::kOfflineMinor)
            F->setLinkage(GlobalValue::LinkageTypes::InternalLinkage);
          break;
        case DXIL::DefaultLinkage::Internal:
          F->setLinkage(GlobalValue::LinkageTypes::InternalLinkage);
          break;
        }
      }
    }
  }
}

void CGMSHLSLRuntime::AddControlFlowHint(CodeGenFunction &CGF, const Stmt &S,
                                         llvm::TerminatorInst *TI,
                                         ArrayRef<const Attr *> Attrs) {
  // Build hints.
  bool bNoBranchFlatten = true;
  bool bBranch = false;
  bool bFlatten = false;

  std::vector<DXIL::ControlFlowHint> hints;
  for (const auto *Attr : Attrs) {
    if (isa<HLSLBranchAttr>(Attr)) {
      hints.emplace_back(DXIL::ControlFlowHint::Branch);
      bNoBranchFlatten = false;
      bBranch = true;
    }
    else if (isa<HLSLFlattenAttr>(Attr)) {
      hints.emplace_back(DXIL::ControlFlowHint::Flatten);
      bNoBranchFlatten = false;
      bFlatten = true;
    } else if (isa<HLSLForceCaseAttr>(Attr)) {
      if (isa<SwitchStmt>(&S)) {
        hints.emplace_back(DXIL::ControlFlowHint::ForceCase);
      }
    }
    // Ignore fastopt, allow_uav_condition and call for now.
  }

  if (bNoBranchFlatten) {
    // CHECK control flow option.
    if (CGF.CGM.getCodeGenOpts().HLSLPreferControlFlow)
      hints.emplace_back(DXIL::ControlFlowHint::Branch);
    else if (CGF.CGM.getCodeGenOpts().HLSLAvoidControlFlow)
      hints.emplace_back(DXIL::ControlFlowHint::Flatten);
  }

  if (bFlatten && bBranch) {
    DiagnosticsEngine &Diags = CGM.getDiags();
    unsigned DiagID = Diags.getCustomDiagID(
        DiagnosticsEngine::Error,
        "can't use branch and flatten attributes together");
    Diags.Report(S.getLocStart(), DiagID);
  }

  if (hints.size()) {      
    // Add meta data to the instruction.
    MDNode *hintsNode = DxilMDHelper::EmitControlFlowHints(Context, hints);
    TI->setMetadata(DxilMDHelper::kDxilControlFlowHintMDName, hintsNode);
  }
}

void CGMSHLSLRuntime::MarkRetTemp(CodeGenFunction &CGF, Value *V,
                                 QualType QualTy) {
  // Save resource properties for ret temp.
  DxilResourceProperties RP = BuildResourceProperty(QualTy);
  if (RP.Class != DXIL::ResourceClass::Invalid)
    valToResPropertiesMap[V] = RP;
}

void CGMSHLSLRuntime::FinishAutoVar(CodeGenFunction &CGF, const VarDecl &D,
                                    llvm::Value *V) {
  if (D.hasAttr<HLSLPreciseAttr>()) {
    AllocaInst *AI = cast<AllocaInst>(V);
    HLModule::MarkPreciseAttributeWithMetadata(AI);
  }
  // Add type annotation for local variable.
  DxilTypeSystem &typeSys = m_pHLModule->GetTypeSystem();
  unsigned arrayEltSize = 0;
  AddTypeAnnotation(D.getType(), typeSys, arrayEltSize);
  // Save resource properties for local variables.
  DxilResourceProperties RP = BuildResourceProperty(D.getType());
  if (RP.Class != DXIL::ResourceClass::Invalid)
    valToResPropertiesMap[V] = RP;
}

hlsl::InterpolationMode CGMSHLSLRuntime::GetInterpMode(const Decl *decl,
                                                       CompType compType,
                                                       bool bKeepUndefined) {
  InterpolationMode Interp(
      decl->hasAttr<HLSLNoInterpolationAttr>(), decl->hasAttr<HLSLLinearAttr>(),
      decl->hasAttr<HLSLNoPerspectiveAttr>(), decl->hasAttr<HLSLCentroidAttr>(),
      decl->hasAttr<HLSLSampleAttr>());
  DXASSERT(Interp.IsValid(), "otherwise front-end missing validation");
  if (Interp.IsUndefined() && !bKeepUndefined) {
    // Type-based default: linear for floats, constant for others.
    if (compType.IsFloatTy())
      Interp = InterpolationMode::Kind::Linear;
    else
      Interp = InterpolationMode::Kind::Constant;
  }
  return Interp;
}

hlsl::CompType CGMSHLSLRuntime::GetCompType(const BuiltinType *BT) {

  hlsl::CompType ElementType = hlsl::CompType::getInvalid();
  switch (BT->getKind()) {
  case BuiltinType::Bool:
    ElementType = hlsl::CompType::getI1();
    break;
  case BuiltinType::Double:
    ElementType = hlsl::CompType::getF64();
    break;
  case BuiltinType::HalfFloat: // HLSL Change
  case BuiltinType::Float:
    ElementType = hlsl::CompType::getF32();
    break;
  // HLSL Changes begin
  case BuiltinType::Min10Float:
  case BuiltinType::Min16Float:
  // HLSL Changes end
  case BuiltinType::Half:
    ElementType = hlsl::CompType::getF16();
    break;
  case BuiltinType::Int:
    ElementType = hlsl::CompType::getI32();
    break;
  case BuiltinType::LongLong:
    ElementType = hlsl::CompType::getI64();
    break;
  // HLSL Changes begin
  case BuiltinType::Min12Int:
  case BuiltinType::Min16Int:
  // HLSL Changes end
  case BuiltinType::Short:
    ElementType = hlsl::CompType::getI16();
    break;
  case BuiltinType::UInt:
    ElementType = hlsl::CompType::getU32();
    break;
  case BuiltinType::ULongLong:
    ElementType = hlsl::CompType::getU64();
    break;
  case BuiltinType::Min16UInt: // HLSL Change
  case BuiltinType::UShort:
    ElementType = hlsl::CompType::getU16();
    break;
  default:
    llvm_unreachable("unsupported type");
    break;
  }

  return ElementType;
}

/// Add resource to the program
void CGMSHLSLRuntime::addResource(Decl *D) {
  if (HLSLBufferDecl *BD = dyn_cast<HLSLBufferDecl>(D))
    GetOrCreateCBuffer(BD);
  else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
    hlsl::DxilResourceBase::Class resClass = TypeToClass(VD->getType());
    // Save resource properties for global variables.
    if (resClass != DXIL::ResourceClass::Invalid) {
      GlobalVariable *GV = cast<GlobalVariable>(CGM.GetAddrOfGlobalVar(VD));
      DxilResourceProperties RP = BuildResourceProperty(VD->getType());
      if (RP.Class != DXIL::ResourceClass::Invalid)
        valToResPropertiesMap[GV] = RP;
    }
    // skip decl has init which is resource.
    if (VD->hasInit() && resClass != DXIL::ResourceClass::Invalid)
      return;
    // skip static global.
    if (!VD->hasExternalFormalLinkage()) {
      if (VD->hasInit() && VD->getType().isConstQualified()) {
        Expr* InitExp = VD->getInit();
        GlobalVariable *GV = cast<GlobalVariable>(CGM.GetAddrOfGlobalVar(VD));
        // Only save const static global of struct type.
        if (GV->getType()->getElementType()->isStructTy()) {
          staticConstGlobalInitMap[InitExp] = GV;
        }
      }
      // Add type annotation for static global variable.
      DxilTypeSystem &typeSys = m_pHLModule->GetTypeSystem();
      unsigned arrayEltSize = 0;
      AddTypeAnnotation(VD->getType(), typeSys, arrayEltSize);
      return;
    }

    if (D->hasAttr<HLSLGroupSharedAttr>()) {
      GlobalVariable *GV = cast<GlobalVariable>(CGM.GetAddrOfGlobalVar(VD));
      DxilTypeSystem &dxilTypeSys = m_pHLModule->GetTypeSystem();
      unsigned arraySize = 0;
      AddTypeAnnotation(VD->getType(), dxilTypeSys, arraySize);
      m_pHLModule->AddGroupSharedVariable(GV);
      return;
    }

    switch (resClass) {
    case hlsl::DxilResourceBase::Class::Sampler:
      AddSampler(VD);
      break;
    case hlsl::DxilResourceBase::Class::UAV:
    case hlsl::DxilResourceBase::Class::SRV:
      AddUAVSRV(VD, resClass);
      break;
    case hlsl::DxilResourceBase::Class::Invalid: {
      // normal global constant, add to global CB
      HLCBuffer &globalCB = GetGlobalCBuffer();
      AddConstant(VD, globalCB);
      break;
    }
    case DXIL::ResourceClass::CBuffer:
      DXASSERT(0, "cbuffer should not be here");
      break;
    }
  }
}

/// Add subobject to the module
void CGMSHLSLRuntime::addSubobject(Decl *D) {
  VarDecl *VD = dyn_cast<VarDecl>(D);
  DXASSERT(VD != nullptr, "must be a global variable");

  DXIL::SubobjectKind subobjKind;
  DXIL::HitGroupType hgType;
  if (!hlsl::GetHLSLSubobjectKind(VD->getType(), subobjKind, hgType)) {
    DXASSERT(false, "not a valid subobject declaration");
    return;
  }

  Expr *initExpr = const_cast<Expr*>(VD->getAnyInitializer());
  if (!initExpr) {
    DiagnosticsEngine &Diags = CGM.getDiags();
    unsigned DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error, "subobject needs to be initialized");
    Diags.Report(D->getLocStart(), DiagID);
    return;
  }

  if (InitListExpr *initListExpr = dyn_cast<InitListExpr>(initExpr)) {
    try {
      CreateSubobject(subobjKind, VD->getName(), initListExpr->getInits(), initListExpr->getNumInits(), hgType);
    } catch (hlsl::Exception&) {
      DiagnosticsEngine &Diags = CGM.getDiags();
      unsigned DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error, "internal error creating subobject");
      Diags.Report(initExpr->getLocStart(), DiagID);
      return;
    }
  }
  else {
    DiagnosticsEngine &Diags = CGM.getDiags();
    unsigned DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error, "expected initialization list");
    Diags.Report(initExpr->getLocStart(), DiagID);
    return;
  }
}

// TODO: collect such helper utility functions in one place.
static DxilResourceBase::Class KeywordToClass(const std::string &keyword) {
  // TODO: refactor for faster search (switch by 1/2/3 first letters, then
  // compare)
  if (keyword == "SamplerState")
    return DxilResourceBase::Class::Sampler;
  
  if (keyword == "SamplerComparisonState")
    return DxilResourceBase::Class::Sampler;

  if (keyword == "ConstantBuffer")
    return DxilResourceBase::Class::CBuffer;

  if (keyword == "TextureBuffer")
    return DxilResourceBase::Class::SRV;

  bool isSRV = keyword == "Buffer";
  isSRV |= keyword == "ByteAddressBuffer";
  isSRV |= keyword == "RaytracingAccelerationStructure";
  isSRV |= keyword == "StructuredBuffer";
  isSRV |= keyword == "Texture1D";
  isSRV |= keyword == "Texture1DArray";
  isSRV |= keyword == "Texture2D";
  isSRV |= keyword == "Texture2DArray";
  isSRV |= keyword == "Texture3D";
  isSRV |= keyword == "TextureCube";
  isSRV |= keyword == "TextureCubeArray";
  isSRV |= keyword == "Texture2DMS";
  isSRV |= keyword == "Texture2DMSArray";
  if (isSRV)
    return DxilResourceBase::Class::SRV;

  bool isUAV = keyword == "RWBuffer";
  isUAV |= keyword == "RWByteAddressBuffer";
  isUAV |= keyword == "RWStructuredBuffer";
  isUAV |= keyword == "RWTexture1D";
  isUAV |= keyword == "RWTexture1DArray";
  isUAV |= keyword == "RWTexture2D";
  isUAV |= keyword == "RWTexture2DArray";
  isUAV |= keyword == "RWTexture3D";
  isUAV |= keyword == "RWTextureCube";
  isUAV |= keyword == "RWTextureCubeArray";
  isUAV |= keyword == "RWTexture2DMS";
  isUAV |= keyword == "RWTexture2DMSArray";
  isUAV |= keyword == "AppendStructuredBuffer";
  isUAV |= keyword == "ConsumeStructuredBuffer";
  isUAV |= keyword == "RasterizerOrderedBuffer";
  isUAV |= keyword == "RasterizerOrderedByteAddressBuffer";
  isUAV |= keyword == "RasterizerOrderedStructuredBuffer";
  isUAV |= keyword == "RasterizerOrderedTexture1D";
  isUAV |= keyword == "RasterizerOrderedTexture1DArray";
  isUAV |= keyword == "RasterizerOrderedTexture2D";
  isUAV |= keyword == "RasterizerOrderedTexture2DArray";
  isUAV |= keyword == "RasterizerOrderedTexture3D";
  isUAV |= keyword == "FeedbackTexture2D";
  isUAV |= keyword == "FeedbackTexture2DArray";
  if (isUAV)
    return DxilResourceBase::Class::UAV;

  return DxilResourceBase::Class::Invalid;
}

// This should probably be refactored to ASTContextHLSL, and follow types
// rather than do string comparisons.
DXIL::ResourceClass
hlsl::GetResourceClassForType(const clang::ASTContext &context,
                              clang::QualType Ty) {
  Ty = Ty.getCanonicalType();
  if (const clang::ArrayType *arrayType = context.getAsArrayType(Ty)) {
    return GetResourceClassForType(context, arrayType->getElementType());
  } else if (const RecordType *RT = Ty->getAsStructureType()) {
    return KeywordToClass(RT->getDecl()->getName());
  } else if (const RecordType *RT = Ty->getAs<RecordType>()) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(RT->getDecl())) {
      return KeywordToClass(templateDecl->getName());
    }
  }

  return hlsl::DxilResourceBase::Class::Invalid;
}

hlsl::DxilResourceBase::Class CGMSHLSLRuntime::TypeToClass(clang::QualType Ty) {
  return hlsl::GetResourceClassForType(CGM.getContext(), Ty);
}

namespace {
  void GetResourceDeclElemTypeAndRangeSize(CodeGenModule &CGM, HLModule &HL, VarDecl &VD,
    QualType &ElemType, unsigned& rangeSize) {
    // We can't canonicalize nor desugar the type without losing the 'snorm' in Buffer<snorm float>
    ElemType = VD.getType();
    rangeSize = 1;
    while (const clang::ArrayType *arrayType = CGM.getContext().getAsArrayType(ElemType)) {
      if (rangeSize != UINT_MAX) {
        if (arrayType->isConstantArrayType()) {
          rangeSize *= cast<ConstantArrayType>(arrayType)->getSize().getLimitedValue();
        }
        else {
          if (HL.GetHLOptions().bLegacyResourceReservation) {
            DiagnosticsEngine &Diags = CGM.getDiags();
            unsigned DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error,
              "unbounded resources are not supported with -flegacy-resource-reservation");
            Diags.Report(VD.getLocation(), DiagID);
          }
          rangeSize = UINT_MAX;
        }
      }
      ElemType = arrayType->getElementType();
    }
  }
}

static void InitFromUnusualAnnotations(DxilResourceBase &Resource, NamedDecl &Decl) {
  for (hlsl::UnusualAnnotation* It : Decl.getUnusualAnnotations()) {
    switch (It->getKind()) {
    case hlsl::UnusualAnnotation::UA_RegisterAssignment: {
      hlsl::RegisterAssignment* RegAssign = cast<hlsl::RegisterAssignment>(It);
      if (RegAssign->RegisterType) {
        Resource.SetLowerBound(RegAssign->RegisterNumber);
        // For backcompat, don't auto-assign the register space if there's an
        // explicit register type.
        Resource.SetSpaceID(RegAssign->RegisterSpace.getValueOr(0));
      }
      else {
        Resource.SetSpaceID(RegAssign->RegisterSpace.getValueOr(UINT_MAX));
      }
      break;
    }
    case hlsl::UnusualAnnotation::UA_SemanticDecl:
      // Ignore Semantics
      break;
    case hlsl::UnusualAnnotation::UA_ConstantPacking:
      // Should be handled by front-end
      llvm_unreachable("packoffset on resource");
      break;
    default:
      llvm_unreachable("unknown UnusualAnnotation on resource");
      break;
    }
  }
}

uint32_t CGMSHLSLRuntime::AddSampler(VarDecl *samplerDecl) {
  llvm::GlobalVariable *val =
    cast<llvm::GlobalVariable>(CGM.GetAddrOfGlobalVar(samplerDecl));

  unique_ptr<DxilSampler> hlslRes(new DxilSampler);
  hlslRes->SetLowerBound(UINT_MAX);
  hlslRes->SetSpaceID(UINT_MAX);
  hlslRes->SetGlobalSymbol(val);
  hlslRes->SetGlobalName(samplerDecl->getName());

  QualType VarTy;
  unsigned rangeSize;
  GetResourceDeclElemTypeAndRangeSize(CGM, *m_pHLModule, *samplerDecl,
    VarTy, rangeSize);
  hlslRes->SetRangeSize(rangeSize);

  const RecordType *RT = VarTy->getAs<RecordType>();
  DxilSampler::SamplerKind kind = KeywordToSamplerKind(RT->getDecl()->getName());

  hlslRes->SetSamplerKind(kind);
  InitFromUnusualAnnotations(*hlslRes, *samplerDecl);

  hlslRes->SetID(m_pHLModule->GetSamplers().size());
  return m_pHLModule->AddSampler(std::move(hlslRes));
}

bool CGMSHLSLRuntime::GetAsConstantUInt32(clang::Expr *expr, uint32_t *value) {
  APSInt result;
  if (!expr->EvaluateAsInt(result, CGM.getContext())) {
    DiagnosticsEngine &Diags = CGM.getDiags();
    unsigned DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error,
      "cannot convert to constant unsigned int");
    Diags.Report(expr->getLocStart(), DiagID);
    return false;
  }

  *value = result.getLimitedValue(UINT32_MAX);
  return true;
}

bool CGMSHLSLRuntime::GetAsConstantString(clang::Expr *expr, StringRef *value, bool failWhenEmpty /*=false*/) {
  Expr::EvalResult result;
  DiagnosticsEngine &Diags = CGM.getDiags();
  unsigned DiagID = 0;

  if (expr->EvaluateAsRValue(result, CGM.getContext())) {
    if (result.Val.isLValue()) {
      DXASSERT_NOMSG(result.Val.getLValueOffset().isZero());
      DXASSERT_NOMSG(result.Val.getLValueCallIndex() == 0);

      const Expr *evExpr = result.Val.getLValueBase().get<const Expr *>();
      if (const StringLiteral *strLit = dyn_cast<const StringLiteral>(evExpr)) {
        *value = strLit->getBytes();
        if (!failWhenEmpty || !(*value).empty()) {
          return true;
        }
        DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error, "empty string not expected here");
      }
    }
  }

  if (!DiagID)
    DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error, "cannot convert to constant string");
  Diags.Report(expr->getLocStart(), DiagID);
  return false;
}

std::vector<StringRef> CGMSHLSLRuntime::ParseSubobjectExportsAssociations(StringRef exports) {
  std::vector<StringRef> parsedExports;
  const char *pData = exports.data();
  const char *pEnd = pData + exports.size();
  const char *pLast = pData;

  while (pData < pEnd) {
    if (*pData == ';') {
      if (pLast < pData) {
        parsedExports.emplace_back(StringRef(pLast, pData - pLast));
      }
      pLast = pData + 1;
    }
    pData++;
  }
  if (pLast < pData) {
    parsedExports.emplace_back(StringRef(pLast, pData - pLast));
  }
  
  return std::move(parsedExports);
}


void CGMSHLSLRuntime::CreateSubobject(DXIL::SubobjectKind kind, const StringRef name,
                                      clang::Expr **args, unsigned int argCount,
                                      DXIL::HitGroupType hgType /*= (DXIL::HitGroupType)(-1)*/) {
  DxilSubobjects *subobjects = m_pHLModule->GetSubobjects();
  if (!subobjects) {
    subobjects = new DxilSubobjects();
    m_pHLModule->ResetSubobjects(subobjects);
  }
 
  DxilRootSignatureCompilationFlags flags = DxilRootSignatureCompilationFlags::GlobalRootSignature;
  switch (kind) {
    case DXIL::SubobjectKind::StateObjectConfig: {
      uint32_t flags;
      DXASSERT_NOMSG(argCount == 1);
      if (GetAsConstantUInt32(args[0], &flags)) {
        subobjects->CreateStateObjectConfig(name, flags);
      }
      break;
    }
    case DXIL::SubobjectKind::LocalRootSignature:
      flags = DxilRootSignatureCompilationFlags::LocalRootSignature;
      __fallthrough;
    case DXIL::SubobjectKind::GlobalRootSignature: {
      DXASSERT_NOMSG(argCount == 1);
      StringRef signature;
      if (!GetAsConstantString(args[0], &signature, true))
        return;

      RootSignatureHandle RootSigHandle;
      CompileRootSignature(signature, CGM.getDiags(), args[0]->getLocStart(), rootSigVer, flags, &RootSigHandle);

      if (!RootSigHandle.IsEmpty()) {
        RootSigHandle.EnsureSerializedAvailable();
        subobjects->CreateRootSignature(name, kind == DXIL::SubobjectKind::LocalRootSignature, 
          RootSigHandle.GetSerializedBytes(), RootSigHandle.GetSerializedSize(), &signature);
      }
      break;
    }
    case DXIL::SubobjectKind::SubobjectToExportsAssociation: {
      DXASSERT_NOMSG(argCount == 2);
      StringRef subObjName, exports;
      if (!GetAsConstantString(args[0], &subObjName, true) ||
          !GetAsConstantString(args[1], &exports, false))
        return;

      std::vector<StringRef> exportList = ParseSubobjectExportsAssociations(exports);
      subobjects->CreateSubobjectToExportsAssociation(name, subObjName, exportList.data(), exportList.size());
      break;
    }
    case DXIL::SubobjectKind::RaytracingShaderConfig: {
      DXASSERT_NOMSG(argCount == 2);
      uint32_t maxPayloadSize;
      uint32_t MaxAttributeSize;
      if (!GetAsConstantUInt32(args[0], &maxPayloadSize) ||
          !GetAsConstantUInt32(args[1], &MaxAttributeSize))
        return;

      subobjects->CreateRaytracingShaderConfig(name, maxPayloadSize, MaxAttributeSize);
      break;
    }
    case DXIL::SubobjectKind::RaytracingPipelineConfig: {
      DXASSERT_NOMSG(argCount == 1);
      uint32_t maxTraceRecursionDepth;
      if (!GetAsConstantUInt32(args[0], &maxTraceRecursionDepth))
        return;

      subobjects->CreateRaytracingPipelineConfig(name, maxTraceRecursionDepth);
      break;
    }
    case DXIL::SubobjectKind::HitGroup: {
      switch (hgType) {
        case DXIL::HitGroupType::Triangle: {
          DXASSERT_NOMSG(argCount == 2);
          StringRef anyhit, closesthit;
          if (!GetAsConstantString(args[0], &anyhit) ||
            !GetAsConstantString(args[1], &closesthit))
            return;
          subobjects->CreateHitGroup(name, DXIL::HitGroupType::Triangle, anyhit, closesthit, llvm::StringRef(""));
          break;
        }
        case DXIL::HitGroupType::ProceduralPrimitive: {
          DXASSERT_NOMSG(argCount == 3);
          StringRef anyhit, closesthit, intersection;
          if (!GetAsConstantString(args[0], &anyhit) ||
              !GetAsConstantString(args[1], &closesthit) ||
              !GetAsConstantString(args[2], &intersection, true))
            return;
          subobjects->CreateHitGroup(name, DXIL::HitGroupType::ProceduralPrimitive, anyhit, closesthit, intersection);
          break;
        }
        default:
          llvm_unreachable("unknown HitGroupType");
      }
      break;
    }
    case DXIL::SubobjectKind::RaytracingPipelineConfig1: {
      DXASSERT_NOMSG(argCount == 2);
      uint32_t maxTraceRecursionDepth;
      uint32_t raytracingPipelineFlags;
      if (!GetAsConstantUInt32(args[0], &maxTraceRecursionDepth))
        return;

      if (!GetAsConstantUInt32(args[1], &raytracingPipelineFlags))
        return;

      subobjects->CreateRaytracingPipelineConfig1(name, maxTraceRecursionDepth, raytracingPipelineFlags);
      break;
    }
    default:
      llvm_unreachable("unknown SubobjectKind");
      break;
  }
}


static void CollectScalarTypes(std::vector<QualType> &ScalarTys, QualType Ty) {
  if (Ty->isRecordType()) {
    if (hlsl::IsHLSLMatType(Ty)) {
      QualType EltTy = hlsl::GetHLSLMatElementType(Ty);
      unsigned row = 0;
      unsigned col = 0;
      hlsl::GetRowsAndCols(Ty, row, col);
      unsigned size = col*row;
      for (unsigned i = 0; i < size; i++) {
        CollectScalarTypes(ScalarTys, EltTy);
      }
    } else if (hlsl::IsHLSLVecType(Ty)) {
      QualType EltTy = hlsl::GetHLSLVecElementType(Ty);
      unsigned row = 0;
      unsigned col = 0;
      hlsl::GetRowsAndColsForAny(Ty, row, col);
      unsigned size = col;
      for (unsigned i = 0; i < size; i++) {
        CollectScalarTypes(ScalarTys, EltTy);
      }
    } else {
      const RecordType *RT = Ty->getAsStructureType();
      // For CXXRecord.
      if (!RT)
        RT = Ty->getAs<RecordType>();
      RecordDecl *RD = RT->getDecl();
      for (FieldDecl *field : RD->fields())
        CollectScalarTypes(ScalarTys, field->getType());
    }
  } else if (Ty->isArrayType()) {
    const clang::ArrayType *AT = Ty->getAsArrayTypeUnsafe();
    QualType EltTy = AT->getElementType();
    // Set it to 5 for unsized array.
    unsigned size = 5;
    if (AT->isConstantArrayType()) {
      size = cast<ConstantArrayType>(AT)->getSize().getLimitedValue();
    }
    for (unsigned i=0;i<size;i++) {
      CollectScalarTypes(ScalarTys, EltTy);
    }
  } else {
    ScalarTys.emplace_back(Ty);
  }
}

bool CGMSHLSLRuntime::SetUAVSRV(SourceLocation loc,
                                hlsl::DxilResourceBase::Class resClass,
                                DxilResource *hlslRes, QualType QualTy) {
  RecordDecl *RD = QualTy->getAs<RecordType>()->getDecl();

  hlsl::DxilResource::Kind kind = KeywordToKind(RD->getName());
  DXASSERT_NOMSG(kind != hlsl::DxilResource::Kind::Invalid);

  hlslRes->SetKind(kind);
  
  // Type annotation for result type of resource.
  DxilTypeSystem &dxilTypeSys = m_pHLModule->GetTypeSystem();
  unsigned arrayEltSize = 0;
  AddTypeAnnotation(QualType(RD->getTypeForDecl(),0), dxilTypeSys, arrayEltSize);

  if (kind == hlsl::DxilResource::Kind::Texture2DMS ||
      kind == hlsl::DxilResource::Kind::Texture2DMSArray) {
    const ClassTemplateSpecializationDecl *templateDecl =
        cast<ClassTemplateSpecializationDecl>(RD);
    const clang::TemplateArgument &sampleCountArg =
        templateDecl->getTemplateArgs()[1];
    uint32_t sampleCount = sampleCountArg.getAsIntegral().getLimitedValue();
    hlslRes->SetSampleCount(sampleCount);
  }

  if (hlsl::DxilResource::IsAnyTexture(kind)) {
    const ClassTemplateSpecializationDecl *templateDecl = cast<ClassTemplateSpecializationDecl>(RD);
    const clang::TemplateArgument &texelTyArg = templateDecl->getTemplateArgs()[0];
    llvm::Type *texelTy = CGM.getTypes().ConvertType(texelTyArg.getAsType());
    if (!texelTy->isFloatingPointTy() && !texelTy->isIntegerTy()
      && !hlsl::IsHLSLVecType(texelTyArg.getAsType())) {
      DiagnosticsEngine &Diags = CGM.getDiags();
      unsigned DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error,
        "texture resource texel type must be scalar or vector");
      Diags.Report(loc, DiagID);
      return false;
    }
  }

  QualType resultTy = hlsl::GetHLSLResourceResultType(QualTy);
  if (kind != hlsl::DxilResource::Kind::StructuredBuffer && !resultTy.isNull()) {
    QualType Ty = resultTy;
    QualType EltTy = Ty;
    if (hlsl::IsHLSLVecType(Ty)) {
      EltTy = hlsl::GetHLSLVecElementType(Ty);
    } else if (hlsl::IsHLSLMatType(Ty)) {
      EltTy = hlsl::GetHLSLMatElementType(Ty);
    } else if (hlsl::IsHLSLAggregateType(resultTy)) {
      // Struct or array in a none-struct resource.
      std::vector<QualType> ScalarTys;
      CollectScalarTypes(ScalarTys, resultTy);
      unsigned size = ScalarTys.size();
      if (size == 0) {
        DiagnosticsEngine &Diags = CGM.getDiags();
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "object's templated type must have at least one element");
        Diags.Report(loc, DiagID);
        return false;
      }
      if (size > 4) {
        DiagnosticsEngine &Diags = CGM.getDiags();
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error, "elements of typed buffers and textures "
                                      "must fit in four 32-bit quantities");
        Diags.Report(loc, DiagID);
        return false;
      }

      EltTy = ScalarTys[0];
      for (QualType ScalarTy : ScalarTys) {
        if (ScalarTy != EltTy) {
          DiagnosticsEngine &Diags = CGM.getDiags();
          unsigned DiagID = Diags.getCustomDiagID(
              DiagnosticsEngine::Error,
              "all template type components must have the same type");
          Diags.Report(loc, DiagID);
          return false;
        }
      }
    }

    bool bSNorm = false;
    bool bHasNormAttribute = hlsl::HasHLSLUNormSNorm(Ty, &bSNorm);

    if (const BuiltinType *BTy = EltTy->getAs<BuiltinType>()) {
      CompType::Kind kind = BuiltinTyToCompTy(BTy, bHasNormAttribute && bSNorm, bHasNormAttribute && !bSNorm);
      // 64bits types are implemented with u32.
      if (kind == CompType::Kind::U64 || kind == CompType::Kind::I64 ||
          kind == CompType::Kind::SNormF64 ||
          kind == CompType::Kind::UNormF64 || kind == CompType::Kind::F64) {
        kind = CompType::Kind::U32;
      }
      hlslRes->SetCompType(kind);
    } else {
      DXASSERT(!bHasNormAttribute, "snorm/unorm on invalid type");
    }
  }

  if (hlslRes->IsFeedbackTexture()) {
    hlslRes->SetSamplerFeedbackType(
      static_cast<DXIL::SamplerFeedbackType>(hlsl::GetHLSLResourceTemplateUInt(QualTy)));
  }

  hlslRes->SetROV(RD->getName().startswith("RasterizerOrdered"));

  if (kind == hlsl::DxilResource::Kind::TypedBuffer ||
      kind == hlsl::DxilResource::Kind::StructuredBuffer) {
    const ClassTemplateSpecializationDecl *templateDecl =
        cast<ClassTemplateSpecializationDecl>(RD);

    const clang::TemplateArgument &retTyArg =
        templateDecl->getTemplateArgs()[0];
    llvm::Type *retTy = CGM.getTypes().ConvertType(retTyArg.getAsType());

    uint32_t strideInBytes = dataLayout.getTypeAllocSize(retTy);
    hlslRes->SetElementStride(strideInBytes);
  }
  if (HasHLSLGloballyCoherent(QualTy)) {
    hlslRes->SetGloballyCoherent(true);
  }
  if (resClass == hlsl::DxilResourceBase::Class::SRV) {
    if (hlslRes->IsGloballyCoherent()) {
      DiagnosticsEngine &Diags = CGM.getDiags();
      unsigned DiagID = Diags.getCustomDiagID(
          DiagnosticsEngine::Error, "globallycoherent can only be used with "
                                    "Unordered Access View buffers.");
      Diags.Report(loc, DiagID);
      return false;
    }

    hlslRes->SetRW(false);
    hlslRes->SetID(m_pHLModule->GetSRVs().size());
  } else {
    hlslRes->SetRW(true);
    hlslRes->SetID(m_pHLModule->GetUAVs().size());
  }
  return true;
}

uint32_t CGMSHLSLRuntime::AddUAVSRV(VarDecl *decl,
                                    hlsl::DxilResourceBase::Class resClass) {
  llvm::GlobalVariable *val =
      cast<llvm::GlobalVariable>(CGM.GetAddrOfGlobalVar(decl));

  unique_ptr<HLResource> hlslRes(new HLResource);
  hlslRes->SetLowerBound(UINT_MAX);
  hlslRes->SetSpaceID(UINT_MAX);
  hlslRes->SetGlobalSymbol(val);
  hlslRes->SetGlobalName(decl->getName());

  QualType VarTy;
  unsigned rangeSize;
  GetResourceDeclElemTypeAndRangeSize(CGM, *m_pHLModule, *decl,
    VarTy, rangeSize);
  hlslRes->SetRangeSize(rangeSize);
  InitFromUnusualAnnotations(*hlslRes, *decl);

  if (decl->hasAttr<HLSLGloballyCoherentAttr>()) {
    hlslRes->SetGloballyCoherent(true);
  }

  if (!SetUAVSRV(decl->getLocation(), resClass, hlslRes.get(), VarTy))
    return 0;

  if (resClass == hlsl::DxilResourceBase::Class::SRV) {
    return m_pHLModule->AddSRV(std::move(hlslRes));
  } else {
    return m_pHLModule->AddUAV(std::move(hlslRes));
  }
}

static bool IsResourceInType(const clang::ASTContext &context,
                              clang::QualType Ty) {
  Ty = Ty.getCanonicalType();
  if (const clang::ArrayType *arrayType = context.getAsArrayType(Ty)) {
    return IsResourceInType(context, arrayType->getElementType());
  } else if (const RecordType *RT = Ty->getAsStructureType()) {
    if (KeywordToClass(RT->getDecl()->getName()) != DxilResourceBase::Class::Invalid)
      return true;
    const CXXRecordDecl* typeRecordDecl = RT->getAsCXXRecordDecl();
    if (typeRecordDecl && !typeRecordDecl->isImplicit()) {
      for (auto field : typeRecordDecl->fields()) {
        if (IsResourceInType(context, field->getType()))
          return true;
      }
    }
  } else if (const RecordType *RT = Ty->getAs<RecordType>()) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(RT->getDecl())) {
      if (KeywordToClass(templateDecl->getName()) != DxilResourceBase::Class::Invalid)
        return true;
    }
  }

  return false; // no resources found
}

void CGMSHLSLRuntime::AddConstant(VarDecl *constDecl, HLCBuffer &CB) {
  if (constDecl->getStorageClass() == SC_Static) {
    // For static inside cbuffer, take as global static.
    // Don't add to cbuffer.
    CGM.EmitGlobal(constDecl);
    // Add type annotation for static global types.
    // May need it when cast from cbuf.
    DxilTypeSystem &dxilTypeSys = m_pHLModule->GetTypeSystem();
    unsigned arraySize = 0;
    AddTypeAnnotation(constDecl->getType(), dxilTypeSys, arraySize);
    return;
  }
  // Search defined structure for resource objects and fail
  if (CB.GetRangeSize() > 1 &&
      IsResourceInType(CGM.getContext(), constDecl->getType())) {
    DiagnosticsEngine &Diags = CGM.getDiags();
    unsigned DiagID = Diags.getCustomDiagID(
        DiagnosticsEngine::Error,
        "object types not supported in cbuffer/tbuffer view arrays.");
    Diags.Report(constDecl->getLocation(), DiagID);
    return;
  }
  llvm::Constant *constVal = CGM.GetAddrOfGlobalVar(constDecl);
  auto &regBindings = constantRegBindingMap[constVal];
  // Save resource properties for cbuffer variables.
  DxilResourceProperties RP = BuildResourceProperty(constDecl->getType());
  if (RP.Class != DXIL::ResourceClass::Invalid)
    valToResPropertiesMap[constVal] = RP;

  bool isGlobalCB = CB.GetID() == globalCBIndex;
  uint32_t offset = 0;
  bool userOffset = false;
  for (hlsl::UnusualAnnotation *it : constDecl->getUnusualAnnotations()) {
    switch (it->getKind()) {
    case hlsl::UnusualAnnotation::UA_ConstantPacking: {
      if (!isGlobalCB) {
        // TODO: check cannot mix packoffset elements with nonpackoffset
        // elements in a cbuffer.
        hlsl::ConstantPacking *cp = cast<hlsl::ConstantPacking>(it);
        offset = cp->Subcomponent << 2;
        offset += cp->ComponentOffset;
        // Change to byte.
        offset <<= 2;
        userOffset = true;
      } else {
        DiagnosticsEngine &Diags = CGM.getDiags();
        unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "packoffset is only allowed in a constant buffer.");
        Diags.Report(it->Loc, DiagID);
      }
      break;
    }
    case hlsl::UnusualAnnotation::UA_RegisterAssignment: {
      RegisterAssignment *ra = cast<RegisterAssignment>(it);
      if (isGlobalCB) {
        if (ra->RegisterSpace.hasValue()) {
          DiagnosticsEngine& Diags = CGM.getDiags();
          unsigned DiagID = Diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "register space cannot be specified on global constants.");
          Diags.Report(it->Loc, DiagID);
        }
        offset = ra->RegisterNumber << 2;
        // Change to byte.
        offset <<= 2;
        userOffset = true;
      }
      switch (ra->RegisterType) {
      default:
        break;
      case 't':
        regBindings.emplace_back(
            std::make_pair(DXIL::ResourceClass::SRV, ra->RegisterNumber));
        break;
      case 'u':
        regBindings.emplace_back(
            std::make_pair(DXIL::ResourceClass::UAV, ra->RegisterNumber));
        break;
      case 's':
        regBindings.emplace_back(
            std::make_pair(DXIL::ResourceClass::Sampler, ra->RegisterNumber));
        break;
      }
      break;
    }
    case hlsl::UnusualAnnotation::UA_SemanticDecl:
      // skip semantic on constant
      break;
    }
  }

  std::unique_ptr<DxilResourceBase> pHlslConst = llvm::make_unique<DxilResourceBase>(DXIL::ResourceClass::Invalid);
  pHlslConst->SetLowerBound(UINT_MAX);
  pHlslConst->SetSpaceID(0);
  pHlslConst->SetGlobalSymbol(cast<llvm::GlobalVariable>(constVal));
  pHlslConst->SetGlobalName(constDecl->getName());

  if (userOffset) {
    pHlslConst->SetLowerBound(offset);
  }
  
  DxilTypeSystem &dxilTypeSys = m_pHLModule->GetTypeSystem();
  // Just add type annotation here.
  // Offset will be allocated later.
  QualType Ty = constDecl->getType();
  if (CB.GetRangeSize() != 1) {
    while (Ty->isArrayType()) {
      Ty = Ty->getAsArrayTypeUnsafe()->getElementType();
    }
  }
  unsigned arrayEltSize = 0;
  unsigned size = AddTypeAnnotation(Ty, dxilTypeSys, arrayEltSize);
  pHlslConst->SetRangeSize(size);

  CB.AddConst(pHlslConst);

  // Save fieldAnnotation for the const var.
  DxilFieldAnnotation fieldAnnotation;
  if (userOffset)
    fieldAnnotation.SetCBufferOffset(offset);

  // Get the nested element type.
  if (Ty->isArrayType()) {
    while (const ConstantArrayType *arrayTy =
               CGM.getContext().getAsConstantArrayType(Ty)) {
      Ty = arrayTy->getElementType();
    }
  }
  bool bDefaultRowMajor = m_pHLModule->GetHLOptions().bDefaultRowMajor;
  ConstructFieldAttributedAnnotation(fieldAnnotation, Ty, bDefaultRowMajor);
  m_ConstVarAnnotationMap[constVal] = fieldAnnotation;
}

uint32_t CGMSHLSLRuntime::AddCBuffer(HLSLBufferDecl *D) {
  unique_ptr<HLCBuffer> CB = llvm::make_unique<HLCBuffer>();

  // setup the CB
  CB->SetGlobalSymbol(nullptr);
  CB->SetGlobalName(D->getNameAsString());
  CB->SetSpaceID(UINT_MAX);
  CB->SetLowerBound(UINT_MAX);
  if (!D->isCBuffer()) {
    CB->SetKind(DXIL::ResourceKind::TBuffer);
  }

  // the global variable will only used once by the createHandle?
  // SetHandle(llvm::Value *pHandle);

  InitFromUnusualAnnotations(*CB, *D);

  // Add constant
  if (D->isConstantBufferView()) {
    VarDecl *constDecl = cast<VarDecl>(*D->decls_begin());
    CB->SetRangeSize(1);
    QualType Ty = constDecl->getType();
    if (Ty->isArrayType()) {
      if (!Ty->isIncompleteArrayType()) {
        unsigned arraySize = 1;
        while (Ty->isArrayType()) {
          Ty = Ty->getCanonicalTypeUnqualified();
          const ConstantArrayType *AT = cast<ConstantArrayType>(Ty);
          arraySize *= AT->getSize().getLimitedValue();
          Ty = AT->getElementType();
        }
        CB->SetRangeSize(arraySize);
      } else {
        CB->SetRangeSize(UINT_MAX);
      }
    }
    AddConstant(constDecl, *CB.get());
  } else {
    auto declsEnds = D->decls_end();
    CB->SetRangeSize(1);
    for (auto it = D->decls_begin(); it != declsEnds; it++) {
      if (VarDecl *constDecl = dyn_cast<VarDecl>(*it)) {
        AddConstant(constDecl, *CB.get());
      } else if (isa<EmptyDecl>(*it)) {
        // Nothing to do for this declaration.
      } else if (isa<CXXRecordDecl>(*it)) {
        // Nothing to do for this declaration.
      } else if (isa<FunctionDecl>(*it)) {
        // A function within an cbuffer is effectively a top-level function,
        // as it only refers to globally scoped declarations.
        this->CGM.EmitTopLevelDecl(*it);
      } else {
        HLSLBufferDecl *inner = cast<HLSLBufferDecl>(*it);
        GetOrCreateCBuffer(inner);
      }
    }
  }

  CB->SetID(m_pHLModule->GetCBuffers().size());
  return m_pHLModule->AddCBuffer(std::move(CB));
}

HLCBuffer &CGMSHLSLRuntime::GetOrCreateCBuffer(HLSLBufferDecl *D) {
  if (constantBufMap.count(D) != 0) {
    uint32_t cbIndex = constantBufMap[D];
    return *static_cast<HLCBuffer*>(&(m_pHLModule->GetCBuffer(cbIndex)));
  }

  uint32_t cbID = AddCBuffer(D);
  constantBufMap[D] = cbID;
  return *static_cast<HLCBuffer*>(&(m_pHLModule->GetCBuffer(cbID)));
}

void CGMSHLSLRuntime::FinishCodeGen() {
  HLModule &HLM = *m_pHLModule;
  llvm::Module &M = TheModule;

  // Do this before CloneShaderEntry and TranslateRayQueryConstructor to avoid
  // update valToResPropertiesMap for cloned inst.
  FinishIntrinsics(HLM, m_IntrinsicMap, valToResPropertiesMap);

  FinishEntries(HLM, Entry, CGM, entryFunctionMap, HSEntryPatchConstantFuncAttr,
                patchConstantFunctionMap, patchConstantFunctionPropsMap);

  ReplaceConstStaticGlobals(staticConstGlobalInitListMap,
                            staticConstGlobalCtorMap);

  // Create copy for clip plane.
  if (!clipPlaneFuncList.empty()) {
    FinishClipPlane(HLM, clipPlaneFuncList, debugInfoMap, CGM);
  }

  // Add Reg bindings for resource in cb.
  AddRegBindingsForResourceInConstantBuffer(HLM, constantRegBindingMap);

  // Allocate constant buffers.
  // Create Global variable and type annotation for each CBuffer.
  FinishCBuffer(HLM, CBufferType, m_ConstVarAnnotationMap);

  // Translate calls to RayQuery constructor into hl Allocate calls
  TranslateRayQueryConstructor(HLM);

  bool bIsLib = HLM.GetShaderModel()->IsLib();
  if (!bIsLib) {
    // need this for "llvm.global_dtors"?
    ProcessCtorFunctions(M, "llvm.global_ctors",
                         Entry.Func->getEntryBlock().getFirstInsertionPt());
  }

  UpdateLinkage(HLM, CGM, m_ExportMap, entryFunctionMap,
                patchConstantFunctionMap);

  // Do simple transform to make later lower pass easier.
  SimpleTransformForHLDXIR(&M);

  // Add dx.break function and make appropriate breaks conditional on it.
  AddDxBreak(M, m_DxBreaks);

  // Handle lang extensions if provided.
  if (CGM.getCodeGenOpts().HLSLExtensionsCodegen) {
    ExtensionCodeGen(HLM, CGM);
  }
  // At this point, we have a high-level DXIL module - record this.
  SetPauseResumePasses(*m_pHLModule->GetModule(), "hlsl-hlemit",
                       "hlsl-hlensure");
}

RValue CGMSHLSLRuntime::EmitHLSLBuiltinCallExpr(CodeGenFunction &CGF,
                                                const FunctionDecl *FD,
                                                const CallExpr *E,
                                                ReturnValueSlot ReturnValue) {
  const Decl *TargetDecl = E->getCalleeDecl();
  llvm::Value *Callee = CGF.EmitScalarExpr(E->getCallee());
  RValue RV = CGF.EmitCall(E->getCallee()->getType(), Callee, E, ReturnValue,
                      TargetDecl);
  if (RV.isScalar() && RV.getScalarVal() != nullptr) {
    if (CallInst *CI = dyn_cast<CallInst>(RV.getScalarVal())) {
      Function *F = CI->getCalledFunction();
      HLOpcodeGroup group = hlsl::GetHLOpcodeGroup(F);
      if (group == HLOpcodeGroup::HLIntrinsic) {
        bool allOperandImm = true;
        for (auto &operand : CI->arg_operands()) {
          bool isImm = isa<ConstantInt>(operand) || isa<ConstantFP>(operand) ||
              isa<ConstantAggregateZero>(operand) || isa<ConstantDataVector>(operand);
          if (!isImm) {
            allOperandImm = false;
            break;
          } else if (operand->getType()->isHalfTy()) {
            // Not support half Eval yet.
            allOperandImm = false;
            break;
          }
        }
        if (allOperandImm) {
          unsigned intrinsicOpcode;
          StringRef intrinsicGroup;
          hlsl::GetIntrinsicOp(FD, intrinsicOpcode, intrinsicGroup);
          IntrinsicOp opcode = static_cast<IntrinsicOp>(intrinsicOpcode);
          if (Value *Result = TryEvalIntrinsic(CI, opcode)) {
            RV = RValue::get(Result);
          }
        }
      }
    }
  }
  return RV;
}

static HLOpcodeGroup GetHLOpcodeGroup(const clang::Stmt::StmtClass stmtClass) {
  switch (stmtClass) {
  case Stmt::CStyleCastExprClass:
  case Stmt::ImplicitCastExprClass:
  case Stmt::CXXFunctionalCastExprClass:
    return HLOpcodeGroup::HLCast;
  case Stmt::InitListExprClass:
    return HLOpcodeGroup::HLInit;
  case Stmt::BinaryOperatorClass:
  case Stmt::CompoundAssignOperatorClass:
    return HLOpcodeGroup::HLBinOp;
  case Stmt::UnaryOperatorClass:
    return HLOpcodeGroup::HLUnOp;
  case Stmt::ExtMatrixElementExprClass:
    return HLOpcodeGroup::HLSubscript;
  case Stmt::CallExprClass:
    return HLOpcodeGroup::HLIntrinsic;
  case Stmt::ConditionalOperatorClass:
    return HLOpcodeGroup::HLSelect;
  default:
    llvm_unreachable("not support operation");
  }
}

// NOTE: This table must match BinaryOperator::Opcode
static const HLBinaryOpcode BinaryOperatorKindMap[] = {
    HLBinaryOpcode::Invalid, // PtrMemD
    HLBinaryOpcode::Invalid, // PtrMemI
    HLBinaryOpcode::Mul, HLBinaryOpcode::Div, HLBinaryOpcode::Rem,
    HLBinaryOpcode::Add, HLBinaryOpcode::Sub, HLBinaryOpcode::Shl,
    HLBinaryOpcode::Shr, HLBinaryOpcode::LT, HLBinaryOpcode::GT,
    HLBinaryOpcode::LE, HLBinaryOpcode::GE, HLBinaryOpcode::EQ,
    HLBinaryOpcode::NE, HLBinaryOpcode::And, HLBinaryOpcode::Xor,
    HLBinaryOpcode::Or, HLBinaryOpcode::LAnd, HLBinaryOpcode::LOr,
    HLBinaryOpcode::Invalid, // Assign,
    // The assign part is done by matrix store
    HLBinaryOpcode::Mul,     // MulAssign
    HLBinaryOpcode::Div,     // DivAssign
    HLBinaryOpcode::Rem,     // RemAssign
    HLBinaryOpcode::Add,     // AddAssign
    HLBinaryOpcode::Sub,     // SubAssign
    HLBinaryOpcode::Shl,     // ShlAssign
    HLBinaryOpcode::Shr,     // ShrAssign
    HLBinaryOpcode::And,     // AndAssign
    HLBinaryOpcode::Xor,     // XorAssign
    HLBinaryOpcode::Or,      // OrAssign
    HLBinaryOpcode::Invalid, // Comma
};

// NOTE: This table must match UnaryOperator::Opcode
static const HLUnaryOpcode UnaryOperatorKindMap[] = {
    HLUnaryOpcode::PostInc, HLUnaryOpcode::PostDec,
    HLUnaryOpcode::PreInc,  HLUnaryOpcode::PreDec,
    HLUnaryOpcode::Invalid, // AddrOf,
    HLUnaryOpcode::Invalid, // Deref,
    HLUnaryOpcode::Plus,    HLUnaryOpcode::Minus,
    HLUnaryOpcode::Not,     HLUnaryOpcode::LNot,
    HLUnaryOpcode::Invalid, // Real,
    HLUnaryOpcode::Invalid, // Imag,
    HLUnaryOpcode::Invalid, // Extension
};

static unsigned GetHLOpcode(const Expr *E) {
  switch (E->getStmtClass()) {
  case Stmt::CompoundAssignOperatorClass:
  case Stmt::BinaryOperatorClass: {
    const clang::BinaryOperator *binOp = cast<clang::BinaryOperator>(E);
    HLBinaryOpcode binOpcode = BinaryOperatorKindMap[binOp->getOpcode()];
    if (HasUnsignedOpcode(binOpcode)) {
      if (hlsl::IsHLSLUnsigned(binOp->getLHS()->getType())) {
        binOpcode = GetUnsignedOpcode(binOpcode);
      }
    }
    return static_cast<unsigned>(binOpcode);
  }
  case Stmt::UnaryOperatorClass: {
    const UnaryOperator *unOp = cast<clang::UnaryOperator>(E);
    HLUnaryOpcode unOpcode = UnaryOperatorKindMap[unOp->getOpcode()];
    return static_cast<unsigned>(unOpcode);
  }
  case Stmt::ImplicitCastExprClass:
  case Stmt::CStyleCastExprClass: {
    const CastExpr *CE = cast<CastExpr>(E);
    bool toUnsigned = hlsl::IsHLSLUnsigned(E->getType());
    bool fromUnsigned = hlsl::IsHLSLUnsigned(CE->getSubExpr()->getType());
    if (toUnsigned && fromUnsigned)
      return static_cast<unsigned>(HLCastOpcode::UnsignedUnsignedCast);
    else if (toUnsigned)
      return static_cast<unsigned>(HLCastOpcode::ToUnsignedCast);
    else if (fromUnsigned)
      return static_cast<unsigned>(HLCastOpcode::FromUnsignedCast);
    else
      return static_cast<unsigned>(HLCastOpcode::DefaultCast);
  }
  default:
    return 0;
  }
}

static Value *
EmitHLSLMatrixOperationCallImp(CGBuilderTy &Builder, HLOpcodeGroup group,
                               unsigned opcode, llvm::Type *RetType,
                               ArrayRef<Value *> paramList, llvm::Module &M) {
  SmallVector<llvm::Type *, 4> paramTyList;
  // Add the opcode param
  llvm::Type *opcodeTy = llvm::Type::getInt32Ty(M.getContext());
  paramTyList.emplace_back(opcodeTy);
  for (Value *param : paramList) {
    paramTyList.emplace_back(param->getType());
  }

  llvm::FunctionType *funcTy =
      llvm::FunctionType::get(RetType, paramTyList, false);

  Function *opFunc = GetOrCreateHLFunction(M, funcTy, group, opcode);

  SmallVector<Value *, 4> opcodeParamList;
  Value *opcodeConst = Constant::getIntegerValue(opcodeTy, APInt(32, opcode));
  opcodeParamList.emplace_back(opcodeConst);
  opcodeParamList.append(paramList.begin(), paramList.end());

  return Builder.CreateCall(opFunc, opcodeParamList);
}

static Value *EmitHLSLArrayInit(CGBuilderTy &Builder, HLOpcodeGroup group,
                                unsigned opcode, llvm::Type *RetType,
                                ArrayRef<Value *> paramList, llvm::Module &M) {
  // It's a matrix init.
  if (!RetType->isVoidTy())
    return EmitHLSLMatrixOperationCallImp(Builder, group, opcode, RetType,
                                          paramList, M);
  Value *arrayPtr = paramList[0];
  llvm::ArrayType *AT =
      cast<llvm::ArrayType>(arrayPtr->getType()->getPointerElementType());
  // Avoid the arrayPtr.
  unsigned paramSize = paramList.size() - 1;
  // Support simple case here.
  if (paramSize == AT->getArrayNumElements()) {
    bool typeMatch = true;
    llvm::Type *EltTy = AT->getArrayElementType();
    if (EltTy->isAggregateType()) {
      // Aggregate Type use pointer in initList.
      EltTy = llvm::PointerType::get(EltTy, 0);
    }
    for (unsigned i = 1; i < paramList.size(); i++) {
      if (paramList[i]->getType() != EltTy) {
        typeMatch = false;
        break;
      }
    }
    // Both size and type match.
    if (typeMatch) {
      bool isPtr = EltTy->isPointerTy();
      llvm::Type *i32Ty = llvm::Type::getInt32Ty(EltTy->getContext());
      Constant *zero = ConstantInt::get(i32Ty, 0);

      for (unsigned i = 1; i < paramList.size(); i++) {
        Constant *idx = ConstantInt::get(i32Ty, i - 1);
        Value *GEP = Builder.CreateInBoundsGEP(arrayPtr, {zero, idx});
        Value *Elt = paramList[i];

        if (isPtr) {
          Elt = Builder.CreateLoad(Elt);
        }

        Builder.CreateStore(Elt, GEP);
      }
      // The return value will not be used.
      return nullptr;
    }
  }
  // Other case will be lowered in later pass.
  return EmitHLSLMatrixOperationCallImp(Builder, group, opcode, RetType,
                                        paramList, M);
}

void CGMSHLSLRuntime::FlattenValToInitList(CodeGenFunction &CGF, SmallVector<Value *, 4> &elts,
                                      SmallVector<QualType, 4> &eltTys,
                                      QualType Ty, Value *val) {
  CGBuilderTy &Builder = CGF.Builder;
  llvm::Type *valTy = val->getType();

  if (valTy->isPointerTy()) {
    llvm::Type *valEltTy = valTy->getPointerElementType();
    if (valEltTy->isVectorTy() || 
        valEltTy->isSingleValueType()) {
      Value *ldVal = Builder.CreateLoad(val);
      FlattenValToInitList(CGF, elts, eltTys, Ty, ldVal);
    } else if (HLMatrixType::isa(valEltTy)) {
      Value *ldVal = EmitHLSLMatrixLoad(Builder, val, Ty);
      FlattenValToInitList(CGF, elts, eltTys, Ty, ldVal);
    } else {
      llvm::Type *i32Ty = llvm::Type::getInt32Ty(valTy->getContext());
      Value *zero = ConstantInt::get(i32Ty, 0);
      if (llvm::ArrayType *AT = dyn_cast<llvm::ArrayType>(valEltTy)) {
        QualType EltTy = Ty->getAsArrayTypeUnsafe()->getElementType();
        for (unsigned i = 0; i < AT->getArrayNumElements(); i++) {
          Value *gepIdx = ConstantInt::get(i32Ty, i);
          Value *EltPtr = Builder.CreateInBoundsGEP(val, {zero, gepIdx});
          FlattenValToInitList(CGF, elts, eltTys, EltTy,EltPtr);
        }
      } else {
        // Struct.
        StructType *ST = cast<StructType>(valEltTy);
        if (dxilutil::IsHLSLObjectType(ST)) {
          // Save object directly like basic type.
          elts.emplace_back(Builder.CreateLoad(val));
          eltTys.emplace_back(Ty);
        } else {
          RecordDecl *RD = Ty->getAsStructureType()->getDecl();
          const CGRecordLayout& RL = CGF.getTypes().getCGRecordLayout(RD);

          // Take care base.
          if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
            if (CXXRD->getNumBases()) {
              for (const auto &I : CXXRD->bases()) {
                const CXXRecordDecl *BaseDecl = cast<CXXRecordDecl>(
                    I.getType()->castAs<RecordType>()->getDecl());
                if (BaseDecl->field_empty())
                  continue;
                QualType parentTy = QualType(BaseDecl->getTypeForDecl(), 0);
                unsigned i = RL.getNonVirtualBaseLLVMFieldNo(BaseDecl);
                Value *gepIdx = ConstantInt::get(i32Ty, i);
                Value *EltPtr = Builder.CreateInBoundsGEP(val, {zero, gepIdx});
                FlattenValToInitList(CGF, elts, eltTys, parentTy, EltPtr);
              }
            }
          }

          for (auto fieldIter = RD->field_begin(), fieldEnd = RD->field_end();
               fieldIter != fieldEnd; ++fieldIter) {
            unsigned i = RL.getLLVMFieldNo(*fieldIter);
            Value *gepIdx = ConstantInt::get(i32Ty, i);
            Value *EltPtr = Builder.CreateInBoundsGEP(val, {zero, gepIdx});
            FlattenValToInitList(CGF, elts, eltTys, fieldIter->getType(), EltPtr);
          }
        }
      }
    }
  } else {
    if (HLMatrixType MatTy = HLMatrixType::dyn_cast(valTy)) {
      llvm::Type *EltTy = MatTy.getElementTypeForReg();
      // All matrix Value should be row major.
      // Init list is row major in scalar.
      // So the order is match here, just cast to vector.
      unsigned matSize = MatTy.getNumElements();
      bool isRowMajor = hlsl::IsHLSLMatRowMajor(Ty, m_pHLModule->GetHLOptions().bDefaultRowMajor);

      HLCastOpcode opcode = isRowMajor ? HLCastOpcode::RowMatrixToVecCast
                                       : HLCastOpcode::ColMatrixToVecCast;
      // Cast to vector.
      val = EmitHLSLMatrixOperationCallImp(
          Builder, HLOpcodeGroup::HLCast,
          static_cast<unsigned>(opcode),
          llvm::VectorType::get(EltTy, matSize), {val}, TheModule);
      valTy = val->getType();
    }

    if (valTy->isVectorTy()) {
      QualType EltTy = hlsl::GetElementTypeOrType(Ty);
      unsigned vecSize = valTy->getVectorNumElements();
      for (unsigned i = 0; i < vecSize; i++) {
        Value *Elt = Builder.CreateExtractElement(val, i);
        elts.emplace_back(Elt);
        eltTys.emplace_back(EltTy);
      }
    } else {
      DXASSERT(valTy->isSingleValueType(), "must be single value type here");
      elts.emplace_back(val);
      eltTys.emplace_back(Ty);
    }
  }  
}

static Value* ConvertScalarOrVector(CGBuilderTy& Builder, CodeGenTypes &Types,
  Value *Val, QualType SrcQualTy, QualType DstQualTy) {
  llvm::Type *SrcTy = Val->getType();
  llvm::Type *DstTy = Types.ConvertType(DstQualTy);

  DXASSERT(Val->getType() == Types.ConvertType(SrcQualTy), "QualType/Value mismatch!");
  DXASSERT((SrcTy->isIntOrIntVectorTy() || SrcTy->isFPOrFPVectorTy())
    && (DstTy->isIntOrIntVectorTy() || DstTy->isFPOrFPVectorTy()),
    "EmitNumericConversion can only be used with int/float scalars/vectors.");

  if (SrcTy == DstTy) return Val; // Valid no-op, including uint to int / int to uint
  DXASSERT(SrcTy->isVectorTy()
    ? (DstTy->isVectorTy() && SrcTy->getVectorNumElements() == DstTy->getVectorNumElements())
    : !DstTy->isVectorTy(),
    "EmitNumericConversion can only cast between scalars or vectors of matching sizes");

  // Conversions to bools are comparisons
  if (DstTy->getScalarSizeInBits() == 1) {
    // fcmp une is what regular clang uses in C++ for (bool)f;
    return SrcTy->isIntOrIntVectorTy()
      ? Builder.CreateICmpNE(Val, llvm::Constant::getNullValue(SrcTy), "tobool")
      : Builder.CreateFCmpUNE(Val, llvm::Constant::getNullValue(SrcTy), "tobool");
  }

  // Cast necessary
  auto CastOp = static_cast<Instruction::CastOps>(HLModule::GetNumericCastOp(
    SrcTy, hlsl::IsHLSLUnsigned(SrcQualTy), DstTy, hlsl::IsHLSLUnsigned(DstQualTy)));
  return Builder.CreateCast(CastOp, Val, DstTy);
}

static Value* ConvertScalarOrVector(CodeGenFunction &CGF,
  Value *Val, QualType SrcQualTy, QualType DstQualTy) {
  return ConvertScalarOrVector(CGF.Builder, CGF.getTypes(), Val, SrcQualTy, DstQualTy);
}

// Cast elements in initlist if not match the target type.
// idx is current element index in initlist, Ty is target type.

// TODO: Stop handling missing cast here. Handle the casting of non-scalar values
// to their destination type in init list expressions at AST level.
static void AddMissingCastOpsInInitList(SmallVector<Value *, 4> &elts, SmallVector<QualType, 4> &eltTys, unsigned &idx, QualType Ty, CodeGenFunction &CGF) {
  if (Ty->isArrayType()) {
    const clang::ArrayType *AT = Ty->getAsArrayTypeUnsafe();
    // Must be ConstantArrayType here.
    unsigned arraySize = cast<ConstantArrayType>(AT)->getSize().getLimitedValue();
    QualType EltTy = AT->getElementType();
    for (unsigned i = 0; i < arraySize; i++)
      AddMissingCastOpsInInitList(elts, eltTys, idx, EltTy, CGF);
  } else if (IsHLSLVecType(Ty)) {
    QualType EltTy = GetHLSLVecElementType(Ty);
    unsigned vecSize = GetHLSLVecSize(Ty);
    for (unsigned i=0;i< vecSize;i++)
      AddMissingCastOpsInInitList(elts, eltTys, idx, EltTy, CGF);
  } else if (IsHLSLMatType(Ty)) {
    QualType EltTy = GetHLSLMatElementType(Ty);
    unsigned row, col;
    GetHLSLMatRowColCount(Ty, row, col);
    unsigned matSize = row*col;
    for (unsigned i = 0; i < matSize; i++)
      AddMissingCastOpsInInitList(elts, eltTys, idx, EltTy, CGF);
  } else if (Ty->isRecordType()) {
    if (dxilutil::IsHLSLObjectType(CGF.ConvertType(Ty))) {
      // Skip hlsl object.
      idx++;
    } else {
      const RecordType *RT = Ty->getAsStructureType();
      // For CXXRecord.
      if (!RT)
        RT = Ty->getAs<RecordType>();
      RecordDecl *RD = RT->getDecl();
      // Take care base.
      if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
        if (CXXRD->getNumBases()) {
          for (const auto &I : CXXRD->bases()) {
            const CXXRecordDecl *BaseDecl = cast<CXXRecordDecl>(
                I.getType()->castAs<RecordType>()->getDecl());
            if (BaseDecl->field_empty())
              continue;
            QualType parentTy = QualType(BaseDecl->getTypeForDecl(), 0);
            AddMissingCastOpsInInitList(elts, eltTys, idx, parentTy, CGF);
          }
        }
      }
      for (FieldDecl *field : RD->fields())
        AddMissingCastOpsInInitList(elts, eltTys, idx, field->getType(), CGF);
    }
  }
  else {
    // Basic type.
    elts[idx] = ConvertScalarOrVector(CGF, elts[idx], eltTys[idx], Ty);
    idx++;
  }
}

static void StoreInitListToDestPtr(Value *DestPtr,
                                   SmallVector<Value *, 4> &elts, unsigned &idx,
                                   QualType Type, bool bDefaultRowMajor,
                                   CodeGenFunction &CGF, llvm::Module &M) {
  CodeGenTypes &Types = CGF.getTypes();
  CGBuilderTy &Builder = CGF.Builder;

  llvm::Type *Ty = DestPtr->getType()->getPointerElementType();

  if (Ty->isVectorTy()) {
    llvm::Type *RegTy = CGF.ConvertType(Type);
    Value *Result = UndefValue::get(RegTy);
    for (unsigned i = 0; i < RegTy->getVectorNumElements(); i++)
      Result = Builder.CreateInsertElement(Result, elts[idx + i], i);
    Result = CGF.EmitToMemory(Result, Type);
    Builder.CreateStore(Result, DestPtr);
    idx += Ty->getVectorNumElements();
  } else if (HLMatrixType MatTy = HLMatrixType::dyn_cast(Ty)) {
    bool isRowMajor = hlsl::IsHLSLMatRowMajor(Type, bDefaultRowMajor);
    std::vector<Value *> matInitList(MatTy.getNumElements());
    for (unsigned c = 0; c < MatTy.getNumColumns(); c++) {
      for (unsigned r = 0; r < MatTy.getNumRows(); r++) {
        unsigned matIdx = c * MatTy.getNumRows() + r;
        matInitList[matIdx] = elts[idx + matIdx];
      }
    }
    idx += MatTy.getNumElements();
    Value *matVal =
        EmitHLSLMatrixOperationCallImp(Builder, HLOpcodeGroup::HLInit,
                                       /*opcode*/ 0, Ty, matInitList, M);
    // matVal return from HLInit is row major.
    // If DestPtr is row major, just store it directly.
    if (!isRowMajor) {
      // ColMatStore need a col major value.
      // Cast row major matrix into col major.
      // Then store it.
      Value *colMatVal = EmitHLSLMatrixOperationCallImp(
          Builder, HLOpcodeGroup::HLCast,
          static_cast<unsigned>(HLCastOpcode::RowMatrixToColMatrix), Ty,
          {matVal}, M);
      EmitHLSLMatrixOperationCallImp(
          Builder, HLOpcodeGroup::HLMatLoadStore,
          static_cast<unsigned>(HLMatLoadStoreOpcode::ColMatStore), Ty,
          {DestPtr, colMatVal}, M);
    } else {
      EmitHLSLMatrixOperationCallImp(
          Builder, HLOpcodeGroup::HLMatLoadStore,
          static_cast<unsigned>(HLMatLoadStoreOpcode::RowMatStore), Ty,
          {DestPtr, matVal}, M);
    }
  } else if (Ty->isStructTy()) {
    if (dxilutil::IsHLSLObjectType(Ty)) {
      Builder.CreateStore(elts[idx], DestPtr);
      idx++;
    } else {
      Constant *zero = Builder.getInt32(0);

      const RecordType *RT = Type->getAsStructureType();
      // For CXXRecord.
      if (!RT)
        RT = Type->getAs<RecordType>();
      RecordDecl *RD = RT->getDecl();
      const CGRecordLayout &RL = Types.getCGRecordLayout(RD);
      // Take care base.
      if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
        if (CXXRD->getNumBases()) {
          for (const auto &I : CXXRD->bases()) {
            const CXXRecordDecl *BaseDecl = cast<CXXRecordDecl>(
                I.getType()->castAs<RecordType>()->getDecl());
            if (BaseDecl->field_empty())
              continue;
            QualType parentTy = QualType(BaseDecl->getTypeForDecl(), 0);
            unsigned i = RL.getNonVirtualBaseLLVMFieldNo(BaseDecl);
            Constant *gepIdx = Builder.getInt32(i);
            Value *GEP = Builder.CreateInBoundsGEP(DestPtr, {zero, gepIdx});
            StoreInitListToDestPtr(GEP, elts, idx, parentTy,
                                   bDefaultRowMajor, CGF, M);
          }
        }
      }
      for (FieldDecl *field : RD->fields()) {
        unsigned i = RL.getLLVMFieldNo(field);
        Constant *gepIdx = Builder.getInt32(i);
        Value *GEP = Builder.CreateInBoundsGEP(DestPtr, {zero, gepIdx});
        StoreInitListToDestPtr(GEP, elts, idx, field->getType(),
                               bDefaultRowMajor, CGF, M);
      }
    }
  } else if (Ty->isArrayTy()) {
    Constant *zero = Builder.getInt32(0);
    QualType EltType = Type->getAsArrayTypeUnsafe()->getElementType();
    for (unsigned i = 0; i < Ty->getArrayNumElements(); i++) {
      Constant *gepIdx = Builder.getInt32(i);
      Value *GEP = Builder.CreateInBoundsGEP(DestPtr, {zero, gepIdx});
      StoreInitListToDestPtr(GEP, elts, idx, EltType, bDefaultRowMajor,
                             CGF, M);
    }
  } else {
    DXASSERT(Ty->isSingleValueType(), "invalid type");
    llvm::Type *i1Ty = Builder.getInt1Ty();
    Value *V = elts[idx];
    if (V->getType() == i1Ty &&
        DestPtr->getType()->getPointerElementType() != i1Ty) {
      V = Builder.CreateZExt(V, DestPtr->getType()->getPointerElementType());
    }
    Builder.CreateStore(V, DestPtr);
    idx++;
  }
}

void CGMSHLSLRuntime::ScanInitList(CodeGenFunction &CGF, InitListExpr *E,
                                   SmallVector<Value *, 4> &EltValList,
                                   SmallVector<QualType, 4> &EltTyList) {
  unsigned NumInitElements = E->getNumInits();
  for (unsigned i = 0; i != NumInitElements; ++i) {
    Expr *init = E->getInit(i);
    QualType iType = init->getType();
    if (InitListExpr *initList = dyn_cast<InitListExpr>(init)) {
      ScanInitList(CGF, initList, EltValList, EltTyList);
    } else if (CodeGenFunction::hasScalarEvaluationKind(iType)) {
      llvm::Value *initVal = CGF.EmitScalarExpr(init);
      FlattenValToInitList(CGF, EltValList, EltTyList, iType, initVal);
    } else {
      AggValueSlot Slot =
          CGF.CreateAggTemp(init->getType(), "Agg.InitList.tmp");
      CGF.EmitAggExpr(init, Slot);
      llvm::Value *aggPtr = Slot.getAddr();
      FlattenValToInitList(CGF, EltValList, EltTyList, iType, aggPtr);
    }

  }
}
// Is Type of E match Ty.
static bool ExpTypeMatch(Expr *E, QualType Ty, ASTContext &Ctx, CodeGenTypes &Types) {
  if (InitListExpr *initList = dyn_cast<InitListExpr>(E)) {
    unsigned NumInitElements = initList->getNumInits();

    // Skip vector and matrix type.
    if (Ty->isVectorType())
      return false;
    if (hlsl::IsHLSLVecMatType(Ty))
      return false;

    if (Ty->isStructureOrClassType()) {
      RecordDecl *record = Ty->castAs<RecordType>()->getDecl();
      bool bMatch = true;
      unsigned i = 0;
      for (auto it = record->field_begin(), end = record->field_end();
           it != end; it++) {
        if (i == NumInitElements) {
          bMatch = false;
          break;
        }
        Expr *init = initList->getInit(i++);
        QualType EltTy = it->getType();
        bMatch &= ExpTypeMatch(init, EltTy, Ctx, Types);
        if (!bMatch)
          break;
      }
      bMatch &= i == NumInitElements;
      if (bMatch && initList->getType()->isVoidType()) {
        initList->setType(Ty);
      }
      return bMatch;
    } else if (Ty->isArrayType() && !Ty->isIncompleteArrayType()) {
      const ConstantArrayType *AT = Ctx.getAsConstantArrayType(Ty);
      QualType EltTy = AT->getElementType();
      unsigned size = AT->getSize().getZExtValue();

      if (size != NumInitElements)
        return false;

      bool bMatch = true;
      for (unsigned i = 0; i != NumInitElements; ++i) {
        Expr *init = initList->getInit(i);
        bMatch &= ExpTypeMatch(init, EltTy, Ctx, Types);
        if (!bMatch)
          break;
      }
      if (bMatch && initList->getType()->isVoidType()) {
        initList->setType(Ty);
      }
      return bMatch;
    } else {
      return false;
    }
  } else {
    llvm::Type *ExpTy = Types.ConvertType(E->getType());
    llvm::Type *TargetTy = Types.ConvertType(Ty);
    return ExpTy == TargetTy;
  }
}

bool CGMSHLSLRuntime::IsTrivalInitListExpr(CodeGenFunction &CGF,
                                           InitListExpr *E) {
  QualType Ty = E->getType();
  bool result = ExpTypeMatch(E, Ty, CGF.getContext(), CGF.getTypes());
  if (result) {
    auto iter = staticConstGlobalInitMap.find(E);
    if (iter != staticConstGlobalInitMap.end()) {
      GlobalVariable * GV = iter->second;
      auto &InitConstants = staticConstGlobalInitListMap[GV];
      // Add Constant to InitList.
      for (unsigned i=0;i<E->getNumInits();i++) {
        Expr *Expr = E->getInit(i);
        if (ImplicitCastExpr *Cast = dyn_cast<ImplicitCastExpr>(Expr)) {
          if (Cast->getCastKind() == CK_LValueToRValue) {
            Expr = Cast->getSubExpr();
          }
        }
        // Only do this on lvalue, if not lvalue, it will not be constant
        // anyway.
        if (Expr->isLValue()) {
          LValue LV = CGF.EmitLValue(Expr);
          if (LV.isSimple()) {
            Constant *SrcPtr = dyn_cast<Constant>(LV.getAddress());
            if (SrcPtr && !isa<UndefValue>(SrcPtr)) {
              InitConstants.emplace_back(SrcPtr);
             continue;
            }
          }
        }

        // Only support simple LV and Constant Ptr case.
        // Other case just go normal path.
        InitConstants.clear();
        break;
      }
      if (InitConstants.empty())
        staticConstGlobalInitListMap.erase(GV);
      else
        staticConstGlobalCtorMap[GV] = CGF.CurFn;
    }
  }
  return result;
}

Value *CGMSHLSLRuntime::EmitHLSLInitListExpr(CodeGenFunction &CGF, InitListExpr *E,
      // The destPtr when emiting aggregate init, for normal case, it will be null.
      Value *DestPtr) {
  if (DestPtr && E->getNumInits() == 1) {
    llvm::Type *ExpTy = CGF.ConvertType(E->getType());
    llvm::Type *TargetTy = CGF.ConvertType(E->getInit(0)->getType());
    if (ExpTy == TargetTy) {
      Expr *Expr = E->getInit(0);
      LValue LV = CGF.EmitLValue(Expr);
      if (LV.isSimple()) {
        Value *SrcPtr = LV.getAddress();
        SmallVector<Value *, 4> idxList;
        EmitHLSLAggregateCopy(CGF, SrcPtr, DestPtr, idxList, Expr->getType(),
                              E->getType(), SrcPtr->getType());
        return nullptr;
      }
    }
  }

  SmallVector<Value *, 4> EltValList;
  SmallVector<QualType, 4> EltTyList;
  
  ScanInitList(CGF, E, EltValList, EltTyList);
  
  QualType ResultTy = E->getType();
  unsigned idx = 0;
  // Create cast if need.
  AddMissingCastOpsInInitList(EltValList, EltTyList, idx, ResultTy, CGF);
  DXASSERT(idx == EltValList.size(), "size must match");

  llvm::Type *RetTy = CGF.ConvertType(ResultTy);
  if (DestPtr) {
    SmallVector<Value *, 4> ParamList;
    DXASSERT_NOMSG(RetTy->isAggregateType());
    ParamList.emplace_back(DestPtr);
    ParamList.append(EltValList.begin(), EltValList.end());
    idx = 0;
    bool bDefaultRowMajor = m_pHLModule->GetHLOptions().bDefaultRowMajor;
    StoreInitListToDestPtr(DestPtr, EltValList, idx, ResultTy,
                           bDefaultRowMajor, CGF, TheModule);
    return nullptr;
  }

  if (IsHLSLVecType(ResultTy)) {
    Value *Result = UndefValue::get(RetTy);
    for (unsigned i = 0; i < RetTy->getVectorNumElements(); i++)
      Result = CGF.Builder.CreateInsertElement(Result, EltValList[i], i);
    return Result;
  } else {
    // Must be matrix here.
    DXASSERT(IsHLSLMatType(ResultTy), "must be matrix type here.");
    return EmitHLSLMatrixOperationCallImp(CGF.Builder, HLOpcodeGroup::HLInit,
                                          /*opcode*/ 0, RetTy, EltValList,
                                          TheModule);
  }
}

static void FlatConstToList(CodeGenTypes &Types, bool bDefaultRowMajor,
    Constant *C, QualType QualTy,
    SmallVectorImpl<Constant *> &EltVals, SmallVectorImpl<QualType> &EltQualTys) {
  llvm::Type *Ty = C->getType();
  DXASSERT(Types.ConvertTypeForMem(QualTy) == Ty, "QualType/Type mismatch!");

  if (llvm::VectorType *VecTy = dyn_cast<llvm::VectorType>(Ty)) {
    DXASSERT(hlsl::IsHLSLVecType(QualTy), "QualType/Type mismatch!");
    QualType VecElemQualTy = hlsl::GetHLSLVecElementType(QualTy);
    for (unsigned i = 0; i < VecTy->getNumElements(); i++) {
      EltVals.emplace_back(C->getAggregateElement(i));
      EltQualTys.emplace_back(VecElemQualTy);
    }
  } else if (HLMatrixType::isa(Ty)) {
    DXASSERT(hlsl::IsHLSLMatType(QualTy), "QualType/Type mismatch!");
    // matrix type is struct { [rowcount x <colcount x T>] };
    // Strip the struct level here.
    Constant *RowArrayVal = C->getAggregateElement((unsigned)0);
    QualType MatEltQualTy = hlsl::GetHLSLMatElementType(QualTy);

    unsigned RowCount, ColCount;
    hlsl::GetHLSLMatRowColCount(QualTy, RowCount, ColCount);

    // Get all the elements from the array of row vectors.
    // Matrices are never in memory representation so convert as needed.
    SmallVector<Constant *, 16> MatElts;
    for (unsigned r = 0; r < RowCount; ++r) {
      Constant *RowVec = RowArrayVal->getAggregateElement(r);
      for (unsigned c = 0; c < ColCount; ++c) {
        Constant *MatElt = RowVec->getAggregateElement(c);
        if (MatEltQualTy->isBooleanType()) {
          DXASSERT(MatElt->getType()->isIntegerTy(1),
            "Matrix elements should be in their register representation.");
          MatElt = llvm::ConstantExpr::getZExt(MatElt, Types.ConvertTypeForMem(MatEltQualTy));
        }
        MatElts.emplace_back(MatElt);
      }
    }

    // Return the elements in the order respecting the orientation.
    // Constant initializers are used as the initial value for static variables,
    // which live in memory. This is why they have to respect memory packing order.
    bool IsRowMajor = hlsl::IsHLSLMatRowMajor(QualTy, bDefaultRowMajor);
    for (unsigned r = 0; r < RowCount; ++r) {
      for (unsigned c = 0; c < ColCount; ++c) {
        unsigned Idx = IsRowMajor ? (r * ColCount + c) : (c * RowCount + r);
        EltVals.emplace_back(MatElts[Idx]);
        EltQualTys.emplace_back(MatEltQualTy);
      }
    }
  }
  else if (const clang::ConstantArrayType *ClangArrayTy = Types.getContext().getAsConstantArrayType(QualTy)) {
    QualType ArrayEltQualTy = ClangArrayTy->getElementType();
    uint64_t ArraySize = ClangArrayTy->getSize().getLimitedValue();
    DXASSERT(cast<llvm::ArrayType>(Ty)->getArrayNumElements() == ArraySize, "QualType/Type mismatch!");
    for (unsigned i = 0; i < ArraySize; i++) {
      FlatConstToList(Types, bDefaultRowMajor, C->getAggregateElement(i), ArrayEltQualTy, 
        EltVals, EltQualTys);
    }
  }
  else if (const clang::RecordType* RecordTy = QualTy->getAs<clang::RecordType>()) {
    DXASSERT(dyn_cast<llvm::StructType>(Ty) != nullptr, "QualType/Type mismatch!");
    RecordDecl *RecordDecl = RecordTy->getDecl();
    const CGRecordLayout &RL = Types.getCGRecordLayout(RecordDecl);
    // Take care base.
    if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RecordDecl)) {
      if (CXXRD->getNumBases()) {
        for (const auto &I : CXXRD->bases()) {
          const CXXRecordDecl *BaseDecl =
              cast<CXXRecordDecl>(I.getType()->castAs<RecordType>()->getDecl());
          if (BaseDecl->field_empty())
            continue;
          QualType BaseQualTy = QualType(BaseDecl->getTypeForDecl(), 0);
          unsigned BaseFieldIdx = RL.getNonVirtualBaseLLVMFieldNo(BaseDecl);
          FlatConstToList(Types, bDefaultRowMajor,
            C->getAggregateElement(BaseFieldIdx), BaseQualTy, EltVals, EltQualTys);
        }
      }
    }

    for (auto FieldIt = RecordDecl->field_begin(), fieldEnd = RecordDecl->field_end();
      FieldIt != fieldEnd; ++FieldIt) {
      unsigned FieldIndex = RL.getLLVMFieldNo(*FieldIt);

      FlatConstToList(Types, bDefaultRowMajor,
        C->getAggregateElement(FieldIndex), FieldIt->getType(), EltVals, EltQualTys);
    }
  }
  else {
    // At this point, we should have scalars in their memory representation
    DXASSERT_NOMSG(QualTy->isBuiltinType());
    EltVals.emplace_back(C);
    EltQualTys.emplace_back(QualTy);
  }
}

static bool ScanConstInitList(CodeGenModule &CGM, bool bDefaultRowMajor, 
                              InitListExpr *InitList,
                              SmallVectorImpl<Constant *> &EltVals,
                              SmallVectorImpl<QualType> &EltQualTys) {
  unsigned NumInitElements = InitList->getNumInits();
  for (unsigned i = 0; i != NumInitElements; ++i) {
    Expr *InitExpr = InitList->getInit(i);
    QualType InitQualTy = InitExpr->getType();
    if (InitListExpr *SubInitList = dyn_cast<InitListExpr>(InitExpr)) {
      if (!ScanConstInitList(CGM, bDefaultRowMajor, SubInitList, EltVals, EltQualTys))
        return false;
    } else if (DeclRefExpr *DeclRef = dyn_cast<DeclRefExpr>(InitExpr)) {
      if (VarDecl *Var = dyn_cast<VarDecl>(DeclRef->getDecl())) {
        if (!Var->hasInit())
          return false;
        if (Constant *InitVal = CGM.EmitConstantInit(*Var)) {
          FlatConstToList(CGM.getTypes(), bDefaultRowMajor,
            InitVal, InitQualTy, EltVals, EltQualTys);
        } else {
          return false;
        }
      } else {
        return false;
      }
    } else if (hlsl::IsHLSLMatType(InitQualTy)) {
      return false;
    } else if (CodeGenFunction::hasScalarEvaluationKind(InitQualTy)) {
      if (Constant *InitVal = CGM.EmitConstantExpr(InitExpr, InitQualTy)) {
        FlatConstToList(CGM.getTypes(), bDefaultRowMajor, InitVal, InitQualTy, EltVals, EltQualTys);
      } else {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

static Constant *BuildConstInitializer(CodeGenTypes &Types, bool bDefaultRowMajor,
  QualType QualTy, bool MemRepr,
  SmallVectorImpl<Constant *> &EltVals, SmallVectorImpl<QualType> &EltQualTys, unsigned &EltIdx);

static Constant *BuildConstMatrix(CodeGenTypes &Types, bool bDefaultRowMajor, QualType QualTy,
    SmallVectorImpl<Constant *> &EltVals, SmallVectorImpl<QualType> &EltQualTys, unsigned &EltIdx) {
  QualType MatEltTy = hlsl::GetHLSLMatElementType(QualTy);
  unsigned RowCount, ColCount;
  hlsl::GetHLSLMatRowColCount(QualTy, RowCount, ColCount);
  bool IsRowMajor = hlsl::IsHLSLMatRowMajor(QualTy, bDefaultRowMajor);

  // Save initializer elements first.
  // Matrix initializer is row major.
  SmallVector<Constant *, 16> RowMajorMatElts;
  for (unsigned i = 0; i < RowCount * ColCount; i++) {
    // Matrix elements are never in their memory representation,
    // to preserve type information for later lowering.
    bool MemRepr = false; 
    RowMajorMatElts.emplace_back(BuildConstInitializer(
      Types, bDefaultRowMajor, MatEltTy, MemRepr,
      EltVals, EltQualTys, EltIdx));
  }

  SmallVector<Constant *, 16> FinalMatElts;
  if (IsRowMajor) {
    FinalMatElts = RowMajorMatElts;
  }
  else {
    // Cast row major to col major.
    for (unsigned c = 0; c < ColCount; c++) {
      for (unsigned r = 0; r < RowCount; r++) {
        FinalMatElts.emplace_back(RowMajorMatElts[r * ColCount + c]);
      }
    }
  }
  // The type is vector<element, col>[row].
  SmallVector<Constant *, 4> Rows;
  unsigned idx = 0;
  for (unsigned r = 0; r < RowCount; r++) {
    SmallVector<Constant *, 4> RowElts;
    for (unsigned c = 0; c < ColCount; c++) {
      RowElts.emplace_back(FinalMatElts[idx++]);
    }
    Rows.emplace_back(llvm::ConstantVector::get(RowElts));
  }

  Constant *RowArray = llvm::ConstantArray::get(
    llvm::ArrayType::get(Rows[0]->getType(), Rows.size()), Rows);
  return llvm::ConstantStruct::get(cast<llvm::StructType>(Types.ConvertType(QualTy)), RowArray);
}

static Constant *BuildConstStruct(CodeGenTypes &Types, bool bDefaultRowMajor, QualType QualTy,
    SmallVectorImpl<Constant *> &EltVals, SmallVectorImpl<QualType> &EltQualTys, unsigned &EltIdx) {
  const RecordDecl *Record = QualTy->castAs<RecordType>()->getDecl();
  bool MemRepr = true; // Structs are always in their memory representation
  SmallVector<Constant *, 4> FieldVals;
  if (const CXXRecordDecl *CXXRecord = dyn_cast<CXXRecordDecl>(Record)) {
    if (CXXRecord->getNumBases()) {
      // Add base as field.
      for (const auto &BaseSpec : CXXRecord->bases()) {
        const CXXRecordDecl *BaseDecl =
            cast<CXXRecordDecl>(BaseSpec.getType()->castAs<RecordType>()->getDecl());
        // Skip empty struct.
        if (BaseDecl->field_empty())
          continue;

        // Add base as a whole constant. Not as element.
        FieldVals.emplace_back(BuildConstInitializer(Types, bDefaultRowMajor,
          BaseSpec.getType(), MemRepr, EltVals, EltQualTys, EltIdx));
      }
    }
  }

  for (auto FieldIt = Record->field_begin(), FieldEnd = Record->field_end();
      FieldIt != FieldEnd; ++FieldIt) {
    FieldVals.emplace_back(BuildConstInitializer(Types, bDefaultRowMajor,
      FieldIt->getType(), MemRepr, EltVals, EltQualTys, EltIdx));
  }

  return llvm::ConstantStruct::get(cast<llvm::StructType>(Types.ConvertTypeForMem(QualTy)), FieldVals);
}

static Constant *BuildConstInitializer(CodeGenTypes &Types, bool bDefaultRowMajor,
    QualType QualTy, bool MemRepr,
    SmallVectorImpl<Constant *> &EltVals, SmallVectorImpl<QualType> &EltQualTys, unsigned &EltIdx) {
  if (hlsl::IsHLSLVecType(QualTy)) {
    QualType VecEltQualTy = hlsl::GetHLSLVecElementType(QualTy);
    unsigned VecSize = hlsl::GetHLSLVecSize(QualTy);
    SmallVector<Constant *, 4> VecElts;
    for (unsigned i = 0; i < VecSize; i++) {
      VecElts.emplace_back(BuildConstInitializer(Types, bDefaultRowMajor,
        VecEltQualTy, MemRepr,
        EltVals, EltQualTys, EltIdx));
    }
    return llvm::ConstantVector::get(VecElts);
  }
  else if (const clang::ConstantArrayType *ArrayTy = Types.getContext().getAsConstantArrayType(QualTy)) {
    QualType ArrayEltQualTy = QualType(ArrayTy->getArrayElementTypeNoTypeQual(), 0);
    uint64_t ArraySize = ArrayTy->getSize().getLimitedValue();
    SmallVector<Constant *, 4> ArrayElts;
    for (unsigned i = 0; i < ArraySize; i++) {
      ArrayElts.emplace_back(BuildConstInitializer(Types, bDefaultRowMajor,
        ArrayEltQualTy, true, // Array elements must be in their memory representation
        EltVals, EltQualTys, EltIdx));
    }
    return llvm::ConstantArray::get(
      cast<llvm::ArrayType>(Types.ConvertTypeForMem(QualTy)), ArrayElts);
  }
  else if (hlsl::IsHLSLMatType(QualTy)) {
    return BuildConstMatrix(Types, bDefaultRowMajor, QualTy,
      EltVals, EltQualTys, EltIdx);
  }
  else if (QualTy->getAs<clang::RecordType>() != nullptr) {
    return BuildConstStruct(Types, bDefaultRowMajor, QualTy,
      EltVals, EltQualTys, EltIdx);
  } else {
    DXASSERT_NOMSG(QualTy->isBuiltinType());
    Constant *EltVal = EltVals[EltIdx];
    QualType EltQualTy = EltQualTys[EltIdx];
    EltIdx++;

    // Initializer constants are in their memory representation.
    if (EltQualTy == QualTy && MemRepr) return EltVal;

    CGBuilderTy Builder(EltVal->getContext());
    if (EltQualTy->isBooleanType()) {
      // Convert to register representation
      // We don't have access to CodeGenFunction::EmitFromMemory here
      DXASSERT_NOMSG(!EltVal->getType()->isIntegerTy(1));
      EltVal = cast<Constant>(Builder.CreateICmpNE(EltVal, Constant::getNullValue(EltVal->getType())));
    }

    Constant *Result = cast<Constant>(ConvertScalarOrVector(Builder, Types, EltVal, EltQualTy, QualTy));

    if (QualTy->isBooleanType() && MemRepr) {
      // Convert back to the memory representation
      // We don't have access to CodeGenFunction::EmitToMemory here
      DXASSERT_NOMSG(Result->getType()->isIntegerTy(1));
      Result = cast<Constant>(Builder.CreateZExt(Result, Types.ConvertTypeForMem(QualTy)));
    }

    return Result;
  }
}

Constant *CGMSHLSLRuntime::EmitHLSLConstInitListExpr(CodeGenModule &CGM,
                                                     InitListExpr *E) {
  bool bDefaultRowMajor = m_pHLModule->GetHLOptions().bDefaultRowMajor;
  SmallVector<Constant *, 4> EltVals;
  SmallVector<QualType, 4> EltQualTys;
  if (!ScanConstInitList(CGM, bDefaultRowMajor, E, EltVals, EltQualTys))
    return nullptr;

  QualType QualTy = E->getType();
  unsigned EltIdx = 0;
  bool MemRepr = true;
  return BuildConstInitializer(CGM.getTypes(), bDefaultRowMajor,
    QualTy, MemRepr, EltVals, EltQualTys, EltIdx);
}

Value *CGMSHLSLRuntime::EmitHLSLMatrixOperationCall(
    CodeGenFunction &CGF, const clang::Expr *E, llvm::Type *RetType,
    ArrayRef<Value *> paramList) {
  HLOpcodeGroup group = GetHLOpcodeGroup(E->getStmtClass());
  unsigned opcode = GetHLOpcode(E);
  if (group == HLOpcodeGroup::HLInit)
    return EmitHLSLArrayInit(CGF.Builder, group, opcode, RetType, paramList,
                             TheModule);
  else
    return EmitHLSLMatrixOperationCallImp(CGF.Builder, group, opcode, RetType,
                                          paramList, TheModule);
}

void CGMSHLSLRuntime::EmitHLSLDiscard(CodeGenFunction &CGF) {
  EmitHLSLMatrixOperationCallImp(
      CGF.Builder, HLOpcodeGroup::HLIntrinsic,
      static_cast<unsigned>(IntrinsicOp::IOP_clip),
      llvm::Type::getVoidTy(CGF.getLLVMContext()),
      {ConstantFP::get(llvm::Type::getFloatTy(CGF.getLLVMContext()), -1.0f)},
      TheModule);
}

// Emit an artificially conditionalized branch for a break operation when in a potentially wave-enabled stage
// This allows the block containing what would have been an unconditional break to be included in the loop
// If the block uses values that are wave-sensitive, it needs to stay in the loop to prevent optimizations
// that might produce incorrect results by ignoring the volatile aspect of wave operation results.
void CGMSHLSLRuntime::EmitHLSLCondBreak(CodeGenFunction &CGF, Function *F, BasicBlock *DestBB, BasicBlock *AltBB) {
  // If not a wave-enabled stage, we can keep everything unconditional as before
  if (!m_pHLModule->GetShaderModel()->IsPS() && !m_pHLModule->GetShaderModel()->IsCS() &&
      !m_pHLModule->GetShaderModel()->IsLib()) {
    CGF.Builder.CreateBr(DestBB);
    return;
  }

  // Create a branch that is temporarily conditional on a constant
  // FinalizeCodeGen will turn this into a function, DxilFinalize will turn it into a global var
  llvm::Type *boolTy = llvm::Type::getInt1Ty(Context);
  BranchInst *BI = CGF.Builder.CreateCondBr(llvm::ConstantInt::get(boolTy,1), DestBB, AltBB);
  m_DxBreaks.emplace_back(BI);
}

static llvm::Type *MergeIntType(llvm::IntegerType *T0, llvm::IntegerType *T1) {
  if (T0->getBitWidth() > T1->getBitWidth())
    return T0;
  else
    return T1;
}

static Value *CreateExt(CGBuilderTy &Builder, Value *Src, llvm::Type *DstTy,
                        bool bSigned) {
  if (bSigned)
    return Builder.CreateSExt(Src, DstTy);
  else
    return Builder.CreateZExt(Src, DstTy);
}
// For integer literal, try to get lowest precision.
static Value *CalcHLSLLiteralToLowestPrecision(CGBuilderTy &Builder, Value *Src,
                                               bool bSigned) {
  if (ConstantInt *CI = dyn_cast<ConstantInt>(Src)) {
    APInt v = CI->getValue();
    switch (v.getActiveWords()) {
    case 4:
      return Builder.getInt32(v.getLimitedValue());
    case 8:
      return Builder.getInt64(v.getLimitedValue());
    case 2:
      // TODO: use low precision type when support it in dxil.
      // return Builder.getInt16(v.getLimitedValue());
      return Builder.getInt32(v.getLimitedValue());
    case 1:
      // TODO: use precision type when support it in dxil.
      // return Builder.getInt8(v.getLimitedValue());
      return Builder.getInt32(v.getLimitedValue());
    default:
      return nullptr;
    }
  } else if (SelectInst *SI = dyn_cast<SelectInst>(Src)) {
    if (SI->getType()->isIntegerTy()) {
      Value *T = SI->getTrueValue();
      Value *F = SI->getFalseValue();
      Value *lowT = CalcHLSLLiteralToLowestPrecision(Builder, T, bSigned);
      Value *lowF = CalcHLSLLiteralToLowestPrecision(Builder, F, bSigned);
      if (lowT && lowF && lowT != T && lowF != F) {
        llvm::IntegerType *TTy = cast<llvm::IntegerType>(lowT->getType());
        llvm::IntegerType *FTy = cast<llvm::IntegerType>(lowF->getType());
        llvm::Type *Ty = MergeIntType(TTy, FTy);
        if (TTy != Ty) {
          lowT = CreateExt(Builder, lowT, Ty, bSigned);
        }
        if (FTy != Ty) {
          lowF = CreateExt(Builder, lowF, Ty, bSigned);
        }
        Value *Cond = SI->getCondition();
        return Builder.CreateSelect(Cond, lowT, lowF);
      }
    }
  } else if (llvm::BinaryOperator *BO = dyn_cast<llvm::BinaryOperator>(Src)) {
    Value *Src0 = BO->getOperand(0);
    Value *Src1 = BO->getOperand(1);
    Value *CastSrc0 = CalcHLSLLiteralToLowestPrecision(Builder, Src0, bSigned);
    Value *CastSrc1 = CalcHLSLLiteralToLowestPrecision(Builder, Src1, bSigned);
    if (Src0 != CastSrc0 && Src1 != CastSrc1 && CastSrc0 && CastSrc1 &&
        CastSrc0->getType() == CastSrc1->getType()) {
      llvm::IntegerType *Ty0 = cast<llvm::IntegerType>(CastSrc0->getType());
      llvm::IntegerType *Ty1 = cast<llvm::IntegerType>(CastSrc0->getType());
      llvm::Type *Ty = MergeIntType(Ty0, Ty1);
      if (Ty0 != Ty) {
        CastSrc0 = CreateExt(Builder, CastSrc0, Ty, bSigned);
      }
      if (Ty1 != Ty) {
        CastSrc1 = CreateExt(Builder, CastSrc1, Ty, bSigned);
      }
      return Builder.CreateBinOp(BO->getOpcode(), CastSrc0, CastSrc1);
    }
  }
  return nullptr;
}

Value *CGMSHLSLRuntime::EmitHLSLLiteralCast(CodeGenFunction &CGF, Value *Src,
                                            QualType SrcType,
                                            QualType DstType) {
  auto &Builder = CGF.Builder;
  llvm::Type *DstTy = CGF.ConvertType(DstType);
  bool bSrcSigned = SrcType->isSignedIntegerType();

  if (ConstantInt *CI = dyn_cast<ConstantInt>(Src)) {
    APInt v = CI->getValue();
    if (llvm::IntegerType *IT = dyn_cast<llvm::IntegerType>(DstTy)) {
      v = v.trunc(IT->getBitWidth());
      switch (IT->getBitWidth()) {
      case 32:
        return Builder.getInt32(v.getLimitedValue());
      case 64:
        return Builder.getInt64(v.getLimitedValue());
      case 16:
        return Builder.getInt16(v.getLimitedValue());
      case 8:
        return Builder.getInt8(v.getLimitedValue());
      default:
        return nullptr;
      }
    } else {
      DXASSERT_NOMSG(DstTy->isFloatingPointTy());
      int64_t val = v.getLimitedValue();
      if (v.isNegative())
        val = 0-v.abs().getLimitedValue();
      if (DstTy->isDoubleTy())
        return ConstantFP::get(DstTy, (double)val);
      else if (DstTy->isFloatTy())
        return ConstantFP::get(DstTy, (float)val);
      else {
        if (bSrcSigned)
          return Builder.CreateSIToFP(Src, DstTy);
        else
          return Builder.CreateUIToFP(Src, DstTy);
      }
    }
  } else if (ConstantFP *CF = dyn_cast<ConstantFP>(Src)) {
    APFloat v = CF->getValueAPF();
    double dv = v.convertToDouble();
    if (llvm::IntegerType *IT = dyn_cast<llvm::IntegerType>(DstTy)) {
      switch (IT->getBitWidth()) {
      case 32:
        return Builder.getInt32(dv);
      case 64:
        return Builder.getInt64(dv);
      case 16:
        return Builder.getInt16(dv);
      case 8:
        return Builder.getInt8(dv);
      default:
        return nullptr;
      }
    } else {
      if (DstTy->isFloatTy()) {
        float fv = dv;
        return ConstantFP::get(DstTy->getContext(), APFloat(fv));
      } else {
        return Builder.CreateFPTrunc(Src, DstTy);
      }
    }
  } else if (dyn_cast<UndefValue>(Src)) {
    return UndefValue::get(DstTy);
  } else {
    Instruction *I = cast<Instruction>(Src);
    if (SelectInst *SI = dyn_cast<SelectInst>(I)) {
      Value *T = SI->getTrueValue();
      Value *F = SI->getFalseValue();
      Value *Cond = SI->getCondition();
      if (isa<llvm::ConstantInt>(T) && isa<llvm::ConstantInt>(F)) {
        llvm::APInt lhs = cast<llvm::ConstantInt>(T)->getValue();
        llvm::APInt rhs = cast<llvm::ConstantInt>(F)->getValue();
        if (DstTy == Builder.getInt32Ty()) {
          T = Builder.getInt32(lhs.getLimitedValue());
          F = Builder.getInt32(rhs.getLimitedValue());
          Value *Sel = Builder.CreateSelect(Cond, T, F, "cond");
          return Sel;
        } else if (DstTy->isFloatingPointTy()) {
          T = ConstantFP::get(DstTy, int64_t(lhs.getLimitedValue()));
          F = ConstantFP::get(DstTy, int64_t(rhs.getLimitedValue()));
          Value *Sel = Builder.CreateSelect(Cond, T, F, "cond");
          return Sel;
        }
      } else if (isa<llvm::ConstantFP>(T) && isa<llvm::ConstantFP>(F)) {
        llvm::APFloat lhs = cast<llvm::ConstantFP>(T)->getValueAPF();
        llvm::APFloat rhs = cast<llvm::ConstantFP>(F)->getValueAPF();
        double ld = lhs.convertToDouble();
        double rd = rhs.convertToDouble();
        if (DstTy->isFloatTy()) {
          float lf = ld;
          float rf = rd;
          T = ConstantFP::get(DstTy->getContext(), APFloat(lf));
          F = ConstantFP::get(DstTy->getContext(), APFloat(rf));
          Value *Sel = Builder.CreateSelect(Cond, T, F, "cond");
          return Sel;
        } else if (DstTy == Builder.getInt32Ty()) {
          T = Builder.getInt32(ld);
          F = Builder.getInt32(rd);
          Value *Sel = Builder.CreateSelect(Cond, T, F, "cond");
          return Sel;
        } else if (DstTy == Builder.getInt64Ty()) {
          T = Builder.getInt64(ld);
          F = Builder.getInt64(rd);
          Value *Sel = Builder.CreateSelect(Cond, T, F, "cond");
          return Sel;
        }
      }
    } else if (llvm::BinaryOperator *BO = dyn_cast<llvm::BinaryOperator>(I)) {
      // For integer binary operator, do the calc on lowest precision, then cast
      // to dstTy.
      if (I->getType()->isIntegerTy()) {
        bool bSigned = DstType->isSignedIntegerType();
        Value *CastResult =
            CalcHLSLLiteralToLowestPrecision(Builder, BO, bSigned);
        if (!CastResult)
          return nullptr;
        if (dyn_cast<llvm::IntegerType>(DstTy)) {
          if (DstTy == CastResult->getType()) {
            return CastResult;
          } else {
            if (bSigned)
              return Builder.CreateSExtOrTrunc(CastResult, DstTy);
            else
              return Builder.CreateZExtOrTrunc(CastResult, DstTy);
          }
        } else {
          if (bSrcSigned)
            return Builder.CreateSIToFP(CastResult, DstTy);
          else
            return Builder.CreateUIToFP(CastResult, DstTy);
        }
      }
    }
    // TODO: support other opcode if need.
    return nullptr;
  }
}

// For case like ((float3xfloat3)mat4x4).m21 or ((float3xfloat3)mat4x4)[1], just
// treat it like mat4x4.m21 or mat4x4[1].
static Value *GetOriginMatrixOperandAndUpdateMatSize(Value *Ptr, unsigned &row,
                                                     unsigned &col) {
  if (CallInst *Mat = dyn_cast<CallInst>(Ptr)) {
    HLOpcodeGroup OpcodeGroup =
        GetHLOpcodeGroupByName(Mat->getCalledFunction());
    if (OpcodeGroup == HLOpcodeGroup::HLCast) {
      HLCastOpcode castOpcode = static_cast<HLCastOpcode>(GetHLOpcode(Mat));
      if (castOpcode == HLCastOpcode::DefaultCast) {
        Ptr = Mat->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
        // Remove the cast which is useless now.
        Mat->eraseFromParent();
        // Update row and col.
        HLMatrixType matTy =
            HLMatrixType::cast(Ptr->getType()->getPointerElementType());
        row = matTy.getNumRows();
        col = matTy.getNumColumns();
        // Don't update RetTy and DxilGeneration pass will do the right thing.
        return Ptr;
      }
    }
  }
  return nullptr;
}


Value *CGMSHLSLRuntime::EmitHLSLMatrixSubscript(CodeGenFunction &CGF,
                                                llvm::Type *RetType,
                                                llvm::Value *Ptr,
                                                llvm::Value *Idx,
                                                clang::QualType Ty) {
  bool isRowMajor =
      hlsl::IsHLSLMatRowMajor(Ty, m_pHLModule->GetHLOptions().bDefaultRowMajor);
  unsigned opcode =
      isRowMajor ? static_cast<unsigned>(HLSubscriptOpcode::RowMatSubscript)
                 : static_cast<unsigned>(HLSubscriptOpcode::ColMatSubscript);
  Value *matBase = Ptr;
  DXASSERT(matBase->getType()->isPointerTy(),
           "matrix subscript should return pointer");

  RetType =
      llvm::PointerType::get(RetType->getPointerElementType(),
                             matBase->getType()->getPointerAddressSpace());

  unsigned row, col;
  hlsl::GetHLSLMatRowColCount(Ty, row, col);
  unsigned resultCol = col;
  if (Value *OriginPtr = GetOriginMatrixOperandAndUpdateMatSize(Ptr, row, col)) {
    Ptr = OriginPtr;
    // Update col to result col to get correct result size.
    col = resultCol;
  }

  // Lower mat[Idx] into real idx.
  SmallVector<Value *, 8> args;
  args.emplace_back(Ptr);

  if (isRowMajor) {
    Value *cCol = ConstantInt::get(Idx->getType(), col);
    Value *Base = CGF.Builder.CreateMul(cCol, Idx);
    for (unsigned i = 0; i < col; i++) {
      Value *c = ConstantInt::get(Idx->getType(), i);
      // r * col + c
      Value *matIdx = CGF.Builder.CreateAdd(Base, c);
      args.emplace_back(matIdx);
    }
  } else {
    for (unsigned i = 0; i < col; i++) {
      Value *cMulRow = ConstantInt::get(Idx->getType(), i * row);
      // c * row + r
      Value *matIdx = CGF.Builder.CreateAdd(cMulRow, Idx);
      args.emplace_back(matIdx);
    }
  }

  Value *matSub =
      EmitHLSLMatrixOperationCallImp(CGF.Builder, HLOpcodeGroup::HLSubscript,
                                     opcode, RetType, args, TheModule);
  return matSub;
}

Value *CGMSHLSLRuntime::EmitHLSLMatrixElement(CodeGenFunction &CGF,
                                              llvm::Type *RetType,
                                              ArrayRef<Value *> paramList,
                                              QualType Ty) {
  bool isRowMajor =
    hlsl::IsHLSLMatRowMajor(Ty, m_pHLModule->GetHLOptions().bDefaultRowMajor);
  unsigned opcode =
      isRowMajor ? static_cast<unsigned>(HLSubscriptOpcode::RowMatElement)
                 : static_cast<unsigned>(HLSubscriptOpcode::ColMatElement);

  Value *matBase = paramList[0];
  DXASSERT(matBase->getType()->isPointerTy(),
           "matrix element should return pointer");

  RetType =
      llvm::PointerType::get(RetType->getPointerElementType(),
                             matBase->getType()->getPointerAddressSpace());

  Value *idx = paramList[HLOperandIndex::kMatSubscriptSubOpIdx-1];

  // Lower _m00 into real idx.

  // -1 to avoid opcode param which is added in EmitHLSLMatrixOperationCallImp.
  Value *args[] = {paramList[HLOperandIndex::kMatSubscriptMatOpIdx - 1],
                   paramList[HLOperandIndex::kMatSubscriptSubOpIdx - 1]};

  unsigned row, col;
  hlsl::GetHLSLMatRowColCount(Ty, row, col);
  Value *Ptr = paramList[0];
  if (Value *OriginPtr = GetOriginMatrixOperandAndUpdateMatSize(Ptr, row, col)) {
    args[0] = OriginPtr;
  }

  // For all zero idx. Still all zero idx.
  if (ConstantAggregateZero *zeros = dyn_cast<ConstantAggregateZero>(idx)) {
    Constant *zero = zeros->getAggregateElement((unsigned)0);
    std::vector<Constant *> elts(zeros->getNumElements() >> 1, zero);
    args[HLOperandIndex::kMatSubscriptSubOpIdx - 1] = ConstantVector::get(elts);
  } else {
    ConstantDataSequential *elts = cast<ConstantDataSequential>(idx);
    unsigned count = elts->getNumElements();
    std::vector<Constant *> idxs(count >> 1);
    for (unsigned i = 0; i < count; i += 2) {
      unsigned rowIdx = elts->getElementAsInteger(i);
      unsigned colIdx = elts->getElementAsInteger(i + 1);
      unsigned matIdx = 0;
      if (isRowMajor) {
        matIdx = rowIdx * col + colIdx;
      } else {
        matIdx = colIdx * row + rowIdx;
      }
      idxs[i >> 1] = CGF.Builder.getInt32(matIdx);
    }
    args[HLOperandIndex::kMatSubscriptSubOpIdx - 1] = ConstantVector::get(idxs);
  }

  return EmitHLSLMatrixOperationCallImp(CGF.Builder, HLOpcodeGroup::HLSubscript,
                                        opcode, RetType, args, TheModule);
}

Value *CGMSHLSLRuntime::EmitHLSLMatrixLoad(CGBuilderTy &Builder, Value *Ptr,
                                           QualType Ty) {
  bool isRowMajor =
    hlsl::IsHLSLMatRowMajor(Ty, m_pHLModule->GetHLOptions().bDefaultRowMajor);
  unsigned opcode =
      isRowMajor
          ? static_cast<unsigned>(HLMatLoadStoreOpcode::RowMatLoad)
          : static_cast<unsigned>(HLMatLoadStoreOpcode::ColMatLoad);

  Value *matVal = EmitHLSLMatrixOperationCallImp(
      Builder, HLOpcodeGroup::HLMatLoadStore, opcode,
      Ptr->getType()->getPointerElementType(), {Ptr}, TheModule);
  if (!isRowMajor) {
    // ColMatLoad will return a col major matrix.
    // All matrix Value should be row major.
    // Cast it to row major.
    matVal = EmitHLSLMatrixOperationCallImp(
        Builder, HLOpcodeGroup::HLCast,
        static_cast<unsigned>(HLCastOpcode::ColMatrixToRowMatrix),
        matVal->getType(), {matVal}, TheModule);
  }
  return matVal;
}
void CGMSHLSLRuntime::EmitHLSLMatrixStore(CGBuilderTy &Builder, Value *Val,
                                          Value *DestPtr, QualType Ty) {
  bool isRowMajor =
    hlsl::IsHLSLMatRowMajor(Ty, m_pHLModule->GetHLOptions().bDefaultRowMajor);
  unsigned opcode =
      isRowMajor
          ? static_cast<unsigned>(HLMatLoadStoreOpcode::RowMatStore)
          : static_cast<unsigned>(HLMatLoadStoreOpcode::ColMatStore);

  if (!isRowMajor) {
    Value *ColVal = nullptr;
    // If Val is casted from col major. Just use the original col major val.
    if (CallInst *CI = dyn_cast<CallInst>(Val)) {
      hlsl::HLOpcodeGroup group =
          hlsl::GetHLOpcodeGroupByName(CI->getCalledFunction());
      if (group == HLOpcodeGroup::HLCast) {
        HLCastOpcode castOp = static_cast<HLCastOpcode>(hlsl::GetHLOpcode(CI));
        if (castOp == HLCastOpcode::ColMatrixToRowMatrix) {
          ColVal = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
        }
      }
    }
    if (ColVal) {
      Val = ColVal;
    } else {
      // All matrix Value should be row major.
      // ColMatStore need a col major value.
      // Cast it to row major.
      Val = EmitHLSLMatrixOperationCallImp(
          Builder, HLOpcodeGroup::HLCast,
          static_cast<unsigned>(HLCastOpcode::RowMatrixToColMatrix),
          Val->getType(), {Val}, TheModule);
    }
  }

  EmitHLSLMatrixOperationCallImp(Builder, HLOpcodeGroup::HLMatLoadStore, opcode,
                                 Val->getType(), {DestPtr, Val}, TheModule);
}

Value *CGMSHLSLRuntime::EmitHLSLMatrixLoad(CodeGenFunction &CGF, Value *Ptr,
                                           QualType Ty) {
  return EmitHLSLMatrixLoad(CGF.Builder, Ptr, Ty);
}
void CGMSHLSLRuntime::EmitHLSLMatrixStore(CodeGenFunction &CGF, Value *Val,
                                          Value *DestPtr, QualType Ty) {
  EmitHLSLMatrixStore(CGF.Builder, Val, DestPtr, Ty);
}

// Copy data from srcPtr to destPtr.
static void SimplePtrCopy(Value *DestPtr, Value *SrcPtr,
                          ArrayRef<Value *> idxList, CGBuilderTy &Builder) {
  if (idxList.size() > 1) {
    DestPtr = Builder.CreateInBoundsGEP(DestPtr, idxList);
    SrcPtr = Builder.CreateInBoundsGEP(SrcPtr, idxList);
  }
  llvm::LoadInst *ld = Builder.CreateLoad(SrcPtr);
  Builder.CreateStore(ld, DestPtr);
}
// Get Element val from SrvVal with extract value.
static Value *GetEltVal(Value *SrcVal, ArrayRef<Value*> idxList,
    CGBuilderTy &Builder) {
  Value *Val = SrcVal;
  // Skip beginning pointer type.
  for (unsigned i = 1; i < idxList.size(); i++) {
    ConstantInt *idx = cast<ConstantInt>(idxList[i]);
    llvm::Type *Ty = Val->getType();
    if (Ty->isAggregateType()) {
      Val = Builder.CreateExtractValue(Val, idx->getLimitedValue());
    }
  }
  return Val;
}
// Copy srcVal to destPtr.
static void SimpleValCopy(Value *DestPtr, Value *SrcVal,
                       ArrayRef<Value*> idxList,
                       CGBuilderTy &Builder) {
  Value *DestGEP = Builder.CreateInBoundsGEP(DestPtr, idxList);
  Value *Val = GetEltVal(SrcVal, idxList, Builder);

  Builder.CreateStore(Val, DestGEP);
}

static void SimpleCopy(Value *Dest, Value *Src,
                       ArrayRef<Value *> idxList,
                       CGBuilderTy &Builder) {
  if (Src->getType()->isPointerTy())
    SimplePtrCopy(Dest, Src, idxList, Builder);
  else
    SimpleValCopy(Dest, Src, idxList, Builder);
}

void CGMSHLSLRuntime::FlattenAggregatePtrToGepList(
    CodeGenFunction &CGF, Value *Ptr, SmallVector<Value *, 4> &idxList,
    clang::QualType Type, llvm::Type *Ty, SmallVector<Value *, 4> &GepList,
    SmallVector<QualType, 4> &EltTyList) {
  if (llvm::PointerType *PT = dyn_cast<llvm::PointerType>(Ty)) {
    Constant *idx = Constant::getIntegerValue(
        IntegerType::get(Ty->getContext(), 32), APInt(32, 0));
    idxList.emplace_back(idx);

    FlattenAggregatePtrToGepList(CGF, Ptr, idxList, Type, PT->getElementType(),
                                 GepList, EltTyList);

    idxList.pop_back();
  } else if (HLMatrixType MatTy = HLMatrixType::dyn_cast(Ty)) {
    // Use matLd/St for matrix.
    llvm::Type *EltTy = MatTy.getElementTypeForReg();
    llvm::PointerType *EltPtrTy =
        llvm::PointerType::get(EltTy, Ptr->getType()->getPointerAddressSpace());
    QualType EltQualTy = hlsl::GetHLSLMatElementType(Type);

    Value *matPtr = CGF.Builder.CreateInBoundsGEP(Ptr, idxList);

    // Flatten matrix to elements.
    for (unsigned r = 0; r < MatTy.getNumRows(); r++) {
      for (unsigned c = 0; c < MatTy.getNumColumns(); c++) {
        ConstantInt *cRow = CGF.Builder.getInt32(r);
        ConstantInt *cCol = CGF.Builder.getInt32(c);
        Constant *CV = llvm::ConstantVector::get({cRow, cCol});
        GepList.push_back(
            EmitHLSLMatrixElement(CGF, EltPtrTy, {matPtr, CV}, Type));
        EltTyList.push_back(EltQualTy);
      }
    }

  } else if (StructType *ST = dyn_cast<StructType>(Ty)) {
    if (dxilutil::IsHLSLObjectType(ST)) {
      // Avoid split HLSL object.
      Value *GEP = CGF.Builder.CreateInBoundsGEP(Ptr, idxList);
      GepList.push_back(GEP);
      EltTyList.push_back(Type);
      return;
    }
    const clang::RecordType *RT = Type->getAsStructureType();
    RecordDecl *RD = RT->getDecl();

    const CGRecordLayout &RL = CGF.getTypes().getCGRecordLayout(RD);

    if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
      if (CXXRD->getNumBases()) {
        // Add base as field.
        for (const auto &I : CXXRD->bases()) {
          const CXXRecordDecl *BaseDecl =
              cast<CXXRecordDecl>(I.getType()->castAs<RecordType>()->getDecl());
          // Skip empty struct.
          if (BaseDecl->field_empty())
            continue;

          QualType parentTy = QualType(BaseDecl->getTypeForDecl(), 0);
          llvm::Type *parentType = CGF.ConvertType(parentTy);

          unsigned i = RL.getNonVirtualBaseLLVMFieldNo(BaseDecl);
          Constant *idx = llvm::Constant::getIntegerValue(
              IntegerType::get(Ty->getContext(), 32), APInt(32, i));
          idxList.emplace_back(idx);

          FlattenAggregatePtrToGepList(CGF, Ptr, idxList, parentTy, parentType,
                                       GepList, EltTyList);
          idxList.pop_back();
        }
      }
    }

    for (auto fieldIter = RD->field_begin(), fieldEnd = RD->field_end();
         fieldIter != fieldEnd; ++fieldIter) {
      unsigned i = RL.getLLVMFieldNo(*fieldIter);
      llvm::Type *ET = ST->getElementType(i);

      Constant *idx = llvm::Constant::getIntegerValue(
          IntegerType::get(Ty->getContext(), 32), APInt(32, i));
      idxList.emplace_back(idx);

      FlattenAggregatePtrToGepList(CGF, Ptr, idxList, fieldIter->getType(), ET,
                                   GepList, EltTyList);

      idxList.pop_back();
    }

  } else if (llvm::ArrayType *AT = dyn_cast<llvm::ArrayType>(Ty)) {
    llvm::Type *ET = AT->getElementType();

    QualType EltType = CGF.getContext().getBaseElementType(Type);

    for (uint32_t i = 0; i < AT->getNumElements(); i++) {
      Constant *idx = Constant::getIntegerValue(
          IntegerType::get(Ty->getContext(), 32), APInt(32, i));
      idxList.emplace_back(idx);

      FlattenAggregatePtrToGepList(CGF, Ptr, idxList, EltType, ET, GepList,
                                   EltTyList);

      idxList.pop_back();
    }
  } else if (llvm::VectorType *VT = dyn_cast<llvm::VectorType>(Ty)) {
    // Flatten vector too.
    QualType EltTy = hlsl::GetHLSLVecElementType(Type);
    for (uint32_t i = 0; i < VT->getNumElements(); i++) {
      Constant *idx = CGF.Builder.getInt32(i);
      idxList.emplace_back(idx);

      Value *GEP = CGF.Builder.CreateInBoundsGEP(Ptr, idxList);
      GepList.push_back(GEP);
      EltTyList.push_back(EltTy);

      idxList.pop_back();
    }
  } else {
    Value *GEP = CGF.Builder.CreateInBoundsGEP(Ptr, idxList);
    GepList.push_back(GEP);
    EltTyList.push_back(Type);
  }
}

void CGMSHLSLRuntime::LoadElements(CodeGenFunction &CGF,
    ArrayRef<Value *> Ptrs, ArrayRef<QualType> QualTys,
    SmallVector<Value *, 4> &Vals) {
  for (size_t i = 0, e = Ptrs.size(); i < e; i++) {
    Value *Ptr = Ptrs[i];
    llvm::Type *Ty = Ptr->getType()->getPointerElementType();
    DXASSERT_LOCALVAR(Ty, Ty->isIntegerTy() || Ty->isFloatingPointTy(), "Expected only element types.");
    Value *Val = CGF.Builder.CreateLoad(Ptr);
    Val = CGF.EmitFromMemory(Val, QualTys[i]);
    Vals.push_back(Val);
  }
}

void CGMSHLSLRuntime::ConvertAndStoreElements(CodeGenFunction &CGF,
    ArrayRef<Value *> SrcVals, ArrayRef<QualType> SrcQualTys,
    ArrayRef<Value *> DstPtrs, ArrayRef<QualType> DstQualTys) {
  for (size_t i = 0, e = DstPtrs.size(); i < e; i++) {
    Value *DstPtr = DstPtrs[i];
    QualType DstQualTy = DstQualTys[i];
    Value *SrcVal = SrcVals[i];
    QualType SrcQualTy = SrcQualTys[i];
    DXASSERT(SrcVal->getType()->isIntegerTy() || SrcVal->getType()->isFloatingPointTy(),
      "Expected only element types.");

    llvm::Value *Result = ConvertScalarOrVector(CGF, SrcVal, SrcQualTy, DstQualTy);
    Result = CGF.EmitToMemory(Result, DstQualTy);
    CGF.Builder.CreateStore(Result, DstPtr);
  }
}

static bool AreMatrixArrayOrientationMatching(ASTContext& Context,
  HLModule &Module, QualType LhsTy, QualType RhsTy) {
  while (const clang::ArrayType *LhsArrayTy = Context.getAsArrayType(LhsTy)) {
    LhsTy = LhsArrayTy->getElementType();
    RhsTy = Context.getAsArrayType(RhsTy)->getElementType();
  }

  bool LhsRowMajor, RhsRowMajor;
  LhsRowMajor = RhsRowMajor = Module.GetHLOptions().bDefaultRowMajor;
  HasHLSLMatOrientation(LhsTy, &LhsRowMajor);
  HasHLSLMatOrientation(RhsTy, &RhsRowMajor);
  return LhsRowMajor == RhsRowMajor;
}

static llvm::Value *CreateInBoundsGEPIfNeeded(llvm::Value *Ptr, ArrayRef<Value*> IdxList, CGBuilderTy &Builder) {
  DXASSERT(IdxList.size() > 0, "Invalid empty GEP index list");
  // If the GEP list is a single zero, it's a no-op, so save us the trouble.
  if (IdxList.size() == 1) {
    if (ConstantInt *FirstIdx = dyn_cast<ConstantInt>(IdxList[0])) {
      if (FirstIdx->isZero()) return Ptr;
    }
  }
  return Builder.CreateInBoundsGEP(Ptr, IdxList);
}

// Copy data from SrcPtr to DestPtr.
// For matrix, use MatLoad/MatStore.
// For matrix array, EmitHLSLAggregateCopy on each element.
// For struct or array, use memcpy.
// Other just load/store.
void CGMSHLSLRuntime::EmitHLSLAggregateCopy(
    CodeGenFunction &CGF, llvm::Value *SrcPtr, llvm::Value *DestPtr,
    SmallVector<Value *, 4> &idxList, clang::QualType SrcType,
    clang::QualType DestType, llvm::Type *Ty) {
  if (llvm::PointerType *PT = dyn_cast<llvm::PointerType>(Ty)) {
    Constant *idx = Constant::getIntegerValue(
        IntegerType::get(Ty->getContext(), 32), APInt(32, 0));
    idxList.emplace_back(idx);

    EmitHLSLAggregateCopy(CGF, SrcPtr, DestPtr, idxList, SrcType, DestType,
                          PT->getElementType());

    idxList.pop_back();
  } else if (HLMatrixType::isa(Ty)) {
    // Use matLd/St for matrix.
    Value *SrcMatPtr = CreateInBoundsGEPIfNeeded(SrcPtr, idxList, CGF.Builder);
    Value *DestMatPtr = CreateInBoundsGEPIfNeeded(DestPtr, idxList, CGF.Builder);
    Value *ldMat = EmitHLSLMatrixLoad(CGF, SrcMatPtr, SrcType);
    EmitHLSLMatrixStore(CGF, ldMat, DestMatPtr, DestType);
  } else if (StructType *ST = dyn_cast<StructType>(Ty)) {
    if (dxilutil::IsHLSLObjectType(ST)) {
      // Avoid split HLSL object.
      SimpleCopy(DestPtr, SrcPtr, idxList, CGF.Builder);
      return;
    }
    Value *SrcStructPtr = CreateInBoundsGEPIfNeeded(SrcPtr, idxList, CGF.Builder);
    Value *DestStructPtr = CreateInBoundsGEPIfNeeded(DestPtr, idxList, CGF.Builder);
    unsigned size = this->TheModule.getDataLayout().getTypeAllocSize(ST);
    // Memcpy struct.
    CGF.Builder.CreateMemCpy(DestStructPtr, SrcStructPtr, size, 1);
  } else if (llvm::ArrayType *AT = dyn_cast<llvm::ArrayType>(Ty)) {
    if (!HLMatrixType::isMatrixArray(Ty)
      || AreMatrixArrayOrientationMatching(CGF.getContext(), *m_pHLModule, SrcType, DestType)) {
      Value *SrcArrayPtr = CreateInBoundsGEPIfNeeded(SrcPtr, idxList, CGF.Builder);
      Value *DestArrayPtr = CreateInBoundsGEPIfNeeded(DestPtr, idxList, CGF.Builder);
      unsigned size = this->TheModule.getDataLayout().getTypeAllocSize(AT);
      // Memcpy non-matrix array.
      CGF.Builder.CreateMemCpy(DestArrayPtr, SrcArrayPtr, size, 1);
    } else {
      // Copy matrix arrays elementwise if orientation changes are needed.
      llvm::Type *ET = AT->getElementType();
      QualType EltDestType = CGF.getContext().getBaseElementType(DestType);
      QualType EltSrcType = CGF.getContext().getBaseElementType(SrcType);

      for (uint32_t i = 0; i < AT->getNumElements(); i++) {
        Constant *idx = Constant::getIntegerValue(
            IntegerType::get(Ty->getContext(), 32), APInt(32, i));
        idxList.emplace_back(idx);

        EmitHLSLAggregateCopy(CGF, SrcPtr, DestPtr, idxList, EltSrcType,
                              EltDestType, ET);

        idxList.pop_back();
      }
    }
  } else {
    SimpleCopy(DestPtr, SrcPtr, idxList, CGF.Builder);
  }
}

void CGMSHLSLRuntime::EmitHLSLAggregateCopy(CodeGenFunction &CGF, llvm::Value *SrcPtr,
    llvm::Value *DestPtr,
    clang::QualType Ty) {
    SmallVector<Value *, 4> idxList;
    EmitHLSLAggregateCopy(CGF, SrcPtr, DestPtr, idxList, Ty, Ty, SrcPtr->getType());
}

// Make sure all element type of struct is same type.
static bool IsStructWithSameElementType(llvm::StructType *ST, llvm::Type *Ty) {
  for (llvm::Type *EltTy : ST->elements()) {
    if (StructType *EltSt = dyn_cast<StructType>(EltTy)) {
      if (!IsStructWithSameElementType(EltSt, Ty))
        return false;
    } else if (llvm::ArrayType *AT = dyn_cast<llvm::ArrayType>(EltTy)) {
      llvm::Type *ArrayEltTy = dxilutil::GetArrayEltTy(AT);
      if (ArrayEltTy == Ty) {
        continue;
      } else if (StructType *EltSt = dyn_cast<StructType>(EltTy)) {
        if (!IsStructWithSameElementType(EltSt, Ty))
          return false;
      } else {
        return false;
      }

    } else if (EltTy != Ty)
      return false;
  }
  return true;
}

// To memcpy, need element type match.
// For struct type, the layout should match in cbuffer layout.
// struct { float2 x; float3 y; } will not match struct { float3 x; float2 y; }.
// struct { float2 x; float3 y; } will not match array of float.
static bool IsTypeMatchForMemcpy(llvm::Type *SrcTy, llvm::Type *DestTy) {
  llvm::Type *SrcEltTy = dxilutil::GetArrayEltTy(SrcTy);
  llvm::Type *DestEltTy = dxilutil::GetArrayEltTy(DestTy);
  if (SrcEltTy == DestEltTy)
    return true;

  llvm::StructType *SrcST = dyn_cast<llvm::StructType>(SrcEltTy);
  llvm::StructType *DestST = dyn_cast<llvm::StructType>(DestEltTy);
  if (SrcST && DestST) {
    // Only allow identical struct.
    return SrcST->isLayoutIdentical(DestST);
  } else if (!SrcST && !DestST) {
    // For basic type, if one is array, one is not array, layout is different.
    // If both array, type mismatch. If both basic, copy should be fine.
    // So all return false.
    return false;
  } else {
    // One struct, one basic type.
    // Make sure all struct element match the basic type and basic type is
    // vector4.
    llvm::StructType *ST = SrcST ? SrcST : DestST;
    llvm::Type *Ty = SrcST ? DestEltTy : SrcEltTy;
    if (!Ty->isVectorTy())
      return false;
    if (Ty->getVectorNumElements() != 4)
      return false;

    return IsStructWithSameElementType(ST, Ty);
  }
}

void CGMSHLSLRuntime::EmitHLSLFlatConversionAggregateCopy(CodeGenFunction &CGF, llvm::Value *SrcPtr,
    clang::QualType SrcTy,
    llvm::Value *DestPtr,
    clang::QualType DestTy) {
  llvm::Type *SrcPtrTy = SrcPtr->getType()->getPointerElementType();
  llvm::Type *DestPtrTy = DestPtr->getType()->getPointerElementType();

  bool bDefaultRowMajor = m_pHLModule->GetHLOptions().bDefaultRowMajor;
  if (SrcPtrTy == DestPtrTy) {
    bool bMatArrayRotate = false;
    if (HLMatrixType::isMatrixArrayPtr(SrcPtr->getType())) {
      QualType SrcEltTy = GetArrayEltType(CGM.getContext(), SrcTy);
      QualType DestEltTy = GetArrayEltType(CGM.getContext(), DestTy);
      if (GetMatrixMajor(SrcEltTy, bDefaultRowMajor) !=
          GetMatrixMajor(DestEltTy, bDefaultRowMajor)) {
        bMatArrayRotate = true;
      }
    }
    if (!bMatArrayRotate) {
      // Memcpy if type is match.
      unsigned size = TheModule.getDataLayout().getTypeAllocSize(SrcPtrTy);
      CGF.Builder.CreateMemCpy(DestPtr, SrcPtr, size, 1);
      return;
    }
  } else if (dxilutil::IsHLSLResourceDescType(SrcPtrTy) &&
             dxilutil::IsHLSLResourceType(DestPtrTy)) {
    // Cast resource desc to resource.
    Value *CastPtr = CGF.Builder.CreatePointerCast(SrcPtr, DestPtr->getType());
    // Load resource.
    Value *V = CGF.Builder.CreateLoad(CastPtr);
    // Store to resource ptr.
    CGF.Builder.CreateStore(V, DestPtr);
    return;
  } else if (dxilutil::IsHLSLObjectType(dxilutil::GetArrayEltTy(SrcPtrTy)) &&
             dxilutil::IsHLSLObjectType(dxilutil::GetArrayEltTy(DestPtrTy))) {
    unsigned sizeSrc = TheModule.getDataLayout().getTypeAllocSize(SrcPtrTy);
    unsigned sizeDest = TheModule.getDataLayout().getTypeAllocSize(DestPtrTy);
    CGF.Builder.CreateMemCpy(DestPtr, SrcPtr, std::max(sizeSrc, sizeDest), 1);
    return;
  } else if (GlobalVariable *GV = dyn_cast<GlobalVariable>(DestPtr)) {
    if (GV->isInternalLinkage(GV->getLinkage()) &&
        IsTypeMatchForMemcpy(SrcPtrTy, DestPtrTy)) {
      unsigned sizeSrc = TheModule.getDataLayout().getTypeAllocSize(SrcPtrTy);
      unsigned sizeDest = TheModule.getDataLayout().getTypeAllocSize(DestPtrTy);
      CGF.Builder.CreateMemCpy(DestPtr, SrcPtr, std::min(sizeSrc, sizeDest), 1);
      return;
    }
  }

  // It is possible to implement EmitHLSLAggregateCopy, EmitHLSLAggregateStore
  // the same way. But split value to scalar will generate many instruction when
  // src type is same as dest type.
  SmallVector<Value *, 4> GEPIdxStack;
  SmallVector<Value *, 4> SrcPtrs;
  SmallVector<QualType, 4> SrcQualTys;
  FlattenAggregatePtrToGepList(CGF, SrcPtr, GEPIdxStack, SrcTy, SrcPtr->getType(),
                               SrcPtrs, SrcQualTys);

  SmallVector<Value *, 4> SrcVals;
  LoadElements(CGF, SrcPtrs, SrcQualTys, SrcVals);

  GEPIdxStack.clear();
  SmallVector<Value *, 4> DstPtrs;
  SmallVector<QualType, 4> DstQualTys;
  FlattenAggregatePtrToGepList(CGF, DestPtr, GEPIdxStack, DestTy,
                               DestPtr->getType(), DstPtrs, DstQualTys);

  ConvertAndStoreElements(CGF, SrcVals, SrcQualTys, DstPtrs, DstQualTys);
}

void CGMSHLSLRuntime::EmitHLSLAggregateStore(CodeGenFunction &CGF, llvm::Value *SrcVal,
    llvm::Value *DestPtr,
    clang::QualType Ty) {
    DXASSERT(0, "aggregate return type will use SRet, no aggregate store should exist");
}

// Either copies a scalar to a scalar, a scalar to a vector, or splats a scalar to a vector
static void SimpleFlatValCopy(CodeGenFunction &CGF, 
    Value *SrcVal, QualType SrcQualTy, Value *DstPtr, QualType DstQualTy) {
  DXASSERT(SrcVal->getType() == CGF.ConvertType(SrcQualTy), "QualType/Type mismatch!");
  
  llvm::Type *DstTy = DstPtr->getType()->getPointerElementType();
  DXASSERT(DstTy == CGF.ConvertTypeForMem(DstQualTy), "QualType/Type mismatch!");

  llvm::VectorType *DstVecTy = dyn_cast<llvm::VectorType>(DstTy);
  QualType DstScalarQualTy = DstQualTy;
  if (DstVecTy) {
    DstScalarQualTy = hlsl::GetHLSLVecElementType(DstQualTy);
  }

  Value *ResultScalar = ConvertScalarOrVector(CGF, SrcVal, SrcQualTy, DstScalarQualTy);
  ResultScalar = CGF.EmitToMemory(ResultScalar, DstScalarQualTy);

  if (DstVecTy) {
    llvm::VectorType *DstScalarVecTy = llvm::VectorType::get(ResultScalar->getType(), 1);
    Value *ResultScalarVec = CGF.Builder.CreateInsertElement(
      UndefValue::get(DstScalarVecTy), ResultScalar, (uint64_t)0);
    std::vector<int> ShufIdx(DstVecTy->getNumElements(), 0);
    Value *ResultVec = CGF.Builder.CreateShuffleVector(ResultScalarVec, ResultScalarVec, ShufIdx);
    CGF.Builder.CreateStore(ResultVec, DstPtr);
  } else
    CGF.Builder.CreateStore(ResultScalar, DstPtr);
}

void CGMSHLSLRuntime::EmitHLSLSplat(
    CodeGenFunction &CGF, Value *SrcVal, llvm::Value *DestPtr,
    SmallVector<Value *, 4> &idxList, QualType Type, QualType SrcType,
    llvm::Type *Ty) {
  if (llvm::PointerType *PT = dyn_cast<llvm::PointerType>(Ty)) {
    idxList.emplace_back(CGF.Builder.getInt32(0));

    EmitHLSLSplat(CGF, SrcVal, DestPtr, idxList, Type,
                                      SrcType, PT->getElementType());

    idxList.pop_back();
  } else if (HLMatrixType MatTy = HLMatrixType::dyn_cast(Ty)) {
    // Use matLd/St for matrix.
    Value *dstGEP = CGF.Builder.CreateInBoundsGEP(DestPtr, idxList);
    llvm::Type *EltTy = MatTy.getElementTypeForReg();

    llvm::VectorType *VT1 = llvm::VectorType::get(EltTy, 1);
    SrcVal = ConvertScalarOrVector(CGF, SrcVal, SrcType, hlsl::GetHLSLMatElementType(Type));

    // Splat the value
    Value *V1 = CGF.Builder.CreateInsertElement(UndefValue::get(VT1), SrcVal,
                                                (uint64_t)0);
    std::vector<int> shufIdx(MatTy.getNumElements(), 0);
    Value *VecMat = CGF.Builder.CreateShuffleVector(V1, V1, shufIdx);
    Value *MatInit = EmitHLSLMatrixOperationCallImp(
        CGF.Builder, HLOpcodeGroup::HLInit, 0, Ty, {VecMat}, TheModule);
    EmitHLSLMatrixStore(CGF, MatInit, dstGEP, Type);
  } else if (StructType *ST = dyn_cast<StructType>(Ty)) {
    DXASSERT(!dxilutil::IsHLSLObjectType(ST), "cannot cast to hlsl object, Sema should reject");

    const clang::RecordType *RT = Type->getAsStructureType();
    RecordDecl *RD = RT->getDecl();

    const CGRecordLayout &RL = CGF.getTypes().getCGRecordLayout(RD);
    // Take care base.
    if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
      if (CXXRD->getNumBases()) {
        for (const auto &I : CXXRD->bases()) {
          const CXXRecordDecl *BaseDecl =
              cast<CXXRecordDecl>(I.getType()->castAs<RecordType>()->getDecl());
          if (BaseDecl->field_empty())
            continue;
          QualType parentTy = QualType(BaseDecl->getTypeForDecl(), 0);
          unsigned i = RL.getNonVirtualBaseLLVMFieldNo(BaseDecl);

          llvm::Type *ET = ST->getElementType(i);
          Constant *idx = llvm::Constant::getIntegerValue(
              IntegerType::get(Ty->getContext(), 32), APInt(32, i));
          idxList.emplace_back(idx);
          EmitHLSLSplat(CGF, SrcVal, DestPtr, idxList, parentTy, SrcType, ET);
          idxList.pop_back();
        }
      }
    }
    for (auto fieldIter = RD->field_begin(), fieldEnd = RD->field_end();
         fieldIter != fieldEnd; ++fieldIter) {
      unsigned i = RL.getLLVMFieldNo(*fieldIter);
      llvm::Type *ET = ST->getElementType(i);

      Constant *idx = llvm::Constant::getIntegerValue(
          IntegerType::get(Ty->getContext(), 32), APInt(32, i));
      idxList.emplace_back(idx);

      EmitHLSLSplat(CGF, SrcVal, DestPtr, idxList, fieldIter->getType(), SrcType, ET);

      idxList.pop_back();
    }

  } else if (llvm::ArrayType *AT = dyn_cast<llvm::ArrayType>(Ty)) {
    llvm::Type *ET = AT->getElementType();

    QualType EltType = CGF.getContext().getBaseElementType(Type);

    for (uint32_t i = 0; i < AT->getNumElements(); i++) {
      Constant *idx = Constant::getIntegerValue(
          IntegerType::get(Ty->getContext(), 32), APInt(32, i));
      idxList.emplace_back(idx);

      EmitHLSLSplat(CGF, SrcVal, DestPtr, idxList, EltType, SrcType, ET);

      idxList.pop_back();
    }
  } else {
    DestPtr = CGF.Builder.CreateInBoundsGEP(DestPtr, idxList);
    SimpleFlatValCopy(CGF, SrcVal, SrcType, DestPtr, Type);
  }
}

void CGMSHLSLRuntime::EmitHLSLFlatConversion(CodeGenFunction &CGF,
                                             Value *Val,
                                             Value *DestPtr,
                                             QualType Ty,
                                             QualType SrcTy) {
  SmallVector<Value *, 4> SrcVals;
  SmallVector<QualType, 4> SrcQualTys;
  FlattenValToInitList(CGF, SrcVals, SrcQualTys, SrcTy, Val);

  if (SrcVals.size() == 1) {
    // Perform a splat
    SmallVector<Value *, 4> GEPIdxStack;
    GEPIdxStack.emplace_back(CGF.Builder.getInt32(0)); // Add first 0 for DestPtr.
    EmitHLSLSplat(
        CGF, SrcVals[0], DestPtr, GEPIdxStack, Ty, SrcQualTys[0],
        DestPtr->getType()->getPointerElementType());
  }
  else {
    SmallVector<Value *, 4> GEPIdxStack;
    SmallVector<Value *, 4> DstPtrs;
    SmallVector<QualType, 4> DstQualTys;
    FlattenAggregatePtrToGepList(CGF, DestPtr, GEPIdxStack, Ty, DestPtr->getType(), DstPtrs, DstQualTys);

    ConvertAndStoreElements(CGF, SrcVals, SrcQualTys, DstPtrs, DstQualTys);
  }
}

void CGMSHLSLRuntime::EmitHLSLRootSignature(CodeGenFunction &CGF,
                                            HLSLRootSignatureAttr *RSA,
                                            Function *Fn) {
  // Only parse root signature for entry function.
  if (Fn != Entry.Func)
    return;

  StringRef StrRef = RSA->getSignatureName();
  DiagnosticsEngine &Diags = CGF.getContext().getDiagnostics();
  SourceLocation SLoc = RSA->getLocation();
  RootSignatureHandle RootSigHandle;
  clang::CompileRootSignature(StrRef, Diags, SLoc, rootSigVer, DxilRootSignatureCompilationFlags::GlobalRootSignature, &RootSigHandle);
  if (!RootSigHandle.IsEmpty()) {
    RootSigHandle.EnsureSerializedAvailable();
    m_pHLModule->SetSerializedRootSignature(RootSigHandle.GetSerializedBytes(),
                                            RootSigHandle.GetSerializedSize());
  }
}

void CGMSHLSLRuntime::EmitHLSLOutParamConversionInit(
    CodeGenFunction &CGF, const FunctionDecl *FD, const CallExpr *E,
    llvm::SmallVector<LValue, 8> &castArgList,
    llvm::SmallVector<const Stmt *, 8> &argList,
    const std::function<void(const VarDecl *, llvm::Value *)> &TmpArgMap) {
  // Special case: skip first argument of CXXOperatorCall (it is "this").
  unsigned ArgsToSkip = isa<CXXOperatorCallExpr>(E) ? 1 : 0;
  for (uint32_t i = 0; i < FD->getNumParams(); i++) {
    const ParmVarDecl *Param = FD->getParamDecl(i);
    const Expr *Arg = E->getArg(i+ArgsToSkip);
    QualType ParamTy = Param->getType().getNonReferenceType();
    bool isObject = dxilutil::IsHLSLObjectType(CGF.ConvertTypeForMem(ParamTy));
    bool isAggregateType = !isObject &&
      (ParamTy->isArrayType() || ParamTy->isRecordType()) &&
      !hlsl::IsHLSLVecMatType(ParamTy);

    bool EmitRValueAgg = false;
    bool RValOnRef = false;
    if (!Param->isModifierOut()) {
      if (!isAggregateType && !isObject) {
        if (Arg->isRValue() && Param->getType()->isReferenceType()) {
          // RValue on a reference type.
          if (const CStyleCastExpr *cCast = dyn_cast<CStyleCastExpr>(Arg)) {
            // TODO: Evolving this to warn then fail in future language versions.
            // Allow special case like cast uint to uint for back-compat.
            if (cCast->getCastKind() == CastKind::CK_NoOp) {
              if (const ImplicitCastExpr *cast =
                      dyn_cast<ImplicitCastExpr>(cCast->getSubExpr())) {
                if (cast->getCastKind() == CastKind::CK_LValueToRValue) {
                  // update the arg
                  argList[i] = cast->getSubExpr();
                  continue;
                }
              }
            }
          }
          // EmitLValue will report error.
          // Mark RValOnRef to create tmpArg for it.
          RValOnRef = true;
        } else {
          continue;
        }
      } else if (isAggregateType) {
        // aggregate in-only - emit RValue, unless LValueToRValue cast
        EmitRValueAgg = true;
        if (const ImplicitCastExpr *cast =
                dyn_cast<ImplicitCastExpr>(Arg)) {
          if (cast->getCastKind() == CastKind::CK_LValueToRValue) {
            EmitRValueAgg = false;
          }
        }
      } else {
        // Must be object
        DXASSERT(isObject, "otherwise, flow condition changed, breaking assumption");
        // in-only objects should be skipped to preserve previous behavior.
        continue;
      }
    }

    // Skip unbounded array, since we cannot preserve copy-in copy-out
    // semantics for these.
    if (ParamTy->isIncompleteArrayType()) {
      continue;
    }

    if (!Param->isModifierOut() && !RValOnRef) {
      // No need to copy arg to in-only param for hlsl intrinsic.
      if (const FunctionDecl *Callee = E->getDirectCallee()) {
        if (Callee->hasAttr<HLSLIntrinsicAttr>())
          continue;
      }
    }


    // get original arg
    // FIXME: This will not emit in correct argument order with the other
    //        arguments. This should be integrated into
    //        CodeGenFunction::EmitCallArg if possible.
    RValue argRV; // emit this if aggregate arg on in-only param
    LValue argLV; // otherwise, we may emit this
    llvm::Value *argAddr = nullptr;
    QualType argType = Arg->getType();
    CharUnits argAlignment;
    if (EmitRValueAgg) {
      argRV = CGF.EmitAnyExprToTemp(Arg);
      argAddr = argRV.getAggregateAddr(); // must be alloca
      argAlignment = CharUnits::fromQuantity(cast<AllocaInst>(argAddr)->getAlignment());
      argLV = LValue::MakeAddr(argAddr, ParamTy, argAlignment, CGF.getContext());
    } else {
      argLV = CGF.EmitLValue(Arg);
      if (argLV.isSimple())
        argAddr = argLV.getAddress();
      // When there's argument need to lower like buffer/cbuffer load, need to
      // copy to let the lower not happen on argument when calle is noinline or
      // extern functions. Will do it in HLLegalizeParameter after known which
      // functions are extern but before inline.
      bool bConstGlobal = false;
      Value *Ptr = argAddr;
      while (GEPOperator *GEP = dyn_cast_or_null<GEPOperator>(Ptr)) {
        Ptr = GEP->getPointerOperand();
      }
      if (GlobalVariable *GV = dyn_cast_or_null<GlobalVariable>(Ptr)) {
        bConstGlobal = m_ConstVarAnnotationMap.count(GV) | GV->isConstant();
      }

      // Skip copy-in copy-out when safe.
      // The unsafe case will be global variable alias with parameter.
      // Then global variable is updated in the function, the parameter will
      // be updated silently. For non global variable or constant global
      // variable, it should be safe.
      if (argAddr &&
          (isa<AllocaInst>(Ptr) || isa<Argument>(Ptr) || bConstGlobal)) {
        llvm::Type *ToTy = CGF.ConvertType(ParamTy.getNonReferenceType());
        if (argAddr->getType()->getPointerElementType() == ToTy &&
            // Check clang Type for case like int cast to unsigned.
            ParamTy.getNonReferenceType().getCanonicalType().getTypePtr() ==
                Arg->getType().getCanonicalType().getTypePtr())
          continue;
      }
      argType = argLV.getType();  // TBD: Can this be different than Arg->getType()?
      argAlignment = argLV.getAlignment();
    }
    // After emit Arg, we must update the argList[i],
    // otherwise we get double emit of the expression.

    // create temp Var
    VarDecl *tmpArg =
        VarDecl::Create(CGF.getContext(), const_cast<FunctionDecl *>(FD),
                        SourceLocation(), SourceLocation(),
                        /*IdentifierInfo*/ nullptr, ParamTy,
                        CGF.getContext().getTrivialTypeSourceInfo(ParamTy),
                        StorageClass::SC_Auto);

    // Aggregate type will be indirect param convert to pointer type.
    // So don't update to ReferenceType, use RValue for it.
    const DeclRefExpr *tmpRef = DeclRefExpr::Create(
        CGF.getContext(), NestedNameSpecifierLoc(), SourceLocation(), tmpArg,
        /*enclosing*/ false, tmpArg->getLocation(), ParamTy,
        (isAggregateType || isObject) ? VK_RValue : VK_LValue);

    // must update the arg, since we did emit Arg, else we get double emit.
    argList[i] = tmpRef;

    // create alloc for the tmp arg
    Value *tmpArgAddr = nullptr;
    BasicBlock *InsertBlock = CGF.Builder.GetInsertBlock();
    Function *F = InsertBlock->getParent();

    // Make sure the alloca is in entry block to stop inline create stacksave.
    IRBuilder<> AllocaBuilder(dxilutil::FindAllocaInsertionPt(F));
    tmpArgAddr = AllocaBuilder.CreateAlloca(CGF.ConvertTypeForMem(ParamTy));

    // add it to local decl map
    TmpArgMap(tmpArg, tmpArgAddr);

    LValue tmpLV = LValue::MakeAddr(tmpArgAddr, ParamTy, argAlignment,
                                    CGF.getContext());

    // save for cast after call
    if (Param->isModifierOut()) {
      castArgList.emplace_back(tmpLV);
      castArgList.emplace_back(argLV);
    }

    // cast before the call
    if (Param->isModifierIn() &&
        // Don't copy object
        !isObject) {
      QualType ArgTy = Arg->getType();
      Value *outVal = nullptr;
      if (!isAggregateType) {
        if (!IsHLSLMatType(ParamTy)) {
          RValue outRVal = CGF.EmitLoadOfLValue(argLV, SourceLocation());
          outVal = outRVal.getScalarVal();
        } else {
          DXASSERT(argAddr, "should be RV or simple LV");
          outVal = EmitHLSLMatrixLoad(CGF, argAddr, ArgTy);
        }

        llvm::Type *ToTy = tmpArgAddr->getType()->getPointerElementType();
        if (HLMatrixType::isa(ToTy)) {
          Value *castVal = CGF.Builder.CreateBitCast(outVal, ToTy);
          EmitHLSLMatrixStore(CGF, castVal, tmpArgAddr, ParamTy);
        }
        else {
          Value *castVal = ConvertScalarOrVector(CGF, outVal, argType, ParamTy);
          castVal = CGF.EmitToMemory(castVal, ParamTy);
          CGF.Builder.CreateStore(castVal, tmpArgAddr);
        }
      } else {
        DXASSERT(argAddr, "should be RV or simple LV");
        SmallVector<Value *, 4> idxList;
        EmitHLSLAggregateCopy(CGF, argAddr, tmpArgAddr,
                              idxList, ArgTy, ParamTy,
                              argAddr->getType());
      }
    }
  }
}

void CGMSHLSLRuntime::EmitHLSLOutParamConversionCopyBack(
    CodeGenFunction &CGF, llvm::SmallVector<LValue, 8> &castArgList) {
  for (uint32_t i = 0; i < castArgList.size(); i += 2) {
    // cast after the call
    LValue tmpLV = castArgList[i];
    LValue argLV = castArgList[i + 1];
    QualType ArgTy = argLV.getType().getNonReferenceType();
    QualType ParamTy = tmpLV.getType().getNonReferenceType();

    Value *tmpArgAddr = tmpLV.getAddress();
    
    Value *outVal = nullptr;

    bool isAggregateTy = hlsl::IsHLSLAggregateType(ArgTy);

    bool isObject = dxilutil::IsHLSLObjectType(
       tmpArgAddr->getType()->getPointerElementType());
    if (!isObject) {
      if (!isAggregateTy) {
        if (!IsHLSLMatType(ParamTy))
          outVal = CGF.Builder.CreateLoad(tmpArgAddr);
        else
          outVal = EmitHLSLMatrixLoad(CGF, tmpArgAddr, ParamTy);

        outVal = CGF.EmitFromMemory(outVal, ParamTy);

        llvm::Type *ToTy = CGF.ConvertType(ArgTy);
        llvm::Type *FromTy = outVal->getType();
        Value *castVal = outVal;
        if (ToTy == FromTy) {
          // Don't need cast.
        } else if (ToTy->getScalarType() == FromTy->getScalarType()) {
          if (ToTy->getScalarType() == ToTy) {
            DXASSERT(FromTy->isVectorTy() &&
                         FromTy->getVectorNumElements() == 1,
                     "must be vector of 1 element");
            castVal = CGF.Builder.CreateExtractElement(outVal, (uint64_t)0);
          } else {
            DXASSERT(!FromTy->isVectorTy(), "must be scalar type");
            DXASSERT(ToTy->isVectorTy() && ToTy->getVectorNumElements() == 1,
                     "must be vector of 1 element");
            castVal = UndefValue::get(ToTy);
            castVal =
                CGF.Builder.CreateInsertElement(castVal, outVal, (uint64_t)0);
          }
        } else {
          castVal = ConvertScalarOrVector(CGF,
            outVal, tmpLV.getType(), argLV.getType());
        }
        if (!HLMatrixType::isa(ToTy))
          CGF.EmitStoreThroughLValue(RValue::get(castVal), argLV);
        else {
          Value *destPtr = argLV.getAddress();
          EmitHLSLMatrixStore(CGF, castVal, destPtr, ArgTy);
        }
      } else {
        SmallVector<Value *, 4> idxList;
        EmitHLSLAggregateCopy(CGF, tmpLV.getAddress(), argLV.getAddress(),
                              idxList, ParamTy, ArgTy,
                              argLV.getAddress()->getType());
      }
    } else
      tmpArgAddr->replaceAllUsesWith(argLV.getAddress());
  }
}

CGHLSLRuntime *CodeGen::CreateMSHLSLRuntime(CodeGenModule &CGM) {
  return new CGMSHLSLRuntime(CGM);
}
