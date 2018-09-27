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
#include "dxc/HLSL/HLMatrixLowerHelper.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/HLSL/DxilUtil.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/HlslTypes.h"
#include "clang/Frontend/CodeGenOptions.h"
#include "clang/Lex/HLSLMacroExpander.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"
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

#include "dxc/HLSL/DxilRootSignature.h"
#include "dxc/HLSL/DxilCBuffer.h"
#include "clang/Parse/ParseHLSL.h"      // root sig would be in Parser if part of lang
#include "dxc/Support/WinIncludes.h"    // stream support
#include "dxc/dxcapi.h"                 // stream support
#include "dxc/HLSL/HLSLExtensionsCodegenHelper.h"
#include "dxc/HLSL/DxilGenerationPass.h" // support pause/resume passes
#include "dxc/HLSL/DxilExportMap.h"

using namespace clang;
using namespace CodeGen;
using namespace hlsl;
using namespace llvm;
using std::unique_ptr;

static const bool KeepUndefinedTrue = true; // Keep interpolation mode undefined if not set explicitly.

namespace {

/// Use this class to represent HLSL cbuffer in high-level DXIL.
class HLCBuffer : public DxilCBuffer {
public:
  HLCBuffer() = default;
  virtual ~HLCBuffer() = default;

  void AddConst(std::unique_ptr<DxilResourceBase> &pItem);

  std::vector<std::unique_ptr<DxilResourceBase>> &GetConstants();

private:
  std::vector<std::unique_ptr<DxilResourceBase>> constants; // constants inside const buffer
};

//------------------------------------------------------------------------------
//
// HLCBuffer methods.
//
void HLCBuffer::AddConst(std::unique_ptr<DxilResourceBase> &pItem) {
  pItem->SetID(constants.size());
  constants.push_back(std::move(pItem));
}

std::vector<std::unique_ptr<DxilResourceBase>> &HLCBuffer::GetConstants() {
  return constants;
}

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
                 DxilResource *hlslRes, const RecordDecl *RD);
  uint32_t AddCBuffer(HLSLBufferDecl *D);
  hlsl::DxilResourceBase::Class TypeToClass(clang::QualType Ty);

  // Save the entryFunc so don't need to find it with original name.
  struct EntryFunctionInfo {
    clang::SourceLocation SL = clang::SourceLocation();
    llvm::Function *Func = nullptr;
  };

  EntryFunctionInfo Entry;

  // Map to save patch constant functions
  struct PatchConstantInfo {
    clang::SourceLocation SL = clang::SourceLocation();
    llvm::Function *Func = nullptr;
    std::uint32_t NumOverloads = 0;
  };

  StringMap<PatchConstantInfo> patchConstantFunctionMap;
  std::unordered_map<Function *, std::unique_ptr<DxilFunctionProps>>
      patchConstantFunctionPropsMap;
  bool IsPatchConstantFunction(const Function *F);

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
  void LoadFlattenedGepList(CodeGenFunction &CGF, ArrayRef<Value *> GepList,
                            ArrayRef<QualType> EltTyList,
                            SmallVector<Value *, 4> &EltList);
  void StoreFlattenedGepList(CodeGenFunction &CGF, ArrayRef<Value *> GepList,
                             ArrayRef<QualType> GepTyList,
                             ArrayRef<Value *> EltValList,
                             ArrayRef<QualType> SrcTyList);

  void EmitHLSLAggregateCopy(CodeGenFunction &CGF, llvm::Value *SrcPtr,
                                   llvm::Value *DestPtr,
                                   SmallVector<Value *, 4> &idxList,
                                   clang::QualType SrcType,
                                   clang::QualType DestType,
                                   llvm::Type *Ty);

  void EmitHLSLFlatConversionToAggregate(CodeGenFunction &CGF, Value *SrcVal,
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

  void SetEntryFunction();
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
  MDNode *GetOrAddResTypeMD(QualType resTy);
  void ConstructFieldAttributedAnnotation(DxilFieldAnnotation &fieldAnnotation,
                                          QualType fieldTy,
                                          bool bDefaultRowMajor);

  std::unordered_map<Constant*, DxilFieldAnnotation> m_ConstVarAnnotationMap;

public:
  CGMSHLSLRuntime(CodeGenModule &CGM);

  bool IsHlslObjectType(llvm::Type * Ty) override;

  /// Add resouce to the program
  void addResource(Decl *D) override;
  void SetPatchConstantFunction(const EntryFunctionInfo &EntryFunc);
  void SetPatchConstantFunctionWithAttr(
      const EntryFunctionInfo &EntryFunc,
      const clang::HLSLPatchConstantFuncAttr *PatchConstantFuncAttr);
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

  void EmitHLSLFlatConversionToAggregate(CodeGenFunction &CGF, Value *Val,
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
  
  void FinishAutoVar(CodeGenFunction &CGF, const VarDecl &D, llvm::Value *V) override;

  /// Get or add constant to the program
  HLCBuffer &GetOrCreateCBuffer(HLSLBufferDecl *D);
};
}

void clang::CompileRootSignature(
    StringRef rootSigStr, DiagnosticsEngine &Diags, SourceLocation SLoc,
    hlsl::DxilRootSignatureVersion rootSigVer,
    hlsl::RootSignatureHandle *pRootSigHandle) {
  std::string OSStr;
  llvm::raw_string_ostream OS(OSStr);
  hlsl::DxilVersionedRootSignatureDesc *D = nullptr;

  if (ParseHLSLRootSignature(rootSigStr.data(), rootSigStr.size(), rootSigVer,
                             &D, SLoc, Diags)) {
    CComPtr<IDxcBlob> pSignature;
    CComPtr<IDxcBlobEncoding> pErrors;
    hlsl::SerializeRootSignature(D, &pSignature, &pErrors, false);
    if (pSignature == nullptr) {
      assert(pErrors != nullptr && "else serialize failed with no msg");
      ReportHLSLRootSigError(Diags, SLoc, (char *)pErrors->GetBufferPointer(),
                             pErrors->GetBufferSize());
      hlsl::DeleteRootSignature(D);
    } else {
      pRootSigHandle->Assign(D, pSignature);
    }
  }
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

  // Set Option.
  HLOptions opts;
  opts.bIEEEStrict = CGM.getCodeGenOpts().UnsafeFPMath;
  opts.bDefaultRowMajor = CGM.getCodeGenOpts().HLSLDefaultRowMajor;
  opts.bDisableOptimizations = CGM.getCodeGenOpts().DisableLLVMOpts;
  opts.bLegacyCBufferLoad = !CGM.getCodeGenOpts().HLSLNotUseLegacyCBufLoad;
  opts.bAllResourcesBound = CGM.getCodeGenOpts().HLSLAllResourcesBound;
  opts.PackingStrategy = CGM.getCodeGenOpts().HLSLSignaturePackingStrategy;

  opts.bUseMinPrecision = CGM.getLangOpts().UseMinPrecision;
  opts.bDX9CompatMode = CGM.getLangOpts().EnableDX9CompatMode;
  opts.bFXCCompatMode = CGM.getLangOpts().EnableFXCCompatMode;

  m_pHLModule->SetHLOptions(opts);
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

bool CGMSHLSLRuntime::IsHlslObjectType(llvm::Type *Ty) {
  return HLModule::IsHLSLObjectType(Ty);
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

static unsigned RoundToAlign(unsigned num, unsigned mod) {
  // round num to next highest mod
  if (mod != 0)
    return mod * ((num + mod - 1) / mod);
  return num;
}

// Align cbuffer offset in legacy mode (16 bytes per row).
static unsigned AlignBufferOffsetInLegacy(unsigned offset, unsigned size,
                                          unsigned scalarSizeInBytes,
                                          bool bNeedNewRow) {
  if (unsigned remainder = (offset & 0xf)) {
    // Start from new row
    if (remainder + size > 16 || bNeedNewRow) {
      return offset + 16 - remainder;
    }
    // If not, naturally align data
    return RoundToAlign(offset, scalarSizeInBytes);
  }
  return offset;
}

static unsigned AlignBaseOffset(unsigned baseOffset, unsigned size,
                                 QualType Ty, bool bDefaultRowMajor) {
  bool needNewAlign = Ty->isArrayType();

  if (IsHLSLMatType(Ty)) {
    bool bColMajor = !bDefaultRowMajor;
    if (const AttributedType *AT = dyn_cast<AttributedType>(Ty)) {
      switch (AT->getAttrKind()) {
      case AttributedType::Kind::attr_hlsl_column_major:
        bColMajor = true;
        break;
      case AttributedType::Kind::attr_hlsl_row_major:
        bColMajor = false;
        break;
      default:
        // Do nothing
        break;
      }
    }

    unsigned row, col;
    hlsl::GetHLSLMatRowColCount(Ty, row, col);

    needNewAlign |= bColMajor && col > 1;
    needNewAlign |= !bColMajor && row > 1;
  }

  unsigned scalarSizeInBytes = 4;
  const clang::BuiltinType *BT = Ty->getAs<clang::BuiltinType>();
  if (hlsl::IsHLSLVecMatType(Ty)) {
    BT = CGHLSLRuntime::GetHLSLVecMatElementType(Ty)->getAs<clang::BuiltinType>();
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

  return AlignBufferOffsetInLegacy(baseOffset, size, scalarSizeInBytes, needNewAlign);
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

static unsigned GetMatrixSizeInCB(QualType Ty, bool defaultRowMajor,
                                  bool b64Bit) {
  bool bColMajor = !defaultRowMajor;
  if (const AttributedType *AT = dyn_cast<AttributedType>(Ty)) {
    switch (AT->getAttrKind()) {
    case AttributedType::Kind::attr_hlsl_column_major:
      bColMajor = true;
      break;
    case AttributedType::Kind::attr_hlsl_row_major:
      bColMajor = false;
      break;
    default:
      // Do nothing
      break;
    }
  }

  unsigned row, col;
  hlsl::GetHLSLMatRowColCount(Ty, row, col);

  unsigned EltSize = b64Bit ? 8 : 4;

  // Align to 4 * 4bytes.
  unsigned alignment = 4 * 4;

  if (bColMajor) {
    unsigned rowSize = EltSize * row;
    // 3x64bit or 4x64bit align to 32 bytes.
    if (rowSize > alignment)
      alignment <<= 1;

    return alignment * (col - 1) + row * EltSize;
  } else {
    unsigned rowSize = EltSize * col;
    // 3x64bit or 4x64bit align to 32 bytes.
    if (rowSize > alignment)
      alignment <<= 1;
    return alignment * (row - 1) + col * EltSize;
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

MDNode *CGMSHLSLRuntime::GetOrAddResTypeMD(QualType resTy) {
  const RecordType *RT = resTy->getAs<RecordType>();
  if (!RT)
    return nullptr;
  RecordDecl *RD = RT->getDecl();
  SourceLocation loc = RD->getLocation();

  hlsl::DxilResourceBase::Class resClass = TypeToClass(resTy);
  llvm::Type *Ty = CGM.getTypes().ConvertType(resTy);
  auto it = resMetadataMap.find(Ty);
  if (it != resMetadataMap.end())
    return it->second;

  // Save resource type metadata.
  switch (resClass) {
  case DXIL::ResourceClass::UAV: {
    DxilResource UAV;
    // TODO: save globalcoherent to variable in EmitHLSLBuiltinCallExpr.
    SetUAVSRV(loc, resClass, &UAV, RD);
    // Set global symbol to save type.
    UAV.SetGlobalSymbol(UndefValue::get(Ty));
    MDNode *MD = m_pHLModule->DxilUAVToMDNode(UAV);
    resMetadataMap[Ty] = MD;
    return MD;
  } break;
  case DXIL::ResourceClass::SRV: {
    DxilResource SRV;
    SetUAVSRV(loc, resClass, &SRV, RD);
    // Set global symbol to save type.
    SRV.SetGlobalSymbol(UndefValue::get(Ty));
    MDNode *MD = m_pHLModule->DxilSRVToMDNode(SRV);
    resMetadataMap[Ty] = MD;
    return MD;
  } break;
  case DXIL::ResourceClass::Sampler: {
    DxilSampler S;
    DxilSampler::SamplerKind kind = KeywordToSamplerKind(RD->getName());
    S.SetSamplerKind(kind);
    // Set global symbol to save type.
    S.SetGlobalSymbol(UndefValue::get(Ty));
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

QualType GetArrayEltType(QualType Ty) {
  // Get element type.
  if (Ty->isArrayType()) {
    while (isa<clang::ArrayType>(Ty)) {
      const clang::ArrayType *ATy = dyn_cast<clang::ArrayType>(Ty);
      Ty = ATy->getElementType();
    }
  }
  return Ty;
}

} // namespace

void CGMSHLSLRuntime::ConstructFieldAttributedAnnotation(
    DxilFieldAnnotation &fieldAnnotation, QualType fieldTy,
    bool bDefaultRowMajor) {
  QualType Ty = fieldTy;
  if (Ty->isReferenceType())
    Ty = Ty.getNonReferenceType();

  // Get element type.
  Ty = GetArrayEltType(Ty);

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
    MDNode *MD = GetOrAddResTypeMD(Ty);
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

unsigned CGMSHLSLRuntime::ConstructStructAnnotation(DxilStructAnnotation *annotation,
                                      const RecordDecl *RD,
                                      DxilTypeSystem &dxilTypeSys) {
  unsigned fieldIdx = 0;
  unsigned offset = 0;
  bool bDefaultRowMajor = m_pHLModule->GetHLOptions().bDefaultRowMajor;
  if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
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
    if (!fieldSemName.empty())
      fieldAnnotation.SetSemanticString(fieldSemName);
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
    unsigned col, row;
    llvm::Type *EltTy = HLMatrixLower::GetMatrixInfo(Type, col, row);
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
    DxilStructAnnotation *annotation = dxilTypeSys.AddStructAnnotation(ST);

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
    DxilStructAnnotation *annotation = dxilTypeSys.AddStructAnnotation(ST);

    return ConstructStructAnnotation(annotation, RD, dxilTypeSys);
  } else if (IsHLSLResourceType(Ty)) {
    // Save result type info.
    AddTypeAnnotation(GetHLSLResourceResultType(Ty), dxilTypeSys, arrayEltSize);
    // Resource don't count for cbuffer size.
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
  if (keyword == "Texture3D" || keyword == "RWTexture3D" || keyword == "RasterizerOrderedTexture3D")
    return DxilResource::Kind::Texture3D;
  if (keyword == "TextureCube" || keyword == "RWTextureCube")
    return DxilResource::Kind::TextureCube;

  if (keyword == "Texture1DArray" || keyword == "RWTexture1DArray" || keyword == "RasterizerOrderedTexture1DArray")
    return DxilResource::Kind::Texture1DArray;
  if (keyword == "Texture2DArray" || keyword == "RWTexture2DArray" || keyword == "RasterizerOrderedTexture2DArray")
    return DxilResource::Kind::Texture2DArray;
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
    // Save resource type annotation.
    if (const CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(FD)) {
      const CXXRecordDecl *RD = MD->getParent();
      // For nested case like sample_slice_type.
      if (const CXXRecordDecl *PRD =
              dyn_cast<CXXRecordDecl>(RD->getDeclContext())) {
        RD = PRD;
      }

      QualType recordTy = MD->getASTContext().getRecordType(RD);
      hlsl::DxilResourceBase::Class resClass = TypeToClass(recordTy);
      llvm::Type *Ty = CGM.getTypes().ConvertType(recordTy);
      llvm::FunctionType *FT = F->getFunctionType();
      // Save resource type metadata.
      switch (resClass) {
      case DXIL::ResourceClass::UAV: {
        MDNode *MD = GetOrAddResTypeMD(recordTy);
        DXASSERT(MD, "else invalid resource type");
        resMetadataMap[Ty] = MD;
      } break;
      case DXIL::ResourceClass::SRV: {
        MDNode *Meta = GetOrAddResTypeMD(recordTy);
        DXASSERT(Meta, "else invalid resource type");
        resMetadataMap[Ty] = Meta;
        if (FT->getNumParams() > 1) {
          QualType paramTy = MD->getParamDecl(0)->getType();
          // Add sampler type.
          if (TypeToClass(paramTy) == DXIL::ResourceClass::Sampler) {
            llvm::Type *Ty = FT->getParamType(1)->getPointerElementType();
            MDNode *MD = GetOrAddResTypeMD(paramTy);
            DXASSERT(MD, "else invalid resource type");
            resMetadataMap[Ty] = MD;
          }
        }
      } break;
      default:
        // Skip OutputStream for GS.
        break;
      }
    }
    if (intrinsicOpcode == (unsigned)IntrinsicOp::IOP_TraceRay) {
      QualType recordTy = FD->getParamDecl(0)->getType();
      llvm::Type *Ty = CGM.getTypes().ConvertType(recordTy);
      MDNode *MD = GetOrAddResTypeMD(recordTy);
      DXASSERT(MD, "else invalid resource type");
      resMetadataMap[Ty] = MD;
    }
    StringRef lower;
    if (hlsl::GetIntrinsicLowering(FD, lower))
      hlsl::SetHLLowerStrategy(F, lower);

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
      isRay = true;
      funcProps->shaderKind = DXIL::ShaderKind::AnyHit;
      break;
    case 'm':
      isRay = true;
      funcProps->shaderKind = DXIL::ShaderKind::Miss;
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

  // Computer shader.
  if (const HLSLNumThreadsAttr *Attr = FD->getAttr<HLSLNumThreadsAttr>()) {
    isCS = true;
    funcProps->shaderKind = DXIL::ShaderKind::Compute;

    funcProps->ShaderProps.CS.numThreads[0] = Attr->getX();
    funcProps->ShaderProps.CS.numThreads[1] = Attr->getY();
    funcProps->ShaderProps.CS.numThreads[2] = Attr->getZ();

    if (isEntry && !SM->IsCS()) {
      unsigned DiagID = Diags.getCustomDiagID(
          DiagnosticsEngine::Error, "attribute numthreads only valid for CS.");
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
    } else if (isEntry && !SM->IsHS()) {
      unsigned DiagID =
          Diags.getCustomDiagID(DiagnosticsEngine::Warning,
                                "attribute outputtopology only valid for HS.");
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

  const unsigned profileAttributes = isCS + isHS + isDS + isGS + isVS + isPS + isRay;

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

  if (const CXXMethodDecl *MethodDecl = dyn_cast<CXXMethodDecl>(FD)) {
    QualType ThisTy = MethodDecl->getThisType(FD->getASTContext());
    DxilParameterAnnotation &paramAnnotation =
        FuncAnnotation->GetParameterAnnotation(ArgNo++);
    // Construct annoation for this pointer.
    ConstructFieldAttributedAnnotation(paramAnnotation, ThisTy,
                                       bDefaultRowMajor);
  }

  // Ret Info
  QualType retTy = FD->getReturnType();
  DxilParameterAnnotation *pRetTyAnnotation = nullptr;
  if (F->getReturnType()->isVoidTy() && !retTy->isVoidType()) {
    // SRet.
    pRetTyAnnotation = &FuncAnnotation->GetParameterAnnotation(ArgNo++);
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

  for (; ArgNo < F->arg_size(); ++ArgNo, ++ParmIdx) {
    DxilParameterAnnotation &paramAnnotation =
        FuncAnnotation->GetParameterAnnotation(ArgNo);

    const ParmVarDecl *parmDecl = FD->getParamDecl(ParmIdx);

    ConstructFieldAttributedAnnotation(paramAnnotation, parmDecl->getType(),
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

void CGMSHLSLRuntime::FinishAutoVar(CodeGenFunction &CGF, const VarDecl &D, llvm::Value *V) {
  if (D.hasAttr<HLSLPreciseAttr>()) {
    AllocaInst *AI = cast<AllocaInst>(V);
    HLModule::MarkPreciseAttributeWithMetadata(AI);
  }
  // Add type annotation for local variable.
  DxilTypeSystem &typeSys = m_pHLModule->GetTypeSystem();
  unsigned arrayEltSize = 0;
  AddTypeAnnotation(D.getType(), typeSys, arrayEltSize);
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

/// Add resouce to the program
void CGMSHLSLRuntime::addResource(Decl *D) {
  if (HLSLBufferDecl *BD = dyn_cast<HLSLBufferDecl>(D))
    GetOrCreateCBuffer(BD);
  else if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
    hlsl::DxilResourceBase::Class resClass = TypeToClass(VD->getType());
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
      return;
    }

    if (D->hasAttr<HLSLGroupSharedAttr>()) {
      GlobalVariable *GV = cast<GlobalVariable>(CGM.GetAddrOfGlobalVar(VD));
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

uint32_t CGMSHLSLRuntime::AddSampler(VarDecl *samplerDecl) {
  llvm::GlobalVariable *val =
    cast<llvm::GlobalVariable>(CGM.GetAddrOfGlobalVar(samplerDecl));

  unique_ptr<DxilSampler> hlslRes(new DxilSampler);
  hlslRes->SetLowerBound(UINT_MAX);
  hlslRes->SetGlobalSymbol(val);
  hlslRes->SetGlobalName(samplerDecl->getName());
  QualType VarTy = samplerDecl->getType();
  if (const clang::ArrayType *arrayType =
          CGM.getContext().getAsArrayType(VarTy)) {
    if (arrayType->isConstantArrayType()) {
      uint32_t arraySize =
          cast<ConstantArrayType>(arrayType)->getSize().getLimitedValue();
      hlslRes->SetRangeSize(arraySize);
    } else {
      hlslRes->SetRangeSize(UINT_MAX);
    }
    // use elementTy
    VarTy = arrayType->getElementType();
    // Support more dim.
    while (const clang::ArrayType *arrayType =
               CGM.getContext().getAsArrayType(VarTy)) {
      unsigned rangeSize = hlslRes->GetRangeSize();
      if (arrayType->isConstantArrayType()) {
        uint32_t arraySize =
            cast<ConstantArrayType>(arrayType)->getSize().getLimitedValue();
        if (rangeSize != UINT_MAX)
          hlslRes->SetRangeSize(rangeSize * arraySize);
      } else
        hlslRes->SetRangeSize(UINT_MAX);
      // use elementTy
      VarTy = arrayType->getElementType();
    }
  } else
    hlslRes->SetRangeSize(1);

  const RecordType *RT = VarTy->getAs<RecordType>();
  DxilSampler::SamplerKind kind = KeywordToSamplerKind(RT->getDecl()->getName());

  hlslRes->SetSamplerKind(kind);

  for (hlsl::UnusualAnnotation *it : samplerDecl->getUnusualAnnotations()) {
    switch (it->getKind()) {
    case hlsl::UnusualAnnotation::UA_RegisterAssignment: {
      hlsl::RegisterAssignment *ra = cast<hlsl::RegisterAssignment>(it);
      hlslRes->SetLowerBound(ra->RegisterNumber);
      hlslRes->SetSpaceID(ra->RegisterSpace);
      break;
    }
    case hlsl::UnusualAnnotation::UA_SemanticDecl:
      // Ignore Semantics
      break;
    case hlsl::UnusualAnnotation::UA_ConstantPacking:
      // Should be handled by front-end
      llvm_unreachable("packoffset on sampler");
      break;
    default:
      llvm_unreachable("unknown UnusualAnnotation on sampler");
      break;
    }
  }

  hlslRes->SetID(m_pHLModule->GetSamplers().size());
  return m_pHLModule->AddSampler(std::move(hlslRes));
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
                                DxilResource *hlslRes, const RecordDecl *RD) {
  hlsl::DxilResource::Kind kind = KeywordToKind(RD->getName());
  hlslRes->SetKind(kind);

  // Get the result type from handle field.
  FieldDecl *FD = *(RD->field_begin());
  DXASSERT(FD->getName() == "h", "must be handle field");
  QualType resultTy = FD->getType();
  // Type annotation for result type of resource.
  DxilTypeSystem &dxilTypeSys = m_pHLModule->GetTypeSystem();
  unsigned arrayEltSize = 0;
  AddTypeAnnotation(QualType(RD->getTypeForDecl(),0), dxilTypeSys, arrayEltSize);

  if (kind == hlsl::DxilResource::Kind::Texture2DMS ||
      kind == hlsl::DxilResource::Kind::Texture2DMSArray) {
    const ClassTemplateSpecializationDecl *templateDecl =
        dyn_cast<ClassTemplateSpecializationDecl>(RD);
    const clang::TemplateArgument &sampleCountArg =
        templateDecl->getTemplateArgs()[1];
    uint32_t sampleCount = sampleCountArg.getAsIntegral().getLimitedValue();
    hlslRes->SetSampleCount(sampleCount);
  }

  if (kind != hlsl::DxilResource::Kind::StructuredBuffer) {
    QualType Ty = resultTy;
    QualType EltTy = Ty;
    if (hlsl::IsHLSLVecType(Ty)) {
      EltTy = hlsl::GetHLSLVecElementType(Ty);
    } else if (hlsl::IsHLSLMatType(Ty)) {
      EltTy = hlsl::GetHLSLMatElementType(Ty);
    } else if (resultTy->isAggregateType()) {
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

    EltTy = EltTy.getCanonicalType();
    bool bSNorm = false;
    bool bUNorm = false;

    if (const AttributedType *AT = dyn_cast<AttributedType>(Ty)) {
      switch (AT->getAttrKind()) {
      case AttributedType::Kind::attr_hlsl_snorm:
        bSNorm = true;
        break;
      case AttributedType::Kind::attr_hlsl_unorm:
        bUNorm = true;
        break;
      default:
        // Do nothing
        break;
      }
    }

    if (EltTy->isBuiltinType()) {
      const BuiltinType *BTy = EltTy->getAs<BuiltinType>();
      CompType::Kind kind = BuiltinTyToCompTy(BTy, bSNorm, bUNorm);
      // 64bits types are implemented with u32.
      if (kind == CompType::Kind::U64 || kind == CompType::Kind::I64 ||
          kind == CompType::Kind::SNormF64 ||
          kind == CompType::Kind::UNormF64 || kind == CompType::Kind::F64) {
        kind = CompType::Kind::U32;
      }
      hlslRes->SetCompType(kind);
    } else {
      DXASSERT(!bSNorm && !bUNorm, "snorm/unorm on invalid type");
    }
  }

  hlslRes->SetROV(RD->getName().startswith("RasterizerOrdered"));

  if (kind == hlsl::DxilResource::Kind::TypedBuffer ||
      kind == hlsl::DxilResource::Kind::StructuredBuffer) {
    const ClassTemplateSpecializationDecl *templateDecl =
        dyn_cast<ClassTemplateSpecializationDecl>(RD);

    const clang::TemplateArgument &retTyArg =
        templateDecl->getTemplateArgs()[0];
    llvm::Type *retTy = CGM.getTypes().ConvertType(retTyArg.getAsType());

    uint32_t strideInBytes = dataLayout.getTypeAllocSize(retTy);
    hlslRes->SetElementStride(strideInBytes);
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

  QualType VarTy = decl->getType().getCanonicalType();

  unique_ptr<HLResource> hlslRes(new HLResource);
  hlslRes->SetLowerBound(UINT_MAX);
  hlslRes->SetGlobalSymbol(val);
  hlslRes->SetGlobalName(decl->getName());
  if (const clang::ArrayType *arrayType =
          CGM.getContext().getAsArrayType(VarTy)) {
    if (arrayType->isConstantArrayType()) {
      uint32_t arraySize =
          cast<ConstantArrayType>(arrayType)->getSize().getLimitedValue();
      hlslRes->SetRangeSize(arraySize);
    } else
      hlslRes->SetRangeSize(UINT_MAX);
    // use elementTy
    VarTy = arrayType->getElementType();
    // Support more dim.
    while (const clang::ArrayType *arrayType =
               CGM.getContext().getAsArrayType(VarTy)) {
      unsigned rangeSize = hlslRes->GetRangeSize();
      if (arrayType->isConstantArrayType()) {
        uint32_t arraySize =
            cast<ConstantArrayType>(arrayType)->getSize().getLimitedValue();
        if (rangeSize != UINT_MAX)
          hlslRes->SetRangeSize(rangeSize * arraySize);
      } else
        hlslRes->SetRangeSize(UINT_MAX);
      // use elementTy
      VarTy = arrayType->getElementType();
    }
  } else
    hlslRes->SetRangeSize(1);

  for (hlsl::UnusualAnnotation *it : decl->getUnusualAnnotations()) {
    switch (it->getKind()) {
    case hlsl::UnusualAnnotation::UA_RegisterAssignment: {
      hlsl::RegisterAssignment *ra = cast<hlsl::RegisterAssignment>(it);
      hlslRes->SetLowerBound(ra->RegisterNumber);
      hlslRes->SetSpaceID(ra->RegisterSpace);
      break;
    }
    case hlsl::UnusualAnnotation::UA_SemanticDecl:
      // Ignore Semantics
      break;
    case hlsl::UnusualAnnotation::UA_ConstantPacking:
      // Should be handled by front-end
      llvm_unreachable("packoffset on uav/srv");
      break;
    default:
      llvm_unreachable("unknown UnusualAnnotation on uav/srv");
      break;
    }
  }

  const RecordType *RT = VarTy->getAs<RecordType>();
  RecordDecl *RD = RT->getDecl();

  if (decl->hasAttr<HLSLGloballyCoherentAttr>()) {
    hlslRes->SetGloballyCoherent(true);
  }

  if (!SetUAVSRV(decl->getLocation(), resClass, hlslRes.get(), RD))
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
      if (isGlobalCB) {
        RegisterAssignment *ra = cast<RegisterAssignment>(it);
        offset = ra->RegisterNumber << 2;
        // Change to byte.
        offset <<= 2;
        userOffset = true;
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
  CB->SetLowerBound(UINT_MAX);
  if (!D->isCBuffer()) {
    CB->SetKind(DXIL::ResourceKind::TBuffer);
  }

  // the global variable will only used once by the createHandle?
  // SetHandle(llvm::Value *pHandle);

  for (hlsl::UnusualAnnotation *it : D->getUnusualAnnotations()) {
    switch (it->getKind()) {
    case hlsl::UnusualAnnotation::UA_RegisterAssignment: {
      hlsl::RegisterAssignment *ra = cast<hlsl::RegisterAssignment>(it);
      uint32_t regNum = ra->RegisterNumber;
      uint32_t regSpace = ra->RegisterSpace;
      CB->SetSpaceID(regSpace);
      CB->SetLowerBound(regNum);
      break;
    }
    case hlsl::UnusualAnnotation::UA_SemanticDecl:
      // skip semantic on constant buffer
      break;
    case hlsl::UnusualAnnotation::UA_ConstantPacking:
      llvm_unreachable("no packoffset on constant buffer");
      break;
    }
  }

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

bool CGMSHLSLRuntime::IsPatchConstantFunction(const Function *F) {
  DXASSERT_NOMSG(F != nullptr);
  for (auto && p : patchConstantFunctionMap) {
    if (p.second.Func == F) return true;
  }
  return false;
}

void CGMSHLSLRuntime::SetEntryFunction() {
  if (Entry.Func == nullptr) {
    DiagnosticsEngine &Diags = CGM.getDiags();
    unsigned DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error,
                                            "cannot find entry function %0");
    Diags.Report(DiagID) << CGM.getCodeGenOpts().HLSLEntryFunction;
    return;
  }

  m_pHLModule->SetEntryFunction(Entry.Func);
}

// Here the size is CB size. So don't need check type.
static unsigned AlignCBufferOffset(unsigned offset, unsigned size, llvm::Type *Ty) {
  DXASSERT(!(offset & 1), "otherwise we have an invalid offset.");
  bool bNeedNewRow = Ty->isArrayTy();
  unsigned scalarSizeInBytes = Ty->getScalarSizeInBits() / 8;

  return AlignBufferOffsetInLegacy(offset, size, scalarSizeInBytes, bNeedNewRow);
}

static unsigned AllocateDxilConstantBuffer(HLCBuffer &CB) {
  unsigned offset = 0;

  // Scan user allocated constants first.
  // Update offset.
  for (const std::unique_ptr<DxilResourceBase> &C : CB.GetConstants()) {
    if (C->GetLowerBound() == UINT_MAX)
      continue;
    unsigned size = C->GetRangeSize();
    unsigned nextOffset = size + C->GetLowerBound();
    if (offset < nextOffset)
      offset = nextOffset;
  }

  // Alloc after user allocated constants.
  for (const std::unique_ptr<DxilResourceBase> &C : CB.GetConstants()) {
    if (C->GetLowerBound() != UINT_MAX)
      continue;

    unsigned size = C->GetRangeSize();
    llvm::Type *Ty = C->GetGlobalSymbol()->getType()->getPointerElementType();
    // Align offset.
    offset = AlignCBufferOffset(offset, size, Ty);
    if (C->GetLowerBound() == UINT_MAX) {
      C->SetLowerBound(offset);
    }
    offset += size;
  }
  return offset;
}

static void AllocateDxilConstantBuffers(HLModule *pHLModule) {
  for (unsigned i = 0; i < pHLModule->GetCBuffers().size(); i++) {
    HLCBuffer &CB = *static_cast<HLCBuffer*>(&(pHLModule->GetCBuffer(i)));
    unsigned size = AllocateDxilConstantBuffer(CB);
    CB.SetSize(size);
  }
}

static void ReplaceUseInFunction(Value *V, Value *NewV, Function *F,
                                 IRBuilder<> &Builder) {
  for (auto U = V->user_begin(); U != V->user_end(); ) {
    User *user = *(U++);
    if (Instruction *I = dyn_cast<Instruction>(user)) {
      if (I->getParent()->getParent() == F) {
        // replace use with GEP if in F
        for (unsigned i = 0; i < I->getNumOperands(); i++) {
          if (I->getOperand(i) == V)
            I->setOperand(i, NewV);
        }
      }
    } else {
      // For constant operator, create local clone which use GEP.
      // Only support GEP and bitcast.
      if (GEPOperator *GEPOp = dyn_cast<GEPOperator>(user)) {
        std::vector<Value *> idxList(GEPOp->idx_begin(), GEPOp->idx_end());
        Value *NewGEP = Builder.CreateInBoundsGEP(NewV, idxList);
        ReplaceUseInFunction(GEPOp, NewGEP, F, Builder);
      } else if (GlobalVariable *GV = dyn_cast<GlobalVariable>(user)) {
        // Change the init val into NewV with Store.
        GV->setInitializer(nullptr);
        Builder.CreateStore(NewV, GV);
      } else {
        // Must be bitcast here.
        BitCastOperator *BC = cast<BitCastOperator>(user);
        Value *NewBC = Builder.CreateBitCast(NewV, BC->getType());
        ReplaceUseInFunction(BC, NewBC, F, Builder);
      }
    }
  }
}

void MarkUsedFunctionForConst(Value *V, std::unordered_set<Function*> &usedFunc) {
  for (auto U = V->user_begin(); U != V->user_end();) {
    User *user = *(U++);
    if (Instruction *I = dyn_cast<Instruction>(user)) {
      Function *F = I->getParent()->getParent();
      usedFunc.insert(F);
    } else {
      // For constant operator, create local clone which use GEP.
      // Only support GEP and bitcast.
      if (GEPOperator *GEPOp = dyn_cast<GEPOperator>(user)) {
        MarkUsedFunctionForConst(GEPOp, usedFunc);
      } else if (GlobalVariable *GV = dyn_cast<GlobalVariable>(user)) {
        MarkUsedFunctionForConst(GV, usedFunc);
      } else {
        // Must be bitcast here.
        BitCastOperator *BC = cast<BitCastOperator>(user);
        MarkUsedFunctionForConst(BC, usedFunc);
      }
    }
  }
}

static Function * GetOrCreateHLCreateHandle(HLModule &HLM, llvm::Type *HandleTy,
    ArrayRef<Value*> paramList, MDNode *MD) {
  SmallVector<llvm::Type *, 4> paramTyList;
  for (Value *param : paramList) {
    paramTyList.emplace_back(param->getType());
  }

  llvm::FunctionType *funcTy =
      llvm::FunctionType::get(HandleTy, paramTyList, false);
  llvm::Module &M = *HLM.GetModule();
  Function *CreateHandle = GetOrCreateHLFunctionWithBody(M, funcTy, HLOpcodeGroup::HLCreateHandle,
      /*opcode*/ 0, "");
  if (CreateHandle->empty()) {
    // Add body.
    BasicBlock *BB =
        BasicBlock::Create(CreateHandle->getContext(), "Entry", CreateHandle);
    IRBuilder<> Builder(BB);
    // Just return undef to make a body.
    Builder.CreateRet(UndefValue::get(HandleTy));
    // Mark resource attribute.
    HLM.MarkDxilResourceAttrib(CreateHandle, MD);
  }
  return CreateHandle;
}

static bool CreateCBufferVariable(HLCBuffer &CB,
    HLModule &HLM, llvm::Type *HandleTy) {
  bool bUsed = false;
  // Build Struct for CBuffer.
  SmallVector<llvm::Type*, 4> Elements;
  for (const std::unique_ptr<DxilResourceBase> &C : CB.GetConstants()) {
    Value *GV = C->GetGlobalSymbol();
    if (GV->hasNUsesOrMore(1))
      bUsed = true;
    // Global variable must be pointer type.
    llvm::Type *Ty = GV->getType()->getPointerElementType();
    Elements.emplace_back(Ty);
  }
  // Don't create CBuffer variable for unused cbuffer.
  if (!bUsed)
    return false;

  llvm::Module &M = *HLM.GetModule();

  bool isCBArray = CB.GetRangeSize() != 1;
  llvm::GlobalVariable *cbGV = nullptr;
  llvm::Type *cbTy = nullptr;

  unsigned cbIndexDepth = 0;
  if (!isCBArray) {
    llvm::StructType *CBStructTy =
        llvm::StructType::create(Elements, CB.GetGlobalName());
    cbGV = new llvm::GlobalVariable(M, CBStructTy, /*IsConstant*/ true,
                                    llvm::GlobalValue::ExternalLinkage,
                                    /*InitVal*/ nullptr, CB.GetGlobalName());
    cbTy = cbGV->getType();
  } else {
    // For array of ConstantBuffer, create array of struct instead of struct of
    // array.
    DXASSERT(CB.GetConstants().size() == 1,
             "ConstantBuffer should have 1 constant");
    Value *GV = CB.GetConstants()[0]->GetGlobalSymbol();
    llvm::Type *CBEltTy =
        GV->getType()->getPointerElementType()->getArrayElementType();
    cbIndexDepth = 1;
    while (CBEltTy->isArrayTy()) {
      CBEltTy = CBEltTy->getArrayElementType();
      cbIndexDepth++;
    }

    // Add one level struct type to match normal case.
    llvm::StructType *CBStructTy =
        llvm::StructType::create({CBEltTy}, CB.GetGlobalName());

    llvm::ArrayType *CBArrayTy =
        llvm::ArrayType::get(CBStructTy, CB.GetRangeSize());
    cbGV = new llvm::GlobalVariable(M, CBArrayTy, /*IsConstant*/ true,
                                    llvm::GlobalValue::ExternalLinkage,
                                    /*InitVal*/ nullptr, CB.GetGlobalName());

    cbTy = llvm::PointerType::get(CBStructTy,
                                  cbGV->getType()->getPointerAddressSpace());
  }

  CB.SetGlobalSymbol(cbGV);

  llvm::Type *opcodeTy = llvm::Type::getInt32Ty(M.getContext());
  llvm::Type *idxTy = opcodeTy;
  Constant *zeroIdx = ConstantInt::get(opcodeTy, 0);

  MDNode *MD = HLM.DxilCBufferToMDNode(CB);

  Value *HandleArgs[] = { zeroIdx, cbGV, zeroIdx };
  Function *CreateHandleFunc = GetOrCreateHLCreateHandle(HLM, HandleTy, HandleArgs, MD);

  llvm::FunctionType *SubscriptFuncTy =
      llvm::FunctionType::get(cbTy, { opcodeTy, HandleTy, idxTy}, false);

  Function *subscriptFunc =
      GetOrCreateHLFunction(M, SubscriptFuncTy, HLOpcodeGroup::HLSubscript,
                            (unsigned)HLSubscriptOpcode::CBufferSubscript);
  Constant *opArg = ConstantInt::get(opcodeTy, (unsigned)HLSubscriptOpcode::CBufferSubscript);
  Value *args[] = { opArg, nullptr, zeroIdx };

  llvm::LLVMContext &Context = M.getContext();
  llvm::Type *i32Ty = llvm::Type::getInt32Ty(Context);
  Value *zero = ConstantInt::get(i32Ty, (uint64_t)0);

  std::vector<Value *> indexArray(CB.GetConstants().size());
  std::vector<std::unordered_set<Function*>> constUsedFuncList(CB.GetConstants().size());

  for (const std::unique_ptr<DxilResourceBase> &C : CB.GetConstants()) {
    Value *idx = ConstantInt::get(i32Ty, C->GetID());
    indexArray[C->GetID()] = idx;

    Value *GV = C->GetGlobalSymbol();
    MarkUsedFunctionForConst(GV, constUsedFuncList[C->GetID()]);
  }

  for (Function &F : M.functions()) {
    if (F.isDeclaration())
      continue;

    if (GetHLOpcodeGroupByName(&F) != HLOpcodeGroup::NotHL)
      continue;

    IRBuilder<> Builder(F.getEntryBlock().getFirstInsertionPt());

    // create HL subscript to make all the use of cbuffer start from it.
    HandleArgs[HLOperandIndex::kCreateHandleResourceOpIdx] = cbGV;
    CallInst *Handle = Builder.CreateCall(CreateHandleFunc, HandleArgs);
    args[HLOperandIndex::kSubscriptObjectOpIdx] = Handle;
    Instruction *cbSubscript =
        cast<Instruction>(Builder.CreateCall(subscriptFunc, {args}));

    // Replace constant var with GEP pGV
    for (const std::unique_ptr<DxilResourceBase> &C : CB.GetConstants()) {
      Value *GV = C->GetGlobalSymbol();
      if (constUsedFuncList[C->GetID()].count(&F) == 0)
        continue;

      Value *idx = indexArray[C->GetID()];
      if (!isCBArray) {
        Instruction *GEP = cast<Instruction>(
            Builder.CreateInBoundsGEP(cbSubscript, {zero, idx}));
        // TODO: make sure the debug info is synced to GEP.
        // GEP->setDebugLoc(GV);
        ReplaceUseInFunction(GV, GEP, &F, Builder);
        // Delete if no use in F.
        if (GEP->user_empty())
          GEP->eraseFromParent();
      } else {
        for (auto U = GV->user_begin(); U != GV->user_end();) {
          User *user = *(U++);
          if (user->user_empty())
            continue;
          Instruction *I = dyn_cast<Instruction>(user);
          if (I && I->getParent()->getParent() != &F)
            continue;

          IRBuilder<> *instBuilder = &Builder;
          unique_ptr<IRBuilder<>> B;
          if (I) {
            B = llvm::make_unique<IRBuilder<>>(I);
            instBuilder = B.get();
          }

          GEPOperator *GEPOp = cast<GEPOperator>(user);
          std::vector<Value *> idxList;

          DXASSERT(GEPOp->getNumIndices() >= 1 + cbIndexDepth,
                   "must indexing ConstantBuffer array");
          idxList.reserve(GEPOp->getNumIndices() - (cbIndexDepth - 1));

          gep_type_iterator GI = gep_type_begin(*GEPOp),
                            E = gep_type_end(*GEPOp);
          idxList.push_back(GI.getOperand());
          // change array index with 0 for struct index.
          idxList.push_back(zero);
          GI++;
          Value *arrayIdx = GI.getOperand();
          GI++;
          for (unsigned curIndex = 1; GI != E && curIndex < cbIndexDepth;
               ++GI, ++curIndex) {
            arrayIdx = instBuilder->CreateMul(
                arrayIdx, Builder.getInt32(GI->getArrayNumElements()));
            arrayIdx = instBuilder->CreateAdd(arrayIdx, GI.getOperand());
          }

          for (; GI != E; ++GI) {
            idxList.push_back(GI.getOperand());
          }

          HandleArgs[HLOperandIndex::kCreateHandleIndexOpIdx] = arrayIdx;
          CallInst *Handle =
              instBuilder->CreateCall(CreateHandleFunc, HandleArgs);
          args[HLOperandIndex::kSubscriptObjectOpIdx] = Handle;
          args[HLOperandIndex::kSubscriptIndexOpIdx] = arrayIdx;

          Instruction *cbSubscript =
              cast<Instruction>(instBuilder->CreateCall(subscriptFunc, {args}));

          Instruction *NewGEP = cast<Instruction>(
              instBuilder->CreateInBoundsGEP(cbSubscript, idxList));

          ReplaceUseInFunction(GEPOp, NewGEP, &F, *instBuilder);
        }
      }
    }
    // Delete if no use in F.
    if (cbSubscript->user_empty()) {
      cbSubscript->eraseFromParent();
      Handle->eraseFromParent();
    } else {
      // merge GEP use for cbSubscript.
      HLModule::MergeGepUse(cbSubscript);
    }
  }
  return true;
}

static void ConstructCBufferAnnotation(
    HLCBuffer &CB, DxilTypeSystem &dxilTypeSys,
    std::unordered_map<Constant *, DxilFieldAnnotation> &AnnotationMap) {
  Value *GV = CB.GetGlobalSymbol();

  llvm::StructType *CBStructTy =
          dyn_cast<llvm::StructType>(GV->getType()->getPointerElementType());

  if (!CBStructTy) {
    // For Array of ConstantBuffer.
    llvm::ArrayType *CBArrayTy =
        cast<llvm::ArrayType>(GV->getType()->getPointerElementType());
    CBStructTy = cast<llvm::StructType>(CBArrayTy->getArrayElementType());
  }

  DxilStructAnnotation *CBAnnotation =
      dxilTypeSys.AddStructAnnotation(CBStructTy);
  CBAnnotation->SetCBufferSize(CB.GetSize());

  // Set fieldAnnotation for each constant var.
  for (const std::unique_ptr<DxilResourceBase> &C : CB.GetConstants()) {
    Constant *GV = C->GetGlobalSymbol();
    DxilFieldAnnotation &fieldAnnotation =
        CBAnnotation->GetFieldAnnotation(C->GetID());
    fieldAnnotation = AnnotationMap[GV];
    // This is after CBuffer allocation.
    fieldAnnotation.SetCBufferOffset(C->GetLowerBound());
    fieldAnnotation.SetFieldName(C->GetGlobalName());
  }
}

static void ConstructCBuffer(
    HLModule *pHLModule,
    llvm::Type *CBufferType,
    std::unordered_map<Constant *, DxilFieldAnnotation> &AnnotationMap) {
  DxilTypeSystem &dxilTypeSys = pHLModule->GetTypeSystem();
  llvm::Type *HandleTy = pHLModule->GetOP()->GetHandleType();
  for (unsigned i = 0; i < pHLModule->GetCBuffers().size(); i++) {
    HLCBuffer &CB = *static_cast<HLCBuffer*>(&(pHLModule->GetCBuffer(i)));
    if (CB.GetConstants().size() == 0) {
      // Create Fake variable for cbuffer which is empty.
      llvm::GlobalVariable *pGV = new llvm::GlobalVariable(
          *pHLModule->GetModule(), CBufferType, true,
          llvm::GlobalValue::ExternalLinkage, nullptr, CB.GetGlobalName());
      CB.SetGlobalSymbol(pGV);
    } else {
      bool bCreated =
          CreateCBufferVariable(CB, *pHLModule, HandleTy);
      if (bCreated)
        ConstructCBufferAnnotation(CB, dxilTypeSys, AnnotationMap);
      else {
        // Create Fake variable for cbuffer which is unused.
        llvm::GlobalVariable *pGV = new llvm::GlobalVariable(
            *pHLModule->GetModule(), CBufferType, true,
            llvm::GlobalValue::ExternalLinkage, nullptr, CB.GetGlobalName());
        CB.SetGlobalSymbol(pGV);
      }
    }
    // Clear the constants which useless now.
    CB.GetConstants().clear();
  }
}

static void ReplaceBoolVectorSubscript(CallInst *CI) {
  Value *Ptr = CI->getArgOperand(0);
  Value *Idx = CI->getArgOperand(1);
  Value *IdxList[] = {ConstantInt::get(Idx->getType(), 0), Idx};

  for (auto It = CI->user_begin(), E = CI->user_end(); It != E;) {
    Instruction *user = cast<Instruction>(*(It++));

    IRBuilder<> Builder(user);
    Value *GEP = Builder.CreateInBoundsGEP(Ptr, IdxList);

    if (LoadInst *LI = dyn_cast<LoadInst>(user)) {
      Value *NewLd = Builder.CreateLoad(GEP);
      Value *cast = Builder.CreateZExt(NewLd, LI->getType());
      LI->replaceAllUsesWith(cast);
      LI->eraseFromParent();
    } else {
      // Must be a store inst here.
      StoreInst *SI = cast<StoreInst>(user);
      Value *V = SI->getValueOperand();
      Value *cast =
          Builder.CreateICmpNE(V, llvm::ConstantInt::get(V->getType(), 0));
      Builder.CreateStore(cast, GEP);
      SI->eraseFromParent();
    }
  }
  CI->eraseFromParent();
}

static void ReplaceBoolVectorSubscript(Function *F) {
  for (auto It = F->user_begin(), E = F->user_end(); It != E; ) {
    User *user = *(It++);
    CallInst *CI = cast<CallInst>(user);
    ReplaceBoolVectorSubscript(CI);
  }
}

// Add function body for intrinsic if possible.
static Function *CreateOpFunction(llvm::Module &M, Function *F,
                                  llvm::FunctionType *funcTy,
                                  HLOpcodeGroup group, unsigned opcode) {
  Function *opFunc = nullptr;

  llvm::Type *opcodeTy = llvm::Type::getInt32Ty(M.getContext());
  if (group == HLOpcodeGroup::HLIntrinsic) {
    IntrinsicOp intriOp = static_cast<IntrinsicOp>(opcode);
    switch (intriOp) {
    case IntrinsicOp::MOP_Append: 
    case IntrinsicOp::MOP_Consume: {
      bool bAppend = intriOp == IntrinsicOp::MOP_Append;
      llvm::Type *handleTy = funcTy->getParamType(HLOperandIndex::kHandleOpIdx);
      // Don't generate body for OutputStream::Append.
      if (bAppend && HLModule::IsStreamOutputPtrType(handleTy)) {
        opFunc = GetOrCreateHLFunction(M, funcTy, group, opcode);
        break;
      }

      opFunc = GetOrCreateHLFunctionWithBody(M, funcTy, group, opcode,
                                             bAppend ? "append" : "consume");
      llvm::Type *counterTy = llvm::Type::getInt32Ty(M.getContext());
      llvm::FunctionType *IncCounterFuncTy =
          llvm::FunctionType::get(counterTy, {opcodeTy, handleTy}, false);
      unsigned counterOpcode = bAppend ? (unsigned)IntrinsicOp::MOP_IncrementCounter:
          (unsigned)IntrinsicOp::MOP_DecrementCounter;
      Function *incCounterFunc =
          GetOrCreateHLFunction(M, IncCounterFuncTy, group,
                                counterOpcode);

      llvm::Type *idxTy = counterTy;
      llvm::Type *valTy = bAppend ?
          funcTy->getParamType(HLOperandIndex::kAppendValOpIndex):funcTy->getReturnType();
      llvm::Type *subscriptTy = valTy;
      if (!valTy->isPointerTy()) {
        // Return type for subscript should be pointer type.
        subscriptTy = llvm::PointerType::get(valTy, 0);
      }

      llvm::FunctionType *SubscriptFuncTy =
          llvm::FunctionType::get(subscriptTy, {opcodeTy, handleTy, idxTy}, false);

      Function *subscriptFunc =
          GetOrCreateHLFunction(M, SubscriptFuncTy, HLOpcodeGroup::HLSubscript,
                                (unsigned)HLSubscriptOpcode::DefaultSubscript);

      BasicBlock *BB = BasicBlock::Create(opFunc->getContext(), "Entry", opFunc);
      IRBuilder<> Builder(BB);
      auto argIter = opFunc->args().begin();
      // Skip the opcode arg.
      argIter++;
      Argument *thisArg = argIter++;
      // int counter = IncrementCounter/DecrementCounter(Buf);
      Value *incCounterOpArg =
          ConstantInt::get(idxTy, counterOpcode);
      Value *counter =
          Builder.CreateCall(incCounterFunc, {incCounterOpArg, thisArg});
      // Buf[counter];
      Value *subscriptOpArg = ConstantInt::get(
          idxTy, (unsigned)HLSubscriptOpcode::DefaultSubscript);
      Value *subscript =
          Builder.CreateCall(subscriptFunc, {subscriptOpArg, thisArg, counter});

      if (bAppend) {
        Argument *valArg = argIter;
        // Buf[counter] = val;
        if (valTy->isPointerTy()) {
          unsigned size = M.getDataLayout().getTypeAllocSize(subscript->getType()->getPointerElementType());
          Builder.CreateMemCpy(subscript, valArg, size, 1);
        } else
          Builder.CreateStore(valArg, subscript);
        Builder.CreateRetVoid();
      } else {
        // return Buf[counter];
        if (valTy->isPointerTy())
          Builder.CreateRet(subscript);
        else {
          Value *retVal = Builder.CreateLoad(subscript);
          Builder.CreateRet(retVal);
        }
      }
    } break;
    case IntrinsicOp::IOP_sincos: {
      opFunc = GetOrCreateHLFunctionWithBody(M, funcTy, group, opcode, "sincos");
      llvm::Type *valTy = funcTy->getParamType(HLOperandIndex::kTrinaryOpSrc0Idx);

      llvm::FunctionType *sinFuncTy =
          llvm::FunctionType::get(valTy, {opcodeTy, valTy}, false);
      unsigned sinOp = static_cast<unsigned>(IntrinsicOp::IOP_sin);
      unsigned cosOp = static_cast<unsigned>(IntrinsicOp::IOP_cos);
      Function *sinFunc = GetOrCreateHLFunction(M, sinFuncTy, group, sinOp);
      Function *cosFunc = GetOrCreateHLFunction(M, sinFuncTy, group, cosOp);

      BasicBlock *BB = BasicBlock::Create(opFunc->getContext(), "Entry", opFunc);
      IRBuilder<> Builder(BB);
      auto argIter = opFunc->args().begin();
      // Skip the opcode arg.
      argIter++;
      Argument *valArg = argIter++;
      Argument *sinPtrArg = argIter++;
      Argument *cosPtrArg = argIter++;

      Value *sinOpArg =
          ConstantInt::get(opcodeTy, sinOp);
      Value *sinVal = Builder.CreateCall(sinFunc, {sinOpArg, valArg});
      Builder.CreateStore(sinVal, sinPtrArg);

      Value *cosOpArg =
          ConstantInt::get(opcodeTy, cosOp);
      Value *cosVal = Builder.CreateCall(cosFunc, {cosOpArg, valArg});
      Builder.CreateStore(cosVal, cosPtrArg);
      // Ret.
      Builder.CreateRetVoid();
    } break;
    default:
      opFunc = GetOrCreateHLFunction(M, funcTy, group, opcode);
      break;
    }
  }
  else if (group == HLOpcodeGroup::HLExtIntrinsic) {
    llvm::StringRef fnName = F->getName();
    llvm::StringRef groupName = GetHLOpcodeGroupNameByAttr(F);
    opFunc = GetOrCreateHLFunction(M, funcTy, group, &groupName, &fnName, opcode);
  }
  else {
    opFunc = GetOrCreateHLFunction(M, funcTy, group, opcode);
  }

  // Add attribute
  if (F->hasFnAttribute(Attribute::ReadNone))
    opFunc->addFnAttr(Attribute::ReadNone);
  if (F->hasFnAttribute(Attribute::ReadOnly))
    opFunc->addFnAttr(Attribute::ReadOnly);
  return opFunc;
}

static Value *CreateHandleFromResPtr(
    Value *ResPtr, HLModule &HLM, llvm::Type *HandleTy,
    std::unordered_map<llvm::Type *, MDNode *> &resMetaMap,
    IRBuilder<> &Builder) {
  llvm::Type *objTy = ResPtr->getType()->getPointerElementType();
  DXASSERT(resMetaMap.count(objTy), "cannot find resource type");
  MDNode *MD = resMetaMap[objTy];
  // Load to make sure resource only have Ld/St use so mem2reg could remove
  // temp resource.
  Value *ldObj = Builder.CreateLoad(ResPtr);
  Value *opcode = Builder.getInt32(0);
  Value *args[] = {opcode, ldObj};
  Function *CreateHandle = GetOrCreateHLCreateHandle(HLM, HandleTy, args, MD);
  CallInst *Handle = Builder.CreateCall(CreateHandle, args);
  return Handle;
}

static void AddOpcodeParamForIntrinsic(HLModule &HLM, Function *F,
                                       unsigned opcode, llvm::Type *HandleTy,
    std::unordered_map<llvm::Type *, MDNode*> &resMetaMap) {
  llvm::Module &M = *HLM.GetModule();
  llvm::FunctionType *oldFuncTy = F->getFunctionType();

  SmallVector<llvm::Type *, 4> paramTyList;
  // Add the opcode param
  llvm::Type *opcodeTy = llvm::Type::getInt32Ty(M.getContext());
  paramTyList.emplace_back(opcodeTy);
  paramTyList.append(oldFuncTy->param_begin(), oldFuncTy->param_end());

  for (unsigned i = 1; i < paramTyList.size(); i++) {
    llvm::Type *Ty = paramTyList[i];
    if (Ty->isPointerTy()) {
      Ty = Ty->getPointerElementType();
      if (HLModule::IsHLSLObjectType(Ty) &&
          // StreamOutput don't need handle.
          !HLModule::IsStreamOutputType(Ty)) {
        // Use handle type for object type.
        // This will make sure temp object variable only used by createHandle.
        paramTyList[i] = HandleTy;
      }
    }
  }

  HLOpcodeGroup group = hlsl::GetHLOpcodeGroup(F);

  if (group == HLOpcodeGroup::HLSubscript &&
      opcode == static_cast<unsigned>(HLSubscriptOpcode::VectorSubscript)) {
    llvm::FunctionType *FT = F->getFunctionType();
    llvm::Type *VecArgTy = FT->getParamType(0);
    llvm::VectorType *VType =
        cast<llvm::VectorType>(VecArgTy->getPointerElementType());
    llvm::Type *Ty = VType->getElementType();
    DXASSERT(Ty->isIntegerTy(), "Only bool could use VectorSubscript");
    llvm::IntegerType *ITy = cast<IntegerType>(Ty);

    DXASSERT_LOCALVAR(ITy, ITy->getBitWidth() == 1, "Only bool could use VectorSubscript");

    // The return type is i8*.
    // Replace all uses with i1*.
    ReplaceBoolVectorSubscript(F);
    return;
  }

  bool isDoubleSubscriptFunc = group == HLOpcodeGroup::HLSubscript &&
      opcode == static_cast<unsigned>(HLSubscriptOpcode::DoubleSubscript);

  llvm::Type *RetTy = oldFuncTy->getReturnType();

  if (isDoubleSubscriptFunc) {
    CallInst *doubleSub = cast<CallInst>(*F->user_begin());
   
    // Change currentIdx type into coord type.
    auto U = doubleSub->user_begin();
    Value *user = *U;
    CallInst *secSub = cast<CallInst>(user);
    unsigned coordIdx = HLOperandIndex::kSubscriptIndexOpIdx;
    // opcode operand not add yet, so the index need -1.
    if (GetHLOpcodeGroupByName(secSub->getCalledFunction()) == HLOpcodeGroup::NotHL)
      coordIdx -= 1;
    
    Value *coord = secSub->getArgOperand(coordIdx);

    llvm::Type *coordTy = coord->getType();
    paramTyList[HLOperandIndex::kSubscriptIndexOpIdx] = coordTy;
    // Add the sampleIdx or mipLevel parameter to the end.
    paramTyList.emplace_back(opcodeTy);
    // Change return type to be resource ret type.
    // opcode operand not add yet, so the index need -1.
    Value *objPtr = doubleSub->getArgOperand(HLOperandIndex::kSubscriptObjectOpIdx-1);
    // Must be a GEP
    GEPOperator *objGEP = cast<GEPOperator>(objPtr);
    gep_type_iterator GEPIt = gep_type_begin(objGEP), E = gep_type_end(objGEP);
    llvm::Type *resTy = nullptr;
    while (GEPIt != E) {
      if (HLModule::IsHLSLObjectType(*GEPIt)) {
        resTy = *GEPIt;
        break;
      }
      GEPIt++;
    }

    DXASSERT(resTy, "must find the resource type");
    // Change object type to handle type.
    paramTyList[HLOperandIndex::kSubscriptObjectOpIdx] = HandleTy;
    // Change RetTy into pointer of resource reture type.
    RetTy = cast<StructType>(resTy)->getElementType(0)->getPointerTo();

    llvm::Type *sliceTy = objGEP->getType()->getPointerElementType();
    DXIL::ResourceClass RC = HLM.GetResourceClass(sliceTy);
    DXIL::ResourceKind RK = HLM.GetResourceKind(sliceTy);
    HLM.AddResourceTypeAnnotation(resTy, RC, RK);
  }

  llvm::FunctionType *funcTy =
      llvm::FunctionType::get(RetTy, paramTyList, false);

  Function *opFunc = CreateOpFunction(M, F, funcTy, group, opcode);
  StringRef lower = hlsl::GetHLLowerStrategy(F);
  if (!lower.empty())
    hlsl::SetHLLowerStrategy(opFunc, lower);

  for (auto user = F->user_begin(); user != F->user_end();) {
    // User must be a call.
    CallInst *oldCI = cast<CallInst>(*(user++));

    SmallVector<Value *, 4> opcodeParamList;
    Value *opcodeConst = Constant::getIntegerValue(opcodeTy, APInt(32, opcode));
    opcodeParamList.emplace_back(opcodeConst);

    opcodeParamList.append(oldCI->arg_operands().begin(),
                           oldCI->arg_operands().end());
    IRBuilder<> Builder(oldCI);

    if (isDoubleSubscriptFunc) {
      // Change obj to the resource pointer.
      Value *objVal = opcodeParamList[HLOperandIndex::kSubscriptObjectOpIdx];
      GEPOperator *objGEP = cast<GEPOperator>(objVal);
      SmallVector<Value *, 8> IndexList;
      IndexList.append(objGEP->idx_begin(), objGEP->idx_end());
      Value *lastIndex = IndexList.back();
      ConstantInt *constIndex = cast<ConstantInt>(lastIndex);
      DXASSERT_LOCALVAR(constIndex, constIndex->getLimitedValue() == 1, "last index must 1");
      // Remove the last index.
      IndexList.pop_back();
      objVal = objGEP->getPointerOperand();
      if (IndexList.size() > 1)
        objVal = Builder.CreateInBoundsGEP(objVal, IndexList);

      Value *Handle =
          CreateHandleFromResPtr(objVal, HLM, HandleTy, resMetaMap, Builder);
      // Change obj to the resource pointer.
      opcodeParamList[HLOperandIndex::kSubscriptObjectOpIdx] = Handle;

      // Set idx and mipIdx.
      Value *mipIdx = opcodeParamList[HLOperandIndex::kSubscriptIndexOpIdx];
      auto U = oldCI->user_begin();
      Value *user = *U;
      CallInst *secSub = cast<CallInst>(user);
      unsigned idxOpIndex = HLOperandIndex::kSubscriptIndexOpIdx;
      if (GetHLOpcodeGroupByName(secSub->getCalledFunction()) == HLOpcodeGroup::NotHL)
        idxOpIndex--;
      Value *idx = secSub->getArgOperand(idxOpIndex);

      DXASSERT(secSub->hasOneUse(), "subscript should only has one use");

      // Add the sampleIdx or mipLevel parameter to the end.
      opcodeParamList[HLOperandIndex::kSubscriptIndexOpIdx] = idx;
      opcodeParamList.emplace_back(mipIdx);
      // Insert new call before secSub to make sure idx is ready to use.
      Builder.SetInsertPoint(secSub);
    }

    for (unsigned i = 1; i < opcodeParamList.size(); i++) {
      Value *arg = opcodeParamList[i];
      llvm::Type *Ty = arg->getType();
      if (Ty->isPointerTy()) {
        Ty = Ty->getPointerElementType();
        if (HLModule::IsHLSLObjectType(Ty) &&
          // StreamOutput don't need handle.
          !HLModule::IsStreamOutputType(Ty)) {
          // Use object type directly, not by pointer.
          // This will make sure temp object variable only used by ld/st.
          if (GEPOperator *argGEP = dyn_cast<GEPOperator>(arg)) {
            std::vector<Value*> idxList(argGEP->idx_begin(), argGEP->idx_end());
            // Create instruction to avoid GEPOperator.
            GetElementPtrInst *GEP = GetElementPtrInst::CreateInBounds(argGEP->getPointerOperand(), 
                idxList);
            Builder.Insert(GEP);
            arg = GEP;
          }
          Value *Handle = CreateHandleFromResPtr(arg, HLM, HandleTy,
                                                 resMetaMap, Builder);
          opcodeParamList[i] = Handle;
        }
      }
    }

    Value *CI = Builder.CreateCall(opFunc, opcodeParamList);
    if (!isDoubleSubscriptFunc) {
      // replace new call and delete the old call
      oldCI->replaceAllUsesWith(CI);
      oldCI->eraseFromParent();
    } else {
      // For double script.
      // Replace single users use with new CI.
      auto U = oldCI->user_begin();
      Value *user = *U;
      CallInst *secSub = cast<CallInst>(user);
      secSub->replaceAllUsesWith(CI);
      secSub->eraseFromParent();
      oldCI->eraseFromParent();
    }
  }
  // delete the function
  F->eraseFromParent();
}

static void AddOpcodeParamForIntrinsics(HLModule &HLM
    , std::vector<std::pair<Function *, unsigned>> &intrinsicMap,
    std::unordered_map<llvm::Type *, MDNode*> &resMetaMap) {
  llvm::Type *HandleTy = HLM.GetOP()->GetHandleType();
  for (auto mapIter : intrinsicMap) {
    Function *F = mapIter.first;
    if (F->user_empty()) {
      // delete the function
      F->eraseFromParent();
      continue;
    }

    unsigned opcode = mapIter.second;
    AddOpcodeParamForIntrinsic(HLM, F, opcode, HandleTy, resMetaMap);
  }
}

static Value *CastLdValue(Value *Ptr, llvm::Type *FromTy, llvm::Type *ToTy, IRBuilder<> &Builder) {
  if (ToTy->isVectorTy()) {
    unsigned vecSize = ToTy->getVectorNumElements();
    if (vecSize == 1 && ToTy->getVectorElementType() == FromTy) {
      Value *V = Builder.CreateLoad(Ptr);
      // ScalarToVec1Splat
      // Change scalar into vec1.
      Value *Vec1 = UndefValue::get(ToTy);
      return Builder.CreateInsertElement(Vec1, V, (uint64_t)0);
    } else if (FromTy->isVectorTy() && vecSize == 1) {
      Value *V = Builder.CreateLoad(Ptr);
      // VectorTrunc
      // Change vector into vec1.
      int mask[] = {0};
      return Builder.CreateShuffleVector(V, V, mask);
    } else if (FromTy->isArrayTy()) {
      llvm::Type *FromEltTy = FromTy->getArrayElementType();

      llvm::Type *ToEltTy = ToTy->getVectorElementType();
      if (FromTy->getArrayNumElements() == vecSize && FromEltTy == ToEltTy) {
        // ArrayToVector.
        Value *NewLd = UndefValue::get(ToTy);
        Value *zeroIdx = Builder.getInt32(0);
        for (unsigned i = 0; i < vecSize; i++) {
          Value *GEP = Builder.CreateInBoundsGEP(
              Ptr, {zeroIdx, Builder.getInt32(i)});
          Value *Elt = Builder.CreateLoad(GEP);
          NewLd = Builder.CreateInsertElement(NewLd, Elt, i);
        }
        return NewLd;
      }
    }
  } else if (FromTy == Builder.getInt1Ty()) {
    Value *V = Builder.CreateLoad(Ptr);
    // BoolCast
    DXASSERT_NOMSG(ToTy->isIntegerTy());
    return Builder.CreateZExt(V, ToTy);
  }

  return nullptr;
}

static Value  *CastStValue(Value *Ptr, Value *V, llvm::Type *FromTy, llvm::Type *ToTy, IRBuilder<> &Builder) {
  if (ToTy->isVectorTy()) {
    unsigned vecSize = ToTy->getVectorNumElements();
    if (vecSize == 1 && ToTy->getVectorElementType() == FromTy) {
      // ScalarToVec1Splat
      // Change vec1 back to scalar.
      Value *Elt = Builder.CreateExtractElement(V, (uint64_t)0);
      return Elt;
    } else if (FromTy->isVectorTy() && vecSize == 1) {
      // VectorTrunc
      // Change vec1 into vector.
      // Should not happen.
      // Reported error at Sema::ImpCastExprToType.
      DXASSERT_NOMSG(0);
    } else if (FromTy->isArrayTy()) {
      llvm::Type *FromEltTy = FromTy->getArrayElementType();

      llvm::Type *ToEltTy = ToTy->getVectorElementType();
      if (FromTy->getArrayNumElements() == vecSize && FromEltTy == ToEltTy) {
        // ArrayToVector.
        Value *zeroIdx = Builder.getInt32(0);
        for (unsigned i = 0; i < vecSize; i++) {
          Value *Elt = Builder.CreateExtractElement(V, i);
          Value *GEP = Builder.CreateInBoundsGEP(
              Ptr, {zeroIdx, Builder.getInt32(i)});
          Builder.CreateStore(Elt, GEP);
        }
        // The store already done.
        // Return null to ignore use of the return value.
        return nullptr;
      }
    }
  } else if (FromTy == Builder.getInt1Ty()) {
    // BoolCast
    // Change i1 to ToTy.
    DXASSERT_NOMSG(ToTy->isIntegerTy());
    Value *CastV = Builder.CreateICmpNE(V, ConstantInt::get(V->getType(), 0));
    return CastV;
  }

  return nullptr;
}

static bool SimplifyBitCastLoad(LoadInst *LI, llvm::Type *FromTy, llvm::Type *ToTy, Value *Ptr) {
  IRBuilder<> Builder(LI);
  // Cast FromLd to ToTy.
  Value *CastV = CastLdValue(Ptr, FromTy, ToTy, Builder);
  if (CastV) {
    LI->replaceAllUsesWith(CastV);
    return true;
  } else {
    return false;
  }
}

static bool SimplifyBitCastStore(StoreInst *SI, llvm::Type *FromTy, llvm::Type *ToTy, Value *Ptr) {
  IRBuilder<> Builder(SI);
  Value *V = SI->getValueOperand();
  // Cast Val to FromTy.
  Value *CastV = CastStValue(Ptr, V, FromTy, ToTy, Builder);
  if (CastV) {
    Builder.CreateStore(CastV, Ptr);
    return true;
  } else {
    return false;
  }
}

static bool SimplifyBitCastGEP(GEPOperator *GEP, llvm::Type *FromTy, llvm::Type *ToTy, Value *Ptr) {
  if (ToTy->isVectorTy()) {
    unsigned vecSize = ToTy->getVectorNumElements();
    if (vecSize == 1 && ToTy->getVectorElementType() == FromTy) {
      // ScalarToVec1Splat
      GEP->replaceAllUsesWith(Ptr);
      return true;
    } else if (FromTy->isVectorTy() && vecSize == 1) {
      // VectorTrunc
      DXASSERT_NOMSG(
          !isa<llvm::VectorType>(GEP->getType()->getPointerElementType()));
      IRBuilder<> Builder(FromTy->getContext());
      if (Instruction *I = dyn_cast<Instruction>(GEP))
        Builder.SetInsertPoint(I);
      std::vector<Value *> idxList(GEP->idx_begin(), GEP->idx_end());
      Value *NewGEP = Builder.CreateInBoundsGEP(Ptr, idxList);
      GEP->replaceAllUsesWith(NewGEP);
      return true;
    } else if (FromTy->isArrayTy()) {
      llvm::Type *FromEltTy = FromTy->getArrayElementType();

      llvm::Type *ToEltTy = ToTy->getVectorElementType();
      if (FromTy->getArrayNumElements() == vecSize && FromEltTy == ToEltTy) {
        // ArrayToVector.
      }
    }
  } else if (FromTy == llvm::Type::getInt1Ty(FromTy->getContext())) {
    // BoolCast
  }
  return false;
}
typedef SmallPtrSet<Instruction *, 4> SmallInstSet;
static void SimplifyBitCast(BitCastOperator *BC, SmallInstSet &deadInsts) {
  Value *Ptr = BC->getOperand(0);
  llvm::Type *FromTy = Ptr->getType();
  llvm::Type *ToTy = BC->getType();

  if (!FromTy->isPointerTy() || !ToTy->isPointerTy())
    return;

  FromTy = FromTy->getPointerElementType();
  ToTy = ToTy->getPointerElementType();
  // Take care case like %2 = bitcast %struct.T* %1 to <1 x float>*.
  if (FromTy->isStructTy()) {
    IRBuilder<> Builder(FromTy->getContext());
    if (Instruction *I = dyn_cast<Instruction>(BC))
      Builder.SetInsertPoint(I);

    Value *zeroIdx = Builder.getInt32(0);
    unsigned nestLevel = 1;
    while (llvm::StructType *ST = dyn_cast<llvm::StructType>(FromTy)) {
      FromTy = ST->getElementType(0);
      nestLevel++;
    }
    std::vector<Value *> idxList(nestLevel, zeroIdx);
    Ptr = Builder.CreateGEP(Ptr, idxList);
  }

  for (User *U : BC->users()) {
    if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
      if (SimplifyBitCastLoad(LI, FromTy, ToTy, Ptr)) {
        LI->dropAllReferences();
        deadInsts.insert(LI);
      }
    } else if (StoreInst *SI = dyn_cast<StoreInst>(U)) {
      if (SimplifyBitCastStore(SI, FromTy, ToTy, Ptr)) {
        SI->dropAllReferences();
        deadInsts.insert(SI);
      }
    } else if (GEPOperator *GEP = dyn_cast<GEPOperator>(U)) {
      if (SimplifyBitCastGEP(GEP, FromTy, ToTy, Ptr))
        if (Instruction *I = dyn_cast<Instruction>(GEP)) {
          I->dropAllReferences();
          deadInsts.insert(I);
        }
    } else if (dyn_cast<CallInst>(U)) {
      // Skip function call.
    } else if (dyn_cast<BitCastInst>(U)) {
      // Skip bitcast.
    } else {
      DXASSERT(0, "not support yet");
    }
  }
}

typedef float(__cdecl *FloatUnaryEvalFuncType)(float);
typedef double(__cdecl *DoubleUnaryEvalFuncType)(double);

typedef float(__cdecl *FloatBinaryEvalFuncType)(float, float);
typedef double(__cdecl *DoubleBinaryEvalFuncType)(double, double);

static Value * EvalUnaryIntrinsic(ConstantFP *fpV,
                               FloatUnaryEvalFuncType floatEvalFunc,
                               DoubleUnaryEvalFuncType doubleEvalFunc) {
  llvm::Type *Ty = fpV->getType();
  Value *Result = nullptr;
  if (Ty->isDoubleTy()) {
    double dV = fpV->getValueAPF().convertToDouble();
    Value *dResult = ConstantFP::get(Ty, doubleEvalFunc(dV));
    Result = dResult;
  } else {
    DXASSERT_NOMSG(Ty->isFloatTy());
    float fV = fpV->getValueAPF().convertToFloat();
    Value *dResult = ConstantFP::get(Ty, floatEvalFunc(fV));
    Result = dResult;
  }
  return Result;
}

static Value * EvalBinaryIntrinsic(ConstantFP *fpV0, ConstantFP *fpV1,
                               FloatBinaryEvalFuncType floatEvalFunc,
                               DoubleBinaryEvalFuncType doubleEvalFunc) {
  llvm::Type *Ty = fpV0->getType();
  Value *Result = nullptr;
  if (Ty->isDoubleTy()) {
    double dV0 = fpV0->getValueAPF().convertToDouble();
    double dV1 = fpV1->getValueAPF().convertToDouble();
    Value *dResult = ConstantFP::get(Ty, doubleEvalFunc(dV0, dV1));
    Result = dResult;
  } else {
    DXASSERT_NOMSG(Ty->isFloatTy());
    float fV0 = fpV0->getValueAPF().convertToFloat();
    float fV1 = fpV1->getValueAPF().convertToFloat();
    Value *dResult = ConstantFP::get(Ty, floatEvalFunc(fV0, fV1));
    Result = dResult;
  }
  return Result;
}

static Value * EvalUnaryIntrinsic(CallInst *CI,
                               FloatUnaryEvalFuncType floatEvalFunc,
                               DoubleUnaryEvalFuncType doubleEvalFunc) {
  Value *V = CI->getArgOperand(0);
  llvm::Type *Ty = CI->getType();
  Value *Result = nullptr;
  if (llvm::VectorType *VT = dyn_cast<llvm::VectorType>(Ty)) {
    Result = UndefValue::get(Ty);
    Constant *CV = cast<Constant>(V);
    IRBuilder<> Builder(CI);
    for (unsigned i=0;i<VT->getNumElements();i++) {
      ConstantFP *fpV = cast<ConstantFP>(CV->getAggregateElement(i));
      Value *EltResult = EvalUnaryIntrinsic(fpV, floatEvalFunc, doubleEvalFunc);
      Result = Builder.CreateInsertElement(Result, EltResult, i);
    }
  } else {
    ConstantFP *fpV = cast<ConstantFP>(V);
    Result = EvalUnaryIntrinsic(fpV, floatEvalFunc, doubleEvalFunc);
  }
  CI->replaceAllUsesWith(Result);
  CI->eraseFromParent();
  return Result;
}

static Value * EvalBinaryIntrinsic(CallInst *CI,
                               FloatBinaryEvalFuncType floatEvalFunc,
                               DoubleBinaryEvalFuncType doubleEvalFunc) {
  Value *V0 = CI->getArgOperand(0);
  Value *V1 = CI->getArgOperand(1);
  llvm::Type *Ty = CI->getType();
  Value *Result = nullptr;
  if (llvm::VectorType *VT = dyn_cast<llvm::VectorType>(Ty)) {
    Result = UndefValue::get(Ty);
    Constant *CV0 = cast<Constant>(V0);
    Constant *CV1 = cast<Constant>(V1);
    IRBuilder<> Builder(CI);
    for (unsigned i=0;i<VT->getNumElements();i++) {
      ConstantFP *fpV0 = cast<ConstantFP>(CV0->getAggregateElement(i));
      ConstantFP *fpV1 = cast<ConstantFP>(CV1->getAggregateElement(i));
      Value *EltResult = EvalBinaryIntrinsic(fpV0, fpV1, floatEvalFunc, doubleEvalFunc);
      Result = Builder.CreateInsertElement(Result, EltResult, i);
    }
  } else {
    ConstantFP *fpV0 = cast<ConstantFP>(V0);
    ConstantFP *fpV1 = cast<ConstantFP>(V1);
    Result = EvalBinaryIntrinsic(fpV0, fpV1, floatEvalFunc, doubleEvalFunc);
  }
  CI->replaceAllUsesWith(Result);
  CI->eraseFromParent();
  return Result;

  CI->eraseFromParent();
  return Result;
}

static Value * TryEvalIntrinsic(CallInst *CI, IntrinsicOp intriOp) {
  switch (intriOp) {
  case IntrinsicOp::IOP_tan: {
    return EvalUnaryIntrinsic(CI, tanf, tan);
  } break;
  case IntrinsicOp::IOP_tanh: {
    return EvalUnaryIntrinsic(CI, tanhf, tanh);
  } break;
  case IntrinsicOp::IOP_sin: {
    return EvalUnaryIntrinsic(CI, sinf, sin);
  } break;
  case IntrinsicOp::IOP_sinh: {
    return EvalUnaryIntrinsic(CI, sinhf, sinh);
  } break;
  case IntrinsicOp::IOP_cos: {
    return EvalUnaryIntrinsic(CI, cosf, cos);
  } break;
  case IntrinsicOp::IOP_cosh: {
    return EvalUnaryIntrinsic(CI, coshf, cosh);
  } break;
  case IntrinsicOp::IOP_asin: {
    return EvalUnaryIntrinsic(CI, asinf, asin);
  } break;
  case IntrinsicOp::IOP_acos: {
    return EvalUnaryIntrinsic(CI, acosf, acos);
  } break;
  case IntrinsicOp::IOP_atan: {
    return EvalUnaryIntrinsic(CI, atanf, atan);
  } break;
  case IntrinsicOp::IOP_atan2: {
    Value *V0 = CI->getArgOperand(0);
    ConstantFP *fpV0 = cast<ConstantFP>(V0);

    Value *V1 = CI->getArgOperand(1);
    ConstantFP *fpV1 = cast<ConstantFP>(V1);

    llvm::Type *Ty = CI->getType();
    Value *Result = nullptr;
    if (Ty->isDoubleTy()) {
      double dV0 = fpV0->getValueAPF().convertToDouble();
      double dV1 = fpV1->getValueAPF().convertToDouble();
      Value *atanV = ConstantFP::get(CI->getType(), atan(dV0 / dV1));
      CI->replaceAllUsesWith(atanV);
      Result = atanV;
    } else {
      DXASSERT_NOMSG(Ty->isFloatTy());
      float fV0 = fpV0->getValueAPF().convertToFloat();
      float fV1 = fpV1->getValueAPF().convertToFloat();
      Value *atanV = ConstantFP::get(CI->getType(), atanf(fV0 / fV1));
      CI->replaceAllUsesWith(atanV);
      Result = atanV;
    }
    CI->eraseFromParent();
    return Result;
  } break;
  case IntrinsicOp::IOP_sqrt: {
    return EvalUnaryIntrinsic(CI, sqrtf, sqrt);
  } break;
  case IntrinsicOp::IOP_rsqrt: {
    auto rsqrtF = [](float v) -> float { return 1.0 / sqrtf(v); };
    auto rsqrtD = [](double v) -> double { return 1.0 / sqrt(v); };

    return EvalUnaryIntrinsic(CI, rsqrtF, rsqrtD);
  } break;
  case IntrinsicOp::IOP_exp: {
    return EvalUnaryIntrinsic(CI, expf, exp);
  } break;
  case IntrinsicOp::IOP_exp2: {
    return EvalUnaryIntrinsic(CI, exp2f, exp2);
  } break;
  case IntrinsicOp::IOP_log: {
    return EvalUnaryIntrinsic(CI, logf, log);
  } break;
  case IntrinsicOp::IOP_log10: {
    return EvalUnaryIntrinsic(CI, log10f, log10);
  } break;
  case IntrinsicOp::IOP_log2: {
    return EvalUnaryIntrinsic(CI, log2f, log2);
  } break;
  case IntrinsicOp::IOP_pow: {
    return EvalBinaryIntrinsic(CI, powf, pow);
  } break;
  case IntrinsicOp::IOP_max: {
    auto maxF = [](float a, float b) -> float { return a > b ? a:b; };
    auto maxD = [](double a, double b) -> double { return a > b ? a:b; };
    return EvalBinaryIntrinsic(CI, maxF, maxD);
  } break;
  case IntrinsicOp::IOP_min: {
    auto minF = [](float a, float b) -> float { return a < b ? a:b; };
    auto minD = [](double a, double b) -> double { return a < b ? a:b; };
    return EvalBinaryIntrinsic(CI, minF, minD);
  } break;
  case IntrinsicOp::IOP_rcp: {
    auto rcpF = [](float v) -> float { return 1.0 / v; };
    auto rcpD = [](double v) -> double { return 1.0 / v; };

    return EvalUnaryIntrinsic(CI, rcpF, rcpD);
  } break;
  case IntrinsicOp::IOP_ceil: {
    return EvalUnaryIntrinsic(CI, ceilf, ceil);
  } break;
  case IntrinsicOp::IOP_floor: {
    return EvalUnaryIntrinsic(CI, floorf, floor);
  } break;
  case IntrinsicOp::IOP_round: {
    return EvalUnaryIntrinsic(CI, roundf, round);
  } break;
  case IntrinsicOp::IOP_trunc: {
    return EvalUnaryIntrinsic(CI, truncf, trunc);
  } break;
  case IntrinsicOp::IOP_frac: {
    auto fracF = [](float v) -> float {
      int exp = 0;
      return frexpf(v, &exp);
    };
    auto fracD = [](double v) -> double {
      int exp = 0;
      return frexp(v, &exp);
    };

    return EvalUnaryIntrinsic(CI, fracF, fracD);
  } break;
  case IntrinsicOp::IOP_isnan: {
    Value *V = CI->getArgOperand(0);
    ConstantFP *fV = cast<ConstantFP>(V);
    bool isNan = fV->getValueAPF().isNaN();
    Constant *cNan = ConstantInt::get(CI->getType(), isNan ? 1 : 0);
    CI->replaceAllUsesWith(cNan);
    CI->eraseFromParent();
    return cNan;
  } break;
  default:
    return nullptr;
  }
}

static void SimpleTransformForHLDXIR(Instruction *I,
                                     SmallInstSet &deadInsts) {

  unsigned opcode = I->getOpcode();
  switch (opcode) {
  case Instruction::BitCast: {
    BitCastOperator *BCI = cast<BitCastOperator>(I);
    SimplifyBitCast(BCI, deadInsts);
  } break;
  case Instruction::Load: {
    LoadInst *ldInst = cast<LoadInst>(I);
    DXASSERT(!HLMatrixLower::IsMatrixType(ldInst->getType()),
                      "matrix load should use HL LdStMatrix");
    Value *Ptr = ldInst->getPointerOperand();
    if (ConstantExpr *CE = dyn_cast_or_null<ConstantExpr>(Ptr)) {
      if (BitCastOperator *BCO = dyn_cast<BitCastOperator>(CE)) {
        SimplifyBitCast(BCO, deadInsts);
      }
    }
  } break;
  case Instruction::Store: {
    StoreInst *stInst = cast<StoreInst>(I);
    Value *V = stInst->getValueOperand();
    DXASSERT_LOCALVAR(V, !HLMatrixLower::IsMatrixType(V->getType()),
                      "matrix store should use HL LdStMatrix");
    Value *Ptr = stInst->getPointerOperand();
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(Ptr)) {
      if (BitCastOperator *BCO = dyn_cast<BitCastOperator>(CE)) {
        SimplifyBitCast(BCO, deadInsts);
      }
    }
  } break;
  case Instruction::LShr:
  case Instruction::AShr:
  case Instruction::Shl: {
    llvm::BinaryOperator *BO = cast<llvm::BinaryOperator>(I);
    Value *op2 = BO->getOperand(1);
    IntegerType *Ty = cast<IntegerType>(BO->getType()->getScalarType());
    unsigned bitWidth = Ty->getBitWidth();
    // Clamp op2 to 0 ~ bitWidth-1
    if (ConstantInt *cOp2 = dyn_cast<ConstantInt>(op2)) {
      unsigned iOp2 = cOp2->getLimitedValue();
      unsigned clampedOp2 = iOp2 & (bitWidth - 1);
      if (iOp2 != clampedOp2) {
        BO->setOperand(1, ConstantInt::get(op2->getType(), clampedOp2));
      }
    } else {
      Value *mask = ConstantInt::get(op2->getType(), bitWidth - 1);
      IRBuilder<> Builder(I);
      op2 = Builder.CreateAnd(op2, mask);
      BO->setOperand(1, op2);
    }
  } break;
  }
}

// Do simple transform to make later lower pass easier.
static void SimpleTransformForHLDXIR(llvm::Module *pM) {
  SmallInstSet deadInsts;
  for (Function &F : pM->functions()) {
    for (BasicBlock &BB : F.getBasicBlockList()) {
      for (BasicBlock::iterator Iter = BB.begin(); Iter != BB.end(); ) {
        Instruction *I = (Iter++);
        if (deadInsts.count(I))
          continue; // Skip dead instructions
        SimpleTransformForHLDXIR(I, deadInsts);
      }
    }
  }

  for (Instruction * I : deadInsts)
    I->dropAllReferences();
  for (Instruction * I : deadInsts)
    I->eraseFromParent();
  deadInsts.clear();

  for (GlobalVariable &GV : pM->globals()) {
    if (dxilutil::IsStaticGlobal(&GV)) {
      for (User *U : GV.users()) {
        if (BitCastOperator *BCO = dyn_cast<BitCastOperator>(U)) {
          SimplifyBitCast(BCO, deadInsts);
        }
      }
    }
  }

  for (Instruction * I : deadInsts)
    I->dropAllReferences();
  for (Instruction * I : deadInsts)
    I->eraseFromParent();
}

static Function *CloneFunction(Function *Orig,
                        const llvm::Twine &Name,
                        llvm::Module *llvmModule,
                        hlsl::DxilTypeSystem &TypeSys,
                        hlsl::DxilTypeSystem &SrcTypeSys) {

  Function *F = Function::Create(Orig->getFunctionType(),
                                 GlobalValue::LinkageTypes::ExternalLinkage,
                                 Name, llvmModule);

  SmallVector<ReturnInst *, 2> Returns;
  ValueToValueMapTy vmap;
  // Map params.
  auto entryParamIt = F->arg_begin();
  for (Argument &param : Orig->args()) {
    vmap[&param] = (entryParamIt++);
  }

  llvm::CloneFunctionInto(F, Orig, vmap, /*ModuleLevelChagnes*/ false, Returns);
  TypeSys.CopyFunctionAnnotation(F, Orig, SrcTypeSys);

  return F;
}

// Clone shader entry function to be called by other functions.
// The original function will be used as shader entry.
static void CloneShaderEntry(Function *ShaderF, StringRef EntryName,
                             HLModule &HLM) {
  Function *F = CloneFunction(ShaderF, "", HLM.GetModule(),
                              HLM.GetTypeSystem(), HLM.GetTypeSystem());

  F->takeName(ShaderF);
  F->setLinkage(GlobalValue::LinkageTypes::InternalLinkage);
  // Set to name before mangled.
  ShaderF->setName(EntryName);

  DxilFunctionAnnotation *annot = HLM.GetFunctionAnnotation(F);
  DxilParameterAnnotation &cloneRetAnnot = annot->GetRetTypeAnnotation();
  // Clear semantic for cloned one.
  cloneRetAnnot.SetSemanticString("");
  cloneRetAnnot.SetSemanticIndexVec({});
  for (unsigned i = 0; i < annot->GetNumParameters(); i++) {
    DxilParameterAnnotation &cloneParamAnnot = annot->GetParameterAnnotation(i);
    // Clear semantic for cloned one.
    cloneParamAnnot.SetSemanticString("");
    cloneParamAnnot.SetSemanticIndexVec({});
  }
}

// For case like:
//cbuffer A {
//  float a;
//  int b;
//}
//
//const static struct {
//  float a;
//  int b;
//}  ST = { a, b };
// Replace user of ST with a and b.
static bool ReplaceConstStaticGlobalUser(GEPOperator *GEP,
                                         std::vector<Constant *> &InitList,
                                         IRBuilder<> &Builder) {
  if (GEP->getNumIndices() < 2) {
    // Don't use sub element.
    return false;
  }

  SmallVector<Value *, 4> idxList;
  auto iter = GEP->idx_begin();
  idxList.emplace_back(*(iter++));
  ConstantInt *subIdx = dyn_cast<ConstantInt>(*(iter++));

  DXASSERT(subIdx, "else dynamic indexing on struct field");
  unsigned subIdxImm = subIdx->getLimitedValue();
  DXASSERT(subIdxImm < InitList.size(), "else struct index out of bound");

  Constant *subPtr = InitList[subIdxImm];
  // Move every idx to idxList except idx for InitList.
  while (iter != GEP->idx_end()) {
    idxList.emplace_back(*(iter++));
  }
  Value *NewGEP = Builder.CreateGEP(subPtr, idxList);
  GEP->replaceAllUsesWith(NewGEP);
  return true;
}

static void ReplaceConstStaticGlobals(
    std::unordered_map<GlobalVariable *, std::vector<Constant *>>
        &staticConstGlobalInitListMap,
    std::unordered_map<GlobalVariable *, Function *>
        &staticConstGlobalCtorMap) {

  for (auto &iter : staticConstGlobalInitListMap) {
    GlobalVariable *GV = iter.first;
    std::vector<Constant *> &InitList = iter.second;
    LLVMContext &Ctx = GV->getContext();
    // Do the replace.
    bool bPass = true;
    for (User *U : GV->users()) {
      IRBuilder<> Builder(Ctx);
      if (GetElementPtrInst *GEPInst = dyn_cast<GetElementPtrInst>(U)) {
        Builder.SetInsertPoint(GEPInst);
        bPass &= ReplaceConstStaticGlobalUser(cast<GEPOperator>(GEPInst), InitList, Builder);
      } else if (GEPOperator *GEP = dyn_cast<GEPOperator>(U)) {
        bPass &= ReplaceConstStaticGlobalUser(GEP, InitList, Builder);
      } else {
        DXASSERT(false, "invalid user of const static global");
      }
    }
    // Clear the Ctor which is useless now.
    if (bPass) {
      Function *Ctor = staticConstGlobalCtorMap[GV];
      Ctor->getBasicBlockList().clear();
      BasicBlock *Entry = BasicBlock::Create(Ctx, "", Ctor);
      IRBuilder<> Builder(Entry);
      Builder.CreateRetVoid();
    }
  }
}

bool BuildImmInit(Function *Ctor) {
  GlobalVariable *GV = nullptr;
  SmallVector<Constant *, 4> ImmList;
  bool allConst = true;
  for (inst_iterator I = inst_begin(Ctor), E = inst_end(Ctor); I != E; ++I) {
    if (StoreInst *SI = dyn_cast<StoreInst>(&(*I))) {
      Value *V = SI->getValueOperand();
      if (!isa<Constant>(V) || V->getType()->isPointerTy()) {
        allConst = false;
        break;
      }
      ImmList.emplace_back(cast<Constant>(V));
      Value *Ptr = SI->getPointerOperand();
      if (GEPOperator *GepOp = dyn_cast<GEPOperator>(Ptr)) {
        Ptr = GepOp->getPointerOperand();
        if (GlobalVariable *pGV = dyn_cast<GlobalVariable>(Ptr)) {
          if (GV == nullptr)
            GV = pGV;
          else {
            DXASSERT(GV == pGV, "else pointer mismatch");
          }
        }
      }
    } else {
      if (!isa<ReturnInst>(*I)) {
        allConst = false;
        break;
      }
    }
  }
  if (!allConst)
    return false;
  if (!GV)
    return false;

  llvm::Type *Ty = GV->getType()->getElementType();
  llvm::ArrayType *AT = dyn_cast<llvm::ArrayType>(Ty);
  // TODO: support other types.
  if (!AT)
    return false;
  if (ImmList.size() != AT->getNumElements())
    return false;
  Constant *Init = llvm::ConstantArray::get(AT, ImmList);
  GV->setInitializer(Init);
  return true;
}

void ProcessCtorFunctions(llvm::Module &M, StringRef globalName,
                          Instruction *InsertPt) {
  // add global call to entry func
  GlobalVariable *GV = M.getGlobalVariable(globalName);
  if (GV) {
    if (ConstantArray *CA = dyn_cast<ConstantArray>(GV->getInitializer())) {

      IRBuilder<> Builder(InsertPt);
      for (User::op_iterator i = CA->op_begin(), e = CA->op_end(); i != e;
           ++i) {
        if (isa<ConstantAggregateZero>(*i))
          continue;
        ConstantStruct *CS = cast<ConstantStruct>(*i);
        if (isa<ConstantPointerNull>(CS->getOperand(1)))
          continue;

        // Must have a function or null ptr.
        if (!isa<Function>(CS->getOperand(1)))
          continue;
        Function *Ctor = cast<Function>(CS->getOperand(1));
        DXASSERT(Ctor->getReturnType()->isVoidTy() && Ctor->arg_size() == 0,
               "function type must be void (void)");

        for (inst_iterator I = inst_begin(Ctor), E = inst_end(Ctor); I != E;
             ++I) {
          if (CallInst *CI = dyn_cast<CallInst>(&(*I))) {
            Function *F = CI->getCalledFunction();
            // Try to build imm initilizer.
            // If not work, add global call to entry func.
            if (BuildImmInit(F) == false) {
              Builder.CreateCall(F);
            }
          } else {
            DXASSERT(isa<ReturnInst>(&(*I)),
                     "else invalid Global constructor function");
          }
        }
      }
      // remove the GV
      GV->eraseFromParent();
    }
  }
}

void CGMSHLSLRuntime::SetPatchConstantFunction(const EntryFunctionInfo &EntryFunc) {

  auto AttrsIter = HSEntryPatchConstantFuncAttr.find(EntryFunc.Func);

  DXASSERT(AttrsIter != HSEntryPatchConstantFuncAttr.end(),
           "we have checked this in AddHLSLFunctionInfo()");

  SetPatchConstantFunctionWithAttr(Entry, AttrsIter->second);
}

void CGMSHLSLRuntime::SetPatchConstantFunctionWithAttr(
    const EntryFunctionInfo &EntryFunc,
    const clang::HLSLPatchConstantFuncAttr *PatchConstantFuncAttr) {
  StringRef funcName = PatchConstantFuncAttr->getFunctionName();

  auto Entry = patchConstantFunctionMap.find(funcName);
  if (Entry == patchConstantFunctionMap.end()) {
    DiagnosticsEngine &Diags = CGM.getDiags();
    unsigned DiagID =
      Diags.getCustomDiagID(DiagnosticsEngine::Error,
        "Cannot find patchconstantfunc %0.");
    Diags.Report(PatchConstantFuncAttr->getLocation(), DiagID)
      << funcName;
    return;
  }

  if (Entry->second.NumOverloads != 1) {
    DiagnosticsEngine &Diags = CGM.getDiags();
    unsigned DiagID =
      Diags.getCustomDiagID(DiagnosticsEngine::Warning,
        "Multiple overloads of patchconstantfunc %0.");
    unsigned NoteID =
      Diags.getCustomDiagID(DiagnosticsEngine::Note,
        "This overload was selected.");
    Diags.Report(PatchConstantFuncAttr->getLocation(), DiagID)
      << funcName;
    Diags.Report(Entry->second.SL, NoteID);
  }

  Function *patchConstFunc = Entry->second.Func;
  DXASSERT(m_pHLModule->HasDxilFunctionProps(EntryFunc.Func),
    " else AddHLSLFunctionInfo did not save the dxil function props for the "
    "HS entry.");
  DxilFunctionProps *HSProps = &m_pHLModule->GetDxilFunctionProps(EntryFunc.Func);
  m_pHLModule->SetPatchConstantFunctionForHS(EntryFunc.Func, patchConstFunc);
  DXASSERT_NOMSG(patchConstantFunctionPropsMap.count(patchConstFunc));
  // Check no inout parameter for patch constant function.
  DxilFunctionAnnotation *patchConstFuncAnnotation =
    m_pHLModule->GetFunctionAnnotation(patchConstFunc);
  for (unsigned i = 0; i < patchConstFuncAnnotation->GetNumParameters(); i++) {
    if (patchConstFuncAnnotation->GetParameterAnnotation(i)
      .GetParamInputQual() == DxilParamInputQual::Inout) {
      DiagnosticsEngine &Diags = CGM.getDiags();
      unsigned DiagID = Diags.getCustomDiagID(
        DiagnosticsEngine::Error,
        "Patch Constant function %0 should not have inout param.");
      Diags.Report(Entry->second.SL, DiagID) << funcName;
    }
  }
  
  // Input/Output control point validation.
  if (patchConstantFunctionPropsMap.count(patchConstFunc)) {
    const DxilFunctionProps &patchProps =
      *patchConstantFunctionPropsMap[patchConstFunc];
    if (patchProps.ShaderProps.HS.inputControlPoints != 0 &&
      patchProps.ShaderProps.HS.inputControlPoints !=
      HSProps->ShaderProps.HS.inputControlPoints) {
      DiagnosticsEngine &Diags = CGM.getDiags();
      unsigned DiagID =
        Diags.getCustomDiagID(DiagnosticsEngine::Error,
          "Patch constant function's input patch input "
          "should have %0 elements, but has %1.");
      Diags.Report(Entry->second.SL, DiagID)
        << HSProps->ShaderProps.HS.inputControlPoints
        << patchProps.ShaderProps.HS.inputControlPoints;
    }
    if (patchProps.ShaderProps.HS.outputControlPoints != 0 &&
      patchProps.ShaderProps.HS.outputControlPoints !=
      HSProps->ShaderProps.HS.outputControlPoints) {
      DiagnosticsEngine &Diags = CGM.getDiags();
      unsigned DiagID = Diags.getCustomDiagID(
        DiagnosticsEngine::Error,
        "Patch constant function's output patch input "
        "should have %0 elements, but has %1.");
      Diags.Report(Entry->second.SL, DiagID)
        << HSProps->ShaderProps.HS.outputControlPoints
        << patchProps.ShaderProps.HS.outputControlPoints;
    }
  }
  
}

static void ReportDisallowedTypeInExportParam(CodeGenModule &CGM, StringRef name) {
  DiagnosticsEngine &Diags = CGM.getDiags();
  unsigned DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error,
    "Exported function %0 must not contain a resource in parameter or return type.");
  std::string escaped;
  llvm::raw_string_ostream os(escaped);
  dxilutil::PrintEscapedString(name, os);
  Diags.Report(DiagID) << os.str();
}

// Returns true a global value is being updated
static bool GlobalHasStoreUserRec(Value *V, std::set<Value *> &visited) {
  bool isWriteEnabled = false;
  if (V && visited.find(V) == visited.end()) {
    visited.insert(V);
    for (User *U : V->users()) {
      if (isa<StoreInst>(U)) {
        return true;
      } else if (CallInst* CI = dyn_cast<CallInst>(U)) {
        Function *F = CI->getCalledFunction();
        if (!F->isIntrinsic()) {
          HLOpcodeGroup hlGroup = GetHLOpcodeGroup(F);
          switch (hlGroup) {
          case HLOpcodeGroup::NotHL:
            return true;
          case HLOpcodeGroup::HLMatLoadStore:
          {
            HLMatLoadStoreOpcode opCode = static_cast<HLMatLoadStoreOpcode>(hlsl::GetHLOpcode(CI));
            if (opCode == HLMatLoadStoreOpcode::ColMatStore || opCode == HLMatLoadStoreOpcode::RowMatStore)
              return true;
            break;
          }
          case HLOpcodeGroup::HLCast:
          case HLOpcodeGroup::HLSubscript:
            if (GlobalHasStoreUserRec(U, visited))
              return true;
            break;
          default:
            break;
          }
        }
      } else if (isa<GEPOperator>(U) || isa<PHINode>(U) || isa<SelectInst>(U)) {
        if (GlobalHasStoreUserRec(U, visited))
          return true;
      }
    }
  }
  return isWriteEnabled;
}

// Returns true if any of the direct user of a global is a store inst
// otherwise recurse through the remaining users and check if any GEP
// exists and which in turn has a store inst as user.
static bool GlobalHasStoreUser(GlobalVariable *GV) {
  std::set<Value *> visited;
  Value *V = cast<Value>(GV);
  return GlobalHasStoreUserRec(V, visited);
}

static GlobalVariable *CreateStaticGlobal(llvm::Module *M, GlobalVariable *GV) {
  Constant *GC = M->getOrInsertGlobal(GV->getName().str() + ".static.copy",
                                      GV->getType()->getPointerElementType());
  GlobalVariable *NGV = cast<GlobalVariable>(GC);
  if (GV->hasInitializer()) {
    NGV->setInitializer(GV->getInitializer());
  }
  // static global should have internal linkage
  NGV->setLinkage(GlobalValue::InternalLinkage);
  return NGV;
}

static void CreateWriteEnabledStaticGlobals(llvm::Module *M,
                                            llvm::Function *EF) {
  std::vector<GlobalVariable *> worklist;
  for (GlobalVariable &GV : M->globals()) {
    if (!GV.isConstant() && GV.getLinkage() != GlobalValue::InternalLinkage &&
        // skip globals which are HLSL objects or group shared
        !HLModule::IsHLSLObjectType(GV.getType()->getPointerElementType()) &&
        !dxilutil::IsSharedMemoryGlobal(&GV)) {
      if (GlobalHasStoreUser(&GV))
        worklist.emplace_back(&GV);
      // TODO: Ensure that constant globals aren't using initializer
      GV.setConstant(true);
    }
  }

  IRBuilder<> Builder(
      dxilutil::FirstNonAllocaInsertionPt(&EF->getEntryBlock()));
  for (GlobalVariable *GV : worklist) {
    GlobalVariable *NGV = CreateStaticGlobal(M, GV);
    GV->replaceAllUsesWith(NGV);

    // insert memcpy in all entryblocks
    uint64_t size = M->getDataLayout().getTypeAllocSize(
        GV->getType()->getPointerElementType());
    Builder.CreateMemCpy(NGV, GV, size, 1);
  }
}

void CGMSHLSLRuntime::FinishCodeGen() {
  // Library don't have entry.
  if (!m_bIsLib) {
    SetEntryFunction();

    // If at this point we haven't determined the entry function it's an error.
    if (m_pHLModule->GetEntryFunction() == nullptr) {
      assert(CGM.getDiags().hasErrorOccurred() &&
             "else SetEntryFunction should have reported this condition");
      return;
    }

    // In back-compat mode (with /Gec flag) create a static global for each const global
    // to allow writing to it.
    // TODO: Verfiy the behavior of static globals in hull shader
    if(CGM.getLangOpts().EnableDX9CompatMode && CGM.getLangOpts().HLSLVersion <= 2016)
      CreateWriteEnabledStaticGlobals(m_pHLModule->GetModule(), m_pHLModule->GetEntryFunction());
    if (m_pHLModule->GetShaderModel()->IsHS()) {
      SetPatchConstantFunction(Entry);
    }
  } else {
    for (auto &it : entryFunctionMap) {
      // skip clone if RT entry
      if (m_pHLModule->GetDxilFunctionProps(it.second.Func).IsRay())
        continue;

      // TODO: change flattened function names to dx.entry.<name>:
      //std::string entryName = (Twine(dxilutil::EntryPrefix) + it.getKey()).str();
      CloneShaderEntry(it.second.Func, it.getKey(), *m_pHLModule);

      auto AttrIter = HSEntryPatchConstantFuncAttr.find(it.second.Func);
      if (AttrIter != HSEntryPatchConstantFuncAttr.end()) {
        SetPatchConstantFunctionWithAttr(it.second, AttrIter->second);
      }
    }
  }

  ReplaceConstStaticGlobals(staticConstGlobalInitListMap,
                            staticConstGlobalCtorMap);

  // Create copy for clip plane.
  for (Function *F : clipPlaneFuncList) {
    DxilFunctionProps &props = m_pHLModule->GetDxilFunctionProps(F);
    IRBuilder<> Builder(F->getEntryBlock().getFirstInsertionPt());

    for (unsigned i = 0; i < DXIL::kNumClipPlanes; i++) {
      Value *clipPlane = props.ShaderProps.VS.clipPlanes[i];
      if (!clipPlane)
        continue;
      if (m_bDebugInfo) {
        Builder.SetCurrentDebugLocation(debugInfoMap[clipPlane]);
      }
      llvm::Type *Ty = clipPlane->getType()->getPointerElementType();
      // Constant *zeroInit = ConstantFP::get(Ty, 0);
      GlobalVariable *GV = new llvm::GlobalVariable(
          TheModule, Ty, /*IsConstant*/ false, // constant false to store.
          llvm::GlobalValue::ExternalLinkage,
          /*InitVal*/ nullptr, Twine("SV_ClipPlane") + Twine(i));
      Value *initVal = Builder.CreateLoad(clipPlane);
      Builder.CreateStore(initVal, GV);
      props.ShaderProps.VS.clipPlanes[i] = GV;
    }
  }

  // Allocate constant buffers.
  AllocateDxilConstantBuffers(m_pHLModule);
  // TODO: create temp variable for constant which has store use.

  // Create Global variable and type annotation for each CBuffer.
  ConstructCBuffer(m_pHLModule, CBufferType, m_ConstVarAnnotationMap);

  if (!m_bIsLib) {
    // need this for "llvm.global_dtors"?
    ProcessCtorFunctions(TheModule ,"llvm.global_ctors",
                  Entry.Func->getEntryBlock().getFirstInsertionPt());
  }
  // translate opcode into parameter for intrinsic functions
  AddOpcodeParamForIntrinsics(*m_pHLModule, m_IntrinsicMap, resMetadataMap);

  // Register patch constant functions referenced by exported Hull Shaders
  if (m_bIsLib && !m_ExportMap.empty()) {
    for (auto &it : entryFunctionMap) {
      if (m_pHLModule->HasDxilFunctionProps(it.second.Func)) {
        const DxilFunctionProps &props = m_pHLModule->GetDxilFunctionProps(it.second.Func);
        if (props.IsHS())
          m_ExportMap.RegisterExportedFunction(props.ShaderProps.HS.patchConstantFunc);
      }
    }
  }

  // Pin entry point and constant buffers, mark everything else internal.
  for (Function &f : m_pHLModule->GetModule()->functions()) {
    if (!m_bIsLib) {
      if (&f == m_pHLModule->GetEntryFunction() ||
          IsPatchConstantFunction(&f) || f.isDeclaration()) {
        if (f.isDeclaration() && !f.isIntrinsic() &&
            GetHLOpcodeGroup(&f) == HLOpcodeGroup::NotHL) {
          DiagnosticsEngine &Diags = CGM.getDiags();
          unsigned DiagID = Diags.getCustomDiagID(
              DiagnosticsEngine::Error,
              "External function used in non-library profile: %0");
          std::string escaped;
          llvm::raw_string_ostream os(escaped);
          dxilutil::PrintEscapedString(f.getName(), os);
          Diags.Report(DiagID) << os.str();
          return;
        }
        f.setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);
      } else {
        f.setLinkage(GlobalValue::LinkageTypes::InternalLinkage);
      }
    }
    // Skip no inline functions.
    if (f.hasFnAttribute(llvm::Attribute::NoInline))
      continue;
    // Always inline for used functions.
    if (!f.user_empty() && !f.isDeclaration())
      f.addFnAttr(llvm::Attribute::AlwaysInline);
  }

  if (m_bIsLib && !m_ExportMap.empty()) {
    m_ExportMap.BeginProcessing();
    for (Function &f : m_pHLModule->GetModule()->functions()) {
      if (f.isDeclaration() || f.isIntrinsic() ||
        GetHLOpcodeGroup(&f) != HLOpcodeGroup::NotHL)
        continue;
      m_ExportMap.ProcessFunction(&f, true);
    }
    // TODO: add subobject export names here.
    if (!m_ExportMap.EndProcessing()) {
      for (auto &name : m_ExportMap.GetNameCollisions()) {
        DiagnosticsEngine &Diags = CGM.getDiags();
        unsigned DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error,
          "Export name collides with another export: %0");
        std::string escaped;
        llvm::raw_string_ostream os(escaped);
        dxilutil::PrintEscapedString(name, os);
        Diags.Report(DiagID) << os.str();
      }
      for (auto &name : m_ExportMap.GetUnusedExports()) {
        DiagnosticsEngine &Diags = CGM.getDiags();
        unsigned DiagID = Diags.getCustomDiagID(DiagnosticsEngine::Error,
          "Could not find target for export: %0");
        std::string escaped;
        llvm::raw_string_ostream os(escaped);
        dxilutil::PrintEscapedString(name, os);
        Diags.Report(DiagID) << os.str();
      }
    }
  }

  for (auto &it : m_ExportMap.GetFunctionRenames()) {
    Function *F = it.first;
    auto &renames = it.second;

    if (renames.empty())
      continue;

    // Rename the original, if necessary, then clone the rest
    if (renames.find(F->getName()) == renames.end())
      F->setName(*renames.begin());

    for (auto &itName : renames) {
      if (F->getName() != itName) {
        Function *pClone = CloneFunction(F, itName, m_pHLModule->GetModule(),
          m_pHLModule->GetTypeSystem(), m_pHLModule->GetTypeSystem());
        // add DxilFunctionProps if entry
        if (m_pHLModule->HasDxilFunctionProps(F)) {
          DxilFunctionProps &props = m_pHLModule->GetDxilFunctionProps(F);
          auto newProps = llvm::make_unique<DxilFunctionProps>(props);
          m_pHLModule->AddDxilFunctionProps(pClone, newProps);
        }
      }
    }
  }

  if (CGM.getCodeGenOpts().ExportShadersOnly) {
    for (Function &f : m_pHLModule->GetModule()->functions()) {
      // Skip declarations, intrinsics, shaders, and non-external linkage
      if (f.isDeclaration() || f.isIntrinsic() ||
          GetHLOpcodeGroup(&f) != HLOpcodeGroup::NotHL ||
          m_pHLModule->HasDxilFunctionProps(&f) ||
          m_pHLModule->IsPatchConstantShader(&f) ||
          f.getLinkage() != GlobalValue::LinkageTypes::ExternalLinkage)
        continue;
      // Mark non-shader user functions as InternalLinkage
      f.setLinkage(GlobalValue::LinkageTypes::InternalLinkage);
    }
  }

  // Now iterate hull shaders and make sure their corresponding patch constant
  // functions are marked ExternalLinkage:
  for (Function &f : m_pHLModule->GetModule()->functions()) {
    if (f.isDeclaration() || f.isIntrinsic() ||
        GetHLOpcodeGroup(&f) != HLOpcodeGroup::NotHL ||
        f.getLinkage() != GlobalValue::LinkageTypes::ExternalLinkage ||
        !m_pHLModule->HasDxilFunctionProps(&f))
      continue;
    DxilFunctionProps &props = m_pHLModule->GetDxilFunctionProps(&f);
    if (!props.IsHS())
      continue;
    Function *PCFunc = props.ShaderProps.HS.patchConstantFunc;
    if (PCFunc->getLinkage() != GlobalValue::LinkageTypes::ExternalLinkage)
      PCFunc->setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);
  }

  // Disallow resource arguments in (non-entry) function exports
  // unless offline linking target.
  if (m_bIsLib && m_pHLModule->GetShaderModel()->GetMinor() != ShaderModel::kOfflineMinor) {
    for (Function &f : m_pHLModule->GetModule()->functions()) {
      // Skip llvm intrinsics, non-external linkage, entry/patch constant func, and HL intrinsics
      if (!f.isIntrinsic() &&
          f.getLinkage() == GlobalValue::LinkageTypes::ExternalLinkage &&
          !m_pHLModule->HasDxilFunctionProps(&f) &&
          !m_pHLModule->IsPatchConstantShader(&f) &&
          GetHLOpcodeGroup(&f) == HLOpcodeGroup::NotHL) {
        // Verify no resources in param/return types
        if (dxilutil::ContainsHLSLObjectType(f.getReturnType())) {
          ReportDisallowedTypeInExportParam(CGM, f.getName());
          continue;
        }
        for (auto &Arg : f.args()) {
          if (dxilutil::ContainsHLSLObjectType(Arg.getType())) {
            ReportDisallowedTypeInExportParam(CGM, f.getName());
            break;
          }
        }
      }
    }
  }

  // Do simple transform to make later lower pass easier.
  SimpleTransformForHLDXIR(m_pHLModule->GetModule());

  // Handle lang extensions if provided.
  if (CGM.getCodeGenOpts().HLSLExtensionsCodegen) {
    // Add semantic defines for extensions if any are available.
    HLSLExtensionsCodegenHelper::SemanticDefineErrorList errors =
      CGM.getCodeGenOpts().HLSLExtensionsCodegen->WriteSemanticDefines(m_pHLModule->GetModule());

    DiagnosticsEngine &Diags = CGM.getDiags();
    for (const HLSLExtensionsCodegenHelper::SemanticDefineError& error : errors) {
      DiagnosticsEngine::Level level = DiagnosticsEngine::Error;
      if (error.IsWarning())
        level = DiagnosticsEngine::Warning;
      unsigned DiagID = Diags.getCustomDiagID(level, "%0");
      Diags.Report(SourceLocation::getFromRawEncoding(error.Location()), DiagID) << error.Message();
    }

    // Add root signature from a #define. Overrides root signature in function attribute.
    {
      using Status = HLSLExtensionsCodegenHelper::CustomRootSignature::Status;
      HLSLExtensionsCodegenHelper::CustomRootSignature customRootSig;
      Status status = CGM.getCodeGenOpts().HLSLExtensionsCodegen->GetCustomRootSignature(&customRootSig);
      if (status == Status::FOUND) {
          CompileRootSignature(customRootSig.RootSignature, Diags,
                               SourceLocation::getFromRawEncoding(customRootSig.EncodedSourceLocation),
                               rootSigVer, &m_pHLModule->GetRootSignature());
      }
    }
  }

  // At this point, we have a high-level DXIL module - record this.
  SetPauseResumePasses(*m_pHLModule->GetModule(), "hlsl-hlemit", "hlsl-hlensure");
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

static bool IsRowMajorMatrix(QualType Ty, bool bDefaultRowMajor) {
  bool bRowMajor = bDefaultRowMajor;
  HasHLSLMatOrientation(Ty, &bRowMajor);
  return bRowMajor;
}

static bool IsUnsigned(QualType Ty) {
  Ty = Ty.getCanonicalType().getNonReferenceType();

  if (hlsl::IsHLSLVecMatType(Ty))
    Ty = CGHLSLRuntime::GetHLSLVecMatElementType(Ty);

  if (Ty->isExtVectorType())
    Ty = Ty->getAs<clang::ExtVectorType>()->getElementType();

  return Ty->isUnsignedIntegerType();
}

static unsigned GetHLOpcode(const Expr *E) {
  switch (E->getStmtClass()) {
  case Stmt::CompoundAssignOperatorClass:
  case Stmt::BinaryOperatorClass: {
    const clang::BinaryOperator *binOp = cast<clang::BinaryOperator>(E);
    HLBinaryOpcode binOpcode = BinaryOperatorKindMap[binOp->getOpcode()];
    if (HasUnsignedOpcode(binOpcode)) {
      if (IsUnsigned(binOp->getLHS()->getType())) {
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
    bool toUnsigned = IsUnsigned(E->getType());
    bool fromUnsigned = IsUnsigned(CE->getSubExpr()->getType());
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
    } else if (HLMatrixLower::IsMatrixType(valEltTy)) {
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
        if (HLModule::IsHLSLObjectType(ST)) {
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
    if (HLMatrixLower::IsMatrixType(valTy)) {
      unsigned col, row;
      llvm::Type *EltTy = HLMatrixLower::GetMatrixInfo(valTy, col, row);
      // All matrix Value should be row major.
      // Init list is row major in scalar.
      // So the order is match here, just cast to vector.
      unsigned matSize = col * row;
      bool isRowMajor = IsRowMajorMatrix(Ty, m_pHLModule->GetHLOptions().bDefaultRowMajor);

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
      QualType EltTy = GetHLSLVecMatElementType(Ty);
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

// Cast elements in initlist if not match the target type.
// idx is current element index in initlist, Ty is target type.
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
    if (HLModule::IsHLSLObjectType(CGF.ConvertType(Ty))) {
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
    Value *val = elts[idx];
    llvm::Type *srcTy = val->getType();
    llvm::Type *dstTy = CGF.ConvertType(Ty);
    if (srcTy != dstTy) {
      Instruction::CastOps castOp =
          static_cast<Instruction::CastOps>(HLModule::FindCastOp(
              IsUnsigned(eltTys[idx]), IsUnsigned(Ty), srcTy, dstTy));
      elts[idx] = CGF.Builder.CreateCast(castOp, val, dstTy);
    }
    idx++;
  }
}

static void StoreInitListToDestPtr(Value *DestPtr,
                                   SmallVector<Value *, 4> &elts, unsigned &idx,
                                   QualType Type, CodeGenTypes &Types, bool bDefaultRowMajor,
                                   CGBuilderTy &Builder, llvm::Module &M) {
  llvm::Type *Ty = DestPtr->getType()->getPointerElementType();
  llvm::Type *i32Ty = llvm::Type::getInt32Ty(Ty->getContext());

  if (Ty->isVectorTy()) {
    Value *Result = UndefValue::get(Ty);
    for (unsigned i = 0; i < Ty->getVectorNumElements(); i++)
      Result = Builder.CreateInsertElement(Result, elts[idx + i], i);
    Builder.CreateStore(Result, DestPtr);
    idx += Ty->getVectorNumElements();
  } else if (HLMatrixLower::IsMatrixType(Ty)) {
    bool isRowMajor =
        IsRowMajorMatrix(Type, bDefaultRowMajor);

    unsigned row, col;
    HLMatrixLower::GetMatrixInfo(Ty, col, row);
    std::vector<Value *> matInitList(col * row);
    for (unsigned i = 0; i < col; i++) {
      for (unsigned r = 0; r < row; r++) {
        unsigned matIdx = i * row + r;
        matInitList[matIdx] = elts[idx + matIdx];
      }
    }
    idx += row * col;
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
    if (HLModule::IsHLSLObjectType(Ty)) {
      Builder.CreateStore(elts[idx], DestPtr);
      idx++;
    } else {
      Constant *zero = ConstantInt::get(i32Ty, 0);

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
            Constant *gepIdx = ConstantInt::get(i32Ty, i);
            Value *GEP = Builder.CreateInBoundsGEP(DestPtr, {zero, gepIdx});
            StoreInitListToDestPtr(GEP, elts, idx, parentTy, Types,
                                   bDefaultRowMajor, Builder, M);
          }
        }
      }
      for (FieldDecl *field : RD->fields()) {
        unsigned i = RL.getLLVMFieldNo(field);
        Constant *gepIdx = ConstantInt::get(i32Ty, i);
        Value *GEP = Builder.CreateInBoundsGEP(DestPtr, {zero, gepIdx});
        StoreInitListToDestPtr(GEP, elts, idx, field->getType(), Types,
                               bDefaultRowMajor, Builder, M);
      }
    }
  } else if (Ty->isArrayTy()) {
    Constant *zero = ConstantInt::get(i32Ty, 0);
    QualType EltType = Type->getAsArrayTypeUnsafe()->getElementType();
    for (unsigned i = 0; i < Ty->getArrayNumElements(); i++) {
      Constant *gepIdx = ConstantInt::get(i32Ty, i);
      Value *GEP = Builder.CreateInBoundsGEP(DestPtr, {zero, gepIdx});
      StoreInitListToDestPtr(GEP, elts, idx, EltType, Types, bDefaultRowMajor,
                             Builder, M);
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
        LValue LV = CGF.EmitLValue(Expr);
        if (LV.isSimple()) {
          Constant *SrcPtr = dyn_cast<Constant>(LV.getAddress());
          if (SrcPtr && !isa<UndefValue>(SrcPtr)) {
            InitConstants.emplace_back(SrcPtr);
            continue;
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
    StoreInitListToDestPtr(DestPtr, EltValList, idx, ResultTy, CGF.getTypes(),
                           bDefaultRowMajor, CGF.Builder, TheModule);
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

static void FlatConstToList(Constant *C, SmallVector<Constant *, 4> &EltValList,
                            QualType Type, CodeGenTypes &Types,
                            bool bDefaultRowMajor) {
  llvm::Type *Ty = C->getType();
  if (llvm::VectorType *VT = dyn_cast<llvm::VectorType>(Ty)) {
    // Type is only for matrix. Keep use Type to next level.
    for (unsigned i = 0; i < VT->getNumElements(); i++) {
      FlatConstToList(C->getAggregateElement(i), EltValList, Type, Types,
                      bDefaultRowMajor);
    }
  } else if (HLMatrixLower::IsMatrixType(Ty)) {
    bool isRowMajor = IsRowMajorMatrix(Type, bDefaultRowMajor);
    // matrix type is struct { vector<Ty, row> [col] };
    // Strip the struct level here.
    Constant *matVal = C->getAggregateElement((unsigned)0);
    const RecordType *RT = Type->getAs<RecordType>();
    RecordDecl *RD = RT->getDecl();
    QualType EltTy = RD->field_begin()->getType();
    // When scan, init list scalars is row major.
    if (isRowMajor) {
      // Don't change the major for row major value.
      FlatConstToList(matVal, EltValList, EltTy, Types, bDefaultRowMajor);
    } else {
      // Save to tmp list.
      SmallVector<Constant *, 4> matEltList;
      FlatConstToList(matVal, matEltList, EltTy, Types, bDefaultRowMajor);
      unsigned row, col;
      HLMatrixLower::GetMatrixInfo(Ty, col, row);
      // Change col major value to row major.
      for (unsigned r = 0; r < row; r++)
        for (unsigned c = 0; c < col; c++) {
          unsigned colMajorIdx = c * row + r;
          EltValList.emplace_back(matEltList[colMajorIdx]);
        }
    }
  } else if (llvm::ArrayType *AT = dyn_cast<llvm::ArrayType>(Ty)) {
    QualType EltTy = Type->getAsArrayTypeUnsafe()->getElementType();
    for (unsigned i = 0; i < AT->getNumElements(); i++) {
      FlatConstToList(C->getAggregateElement(i), EltValList, EltTy, Types,
                      bDefaultRowMajor);
    }
  } else if (dyn_cast<llvm::StructType>(Ty)) {
    RecordDecl *RD = Type->getAsStructureType()->getDecl();
    const CGRecordLayout &RL = Types.getCGRecordLayout(RD);
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
          FlatConstToList(C->getAggregateElement(i), EltValList, parentTy,
                          Types, bDefaultRowMajor);
        }
      }
    }

    for (auto fieldIter = RD->field_begin(), fieldEnd = RD->field_end();
         fieldIter != fieldEnd; ++fieldIter) {
      unsigned i = RL.getLLVMFieldNo(*fieldIter);

      FlatConstToList(C->getAggregateElement(i), EltValList,
                      fieldIter->getType(), Types, bDefaultRowMajor);
    }
  } else {
    EltValList.emplace_back(C);
  }
}

static bool ScanConstInitList(CodeGenModule &CGM, InitListExpr *E,
                              SmallVector<Constant *, 4> &EltValList,
                              CodeGenTypes &Types, bool bDefaultRowMajor) {
  unsigned NumInitElements = E->getNumInits();
  for (unsigned i = 0; i != NumInitElements; ++i) {
    Expr *init = E->getInit(i);
    QualType iType = init->getType();
    if (InitListExpr *initList = dyn_cast<InitListExpr>(init)) {
      if (!ScanConstInitList(CGM, initList, EltValList, Types,
                             bDefaultRowMajor))
        return false;
    } else if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(init)) {
      if (VarDecl *D = dyn_cast<VarDecl>(ref->getDecl())) {
        if (!D->hasInit())
          return false;
        if (Constant *initVal = CGM.EmitConstantInit(*D)) {
          FlatConstToList(initVal, EltValList, iType, Types, bDefaultRowMajor);
        } else {
          return false;
        }
      } else {
        return false;
      }
    } else if (hlsl::IsHLSLMatType(iType)) {
      return false;
    } else if (CodeGenFunction::hasScalarEvaluationKind(iType)) {
      if (Constant *initVal = CGM.EmitConstantExpr(init, iType)) {
        FlatConstToList(initVal, EltValList, iType, Types, bDefaultRowMajor);
      } else {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

static Constant *BuildConstInitializer(QualType Type, unsigned &offset,
                                       SmallVector<Constant *, 4> &EltValList,
                                       CodeGenTypes &Types,
                                       bool bDefaultRowMajor);

static Constant *BuildConstVector(llvm::VectorType *VT, unsigned &offset,
                                  SmallVector<Constant *, 4> &EltValList,
                                  QualType Type, CodeGenTypes &Types) {
  SmallVector<Constant *, 4> Elts;
  QualType EltTy = hlsl::GetHLSLVecElementType(Type);
  for (unsigned i = 0; i < VT->getNumElements(); i++) {
    Elts.emplace_back(BuildConstInitializer(EltTy, offset, EltValList, Types,
                                            // Vector don't need major.
                                            /*bDefaultRowMajor*/ false));
  }
  return llvm::ConstantVector::get(Elts);
}

static Constant *BuildConstMatrix(llvm::Type *Ty, unsigned &offset,
                                  SmallVector<Constant *, 4> &EltValList,
                                  QualType Type, CodeGenTypes &Types,
                                  bool bDefaultRowMajor) {
  QualType EltTy = hlsl::GetHLSLMatElementType(Type);
  unsigned col, row;
  HLMatrixLower::GetMatrixInfo(Ty, col, row);
  llvm::ArrayType *AT = cast<llvm::ArrayType>(Ty->getStructElementType(0));
  // Save initializer elements first.
  // Matrix initializer is row major.
  SmallVector<Constant *, 16> elts;
  for (unsigned i = 0; i < col * row; i++) {
    elts.emplace_back(BuildConstInitializer(EltTy, offset, EltValList, Types,
                                            bDefaultRowMajor));
  }

  bool isRowMajor = IsRowMajorMatrix(Type, bDefaultRowMajor);

  SmallVector<Constant *, 16> majorElts(elts.begin(), elts.end());
  if (!isRowMajor) {
    // cast row major to col major.
    for (unsigned c = 0; c < col; c++) {
      SmallVector<Constant *, 4> rows;
      for (unsigned r = 0; r < row; r++) {
        unsigned rowMajorIdx = r * col + c;
        unsigned colMajorIdx = c * row + r;
        majorElts[colMajorIdx] = elts[rowMajorIdx];
      }
    }
  }
  // The type is vector<element, col>[row].
  SmallVector<Constant *, 4> rows;
  unsigned idx = 0;
  for (unsigned r = 0; r < row; r++) {
    SmallVector<Constant *, 4> cols;
    for (unsigned c = 0; c < col; c++) {
      cols.emplace_back(majorElts[idx++]);
    }
    rows.emplace_back(llvm::ConstantVector::get(cols));
  }
  Constant *mat = llvm::ConstantArray::get(AT, rows);
  return llvm::ConstantStruct::get(cast<llvm::StructType>(Ty), mat);
}

static Constant *BuildConstArray(llvm::ArrayType *AT, unsigned &offset,
                                 SmallVector<Constant *, 4> &EltValList,
                                 QualType Type, CodeGenTypes &Types,
                                 bool bDefaultRowMajor) {
  SmallVector<Constant *, 4> Elts;
  QualType EltType = QualType(Type->getArrayElementTypeNoTypeQual(), 0);
  for (unsigned i = 0; i < AT->getNumElements(); i++) {
    Elts.emplace_back(BuildConstInitializer(EltType, offset, EltValList, Types,
                                            bDefaultRowMajor));
  }
  return llvm::ConstantArray::get(AT, Elts);
}

static Constant *BuildConstStruct(llvm::StructType *ST, unsigned &offset,
                                  SmallVector<Constant *, 4> &EltValList,
                                  QualType Type, CodeGenTypes &Types,
                                  bool bDefaultRowMajor) {
  SmallVector<Constant *, 4> Elts;

  const RecordType *RT = Type->getAsStructureType();
  if (!RT)
    RT = Type->getAs<RecordType>();
  const RecordDecl *RD = RT->getDecl();

  if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
    if (CXXRD->getNumBases()) {
      // Add base as field.
      for (const auto &I : CXXRD->bases()) {
        const CXXRecordDecl *BaseDecl =
            cast<CXXRecordDecl>(I.getType()->castAs<RecordType>()->getDecl());
        // Skip empty struct.
        if (BaseDecl->field_empty())
          continue;

        // Add base as a whole constant. Not as element.
        Elts.emplace_back(BuildConstInitializer(I.getType(), offset, EltValList,
                                                Types, bDefaultRowMajor));
      }
    }
  }

  for (auto fieldIter = RD->field_begin(), fieldEnd = RD->field_end();
       fieldIter != fieldEnd; ++fieldIter) {
    Elts.emplace_back(BuildConstInitializer(
        fieldIter->getType(), offset, EltValList, Types, bDefaultRowMajor));
  }

  return llvm::ConstantStruct::get(ST, Elts);
}

static Constant *BuildConstInitializer(QualType Type, unsigned &offset,
                                       SmallVector<Constant *, 4> &EltValList,
                                       CodeGenTypes &Types,
                                       bool bDefaultRowMajor) {
  llvm::Type *Ty = Types.ConvertType(Type);
  if (llvm::VectorType *VT = dyn_cast<llvm::VectorType>(Ty)) {
    return BuildConstVector(VT, offset, EltValList, Type, Types);
  } else if (llvm::ArrayType *AT = dyn_cast<llvm::ArrayType>(Ty)) {
    return BuildConstArray(AT, offset, EltValList, Type, Types,
                           bDefaultRowMajor);
  } else if (HLMatrixLower::IsMatrixType(Ty)) {
    return BuildConstMatrix(Ty, offset, EltValList, Type, Types,
                            bDefaultRowMajor);
  } else if (StructType *ST = dyn_cast<llvm::StructType>(Ty)) {
    return BuildConstStruct(ST, offset, EltValList, Type, Types,
                            bDefaultRowMajor);
  } else {
    // Scalar basic types.
    Constant *Val = EltValList[offset++];
    if (Val->getType() == Ty) {
      return Val;
    } else {
      IRBuilder<> Builder(Ty->getContext());
      // Don't cast int to bool. bool only for scalar.
      if (Ty == Builder.getInt1Ty() && Val->getType() == Builder.getInt32Ty())
        return Val;
      Instruction::CastOps castOp =
          static_cast<Instruction::CastOps>(HLModule::FindCastOp(
              IsUnsigned(Type), IsUnsigned(Type), Val->getType(), Ty));
      return cast<Constant>(Builder.CreateCast(castOp, Val, Ty));
    }
  }
}

Constant *CGMSHLSLRuntime::EmitHLSLConstInitListExpr(CodeGenModule &CGM,
                                                     InitListExpr *E) {
  bool bDefaultRowMajor = m_pHLModule->GetHLOptions().bDefaultRowMajor;
  SmallVector<Constant *, 4> EltValList;
  if (!ScanConstInitList(CGM, E, EltValList, CGM.getTypes(), bDefaultRowMajor))
    return nullptr;

  QualType Type = E->getType();
  unsigned offset = 0;
  return BuildConstInitializer(Type, offset, EltValList, CGM.getTypes(),
                               bDefaultRowMajor);
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
  bool bDstSigned = DstType->isSignedIntegerType();

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
        if (bDstSigned)
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
          if (bDstSigned)
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

Value *CGMSHLSLRuntime::EmitHLSLMatrixSubscript(CodeGenFunction &CGF,
                                                llvm::Type *RetType,
                                                llvm::Value *Ptr,
                                                llvm::Value *Idx,
                                                clang::QualType Ty) {
  bool isRowMajor =
      IsRowMajorMatrix(Ty, m_pHLModule->GetHLOptions().bDefaultRowMajor);
  unsigned opcode =
      isRowMajor ? static_cast<unsigned>(HLSubscriptOpcode::RowMatSubscript)
                 : static_cast<unsigned>(HLSubscriptOpcode::ColMatSubscript);
  Value *matBase = Ptr;
  DXASSERT(matBase->getType()->isPointerTy(),
           "matrix subscript should return pointer");

  RetType =
      llvm::PointerType::get(RetType->getPointerElementType(),
                             matBase->getType()->getPointerAddressSpace());

  // Lower mat[Idx] into real idx.
  SmallVector<Value *, 8> args;
  args.emplace_back(Ptr);
  unsigned row, col;
  hlsl::GetHLSLMatRowColCount(Ty, row, col);
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
      IsRowMajorMatrix(Ty, m_pHLModule->GetHLOptions().bDefaultRowMajor);
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
  // For all zero idx. Still all zero idx.
  if (ConstantAggregateZero *zeros = dyn_cast<ConstantAggregateZero>(idx)) {
    Constant *zero = zeros->getAggregateElement((unsigned)0);
    std::vector<Constant *> elts(zeros->getNumElements() >> 1, zero);
    args[HLOperandIndex::kMatSubscriptSubOpIdx - 1] = ConstantVector::get(elts);
  } else {
    ConstantDataSequential *elts = cast<ConstantDataSequential>(idx);
    unsigned count = elts->getNumElements();
    unsigned row, col;
    hlsl::GetHLSLMatRowColCount(Ty, row, col);
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
      IsRowMajorMatrix(Ty, m_pHLModule->GetHLOptions().bDefaultRowMajor);
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
      IsRowMajorMatrix(Ty, m_pHLModule->GetHLOptions().bDefaultRowMajor);
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
  } else if (HLMatrixLower::IsMatrixType(Ty)) {
    // Use matLd/St for matrix.
    unsigned col, row;
    llvm::Type *EltTy = HLMatrixLower::GetMatrixInfo(Ty, col, row);
    llvm::PointerType *EltPtrTy =
        llvm::PointerType::get(EltTy, Ptr->getType()->getPointerAddressSpace());
    QualType EltQualTy = hlsl::GetHLSLMatElementType(Type);

    Value *matPtr = CGF.Builder.CreateInBoundsGEP(Ptr, idxList);

    // Flatten matrix to elements.
    for (unsigned r = 0; r < row; r++) {
      for (unsigned c = 0; c < col; c++) {
        ConstantInt *cRow = CGF.Builder.getInt32(r);
        ConstantInt *cCol = CGF.Builder.getInt32(c);
        Constant *CV = llvm::ConstantVector::get({cRow, cCol});
        GepList.push_back(
            EmitHLSLMatrixElement(CGF, EltPtrTy, {matPtr, CV}, Type));
        EltTyList.push_back(EltQualTy);
      }
    }

  } else if (StructType *ST = dyn_cast<StructType>(Ty)) {
    if (HLModule::IsHLSLObjectType(ST)) {
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

void CGMSHLSLRuntime::LoadFlattenedGepList(CodeGenFunction &CGF,
                                           ArrayRef<Value *> GepList,
                                           ArrayRef<QualType> EltTyList,
                                           SmallVector<Value *, 4> &EltList) {
  unsigned eltSize = GepList.size();
  for (unsigned i = 0; i < eltSize; i++) {
    Value *Ptr = GepList[i];
    // Everying is element type.
    EltList.push_back(CGF.Builder.CreateLoad(Ptr));
  }
}

void CGMSHLSLRuntime::StoreFlattenedGepList(CodeGenFunction &CGF, ArrayRef<Value *> GepList,
    ArrayRef<QualType> GepTyList, ArrayRef<Value *> EltValList, ArrayRef<QualType> SrcTyList) {
  unsigned eltSize = GepList.size();
  for (unsigned i = 0; i < eltSize; i++) {
    Value *Ptr = GepList[i];
    QualType DestType = GepTyList[i];
    Value *Val = EltValList[i];
    QualType SrcType = SrcTyList[i];

    llvm::Type *Ty = Ptr->getType()->getPointerElementType();
    // Everything is element type.
    if (Ty != Val->getType()) {
      Instruction::CastOps castOp =
          static_cast<Instruction::CastOps>(HLModule::FindCastOp(
              IsUnsigned(SrcType), IsUnsigned(DestType), Val->getType(), Ty));

      Val = CGF.Builder.CreateCast(castOp, Val, Ty);
    }
    CGF.Builder.CreateStore(Val, Ptr);
  }
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
  } else if (HLMatrixLower::IsMatrixType(Ty)) {
    // Use matLd/St for matrix.
    Value *srcGEP = CGF.Builder.CreateInBoundsGEP(SrcPtr, idxList);
    Value *dstGEP = CGF.Builder.CreateInBoundsGEP(DestPtr, idxList);
    Value *ldMat = EmitHLSLMatrixLoad(CGF, srcGEP, SrcType);
    EmitHLSLMatrixStore(CGF, ldMat, dstGEP, DestType);
  } else if (StructType *ST = dyn_cast<StructType>(Ty)) {
    if (HLModule::IsHLSLObjectType(ST)) {
      // Avoid split HLSL object.
      SimpleCopy(DestPtr, SrcPtr, idxList, CGF.Builder);
      return;
    }
    Value *srcGEP = CGF.Builder.CreateInBoundsGEP(SrcPtr, idxList);
    Value *dstGEP = CGF.Builder.CreateInBoundsGEP(DestPtr, idxList);
    unsigned size = this->TheModule.getDataLayout().getTypeAllocSize(ST);
    // Memcpy struct.
    CGF.Builder.CreateMemCpy(dstGEP, srcGEP, size, 1);
  } else if (llvm::ArrayType *AT = dyn_cast<llvm::ArrayType>(Ty)) {
    if (!HLMatrixLower::IsMatrixArrayPointer(llvm::PointerType::get(Ty,0))) {
      Value *srcGEP = CGF.Builder.CreateInBoundsGEP(SrcPtr, idxList);
      Value *dstGEP = CGF.Builder.CreateInBoundsGEP(DestPtr, idxList);
      unsigned size = this->TheModule.getDataLayout().getTypeAllocSize(AT);
      // Memcpy non-matrix array.
      CGF.Builder.CreateMemCpy(dstGEP, srcGEP, size, 1);
    } else {
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
    for (llvm::Type *EltTy : ST->elements()) {
      if (EltTy != Ty)
        return false;
    }
    return true;
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
    if (HLMatrixLower::IsMatrixArrayPointer(SrcPtr->getType())) {
      QualType SrcEltTy = GetArrayEltType(SrcTy);
      QualType DestEltTy = GetArrayEltType(DestTy);
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
  } else if (HLModule::IsHLSLObjectType(dxilutil::GetArrayEltTy(SrcPtrTy)) &&
             HLModule::IsHLSLObjectType(dxilutil::GetArrayEltTy(DestPtrTy))) {
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

  // It is possiable to implement EmitHLSLAggregateCopy, EmitHLSLAggregateStore
  // the same way. But split value to scalar will generate many instruction when
  // src type is same as dest type.
  SmallVector<Value *, 4> idxList;
  SmallVector<Value *, 4> SrcGEPList;
  SmallVector<QualType, 4> SrcEltTyList;
  FlattenAggregatePtrToGepList(CGF, SrcPtr, idxList, SrcTy, SrcPtr->getType(),
                               SrcGEPList, SrcEltTyList);

  SmallVector<Value *, 4> LdEltList;
  LoadFlattenedGepList(CGF, SrcGEPList, SrcEltTyList, LdEltList);

  idxList.clear();
  SmallVector<Value *, 4> DestGEPList;
  SmallVector<QualType, 4> DestEltTyList;
  FlattenAggregatePtrToGepList(CGF, DestPtr, idxList, DestTy,
                               DestPtr->getType(), DestGEPList, DestEltTyList);

  StoreFlattenedGepList(CGF, DestGEPList, DestEltTyList, LdEltList,
                        SrcEltTyList);
}

void CGMSHLSLRuntime::EmitHLSLAggregateStore(CodeGenFunction &CGF, llvm::Value *SrcVal,
    llvm::Value *DestPtr,
    clang::QualType Ty) {
    DXASSERT(0, "aggregate return type will use SRet, no aggregate store should exist");
}

static void SimpleFlatValCopy(Value *DestPtr, Value *SrcVal, QualType Ty,
                              QualType SrcTy, ArrayRef<Value *> idxList,
                              CGBuilderTy &Builder) {
  Value *DestGEP = Builder.CreateInBoundsGEP(DestPtr, idxList);
  llvm::Type *ToTy = DestGEP->getType()->getPointerElementType();

  llvm::Type *EltToTy = ToTy;
  if (llvm::VectorType *VT = dyn_cast<llvm::VectorType>(ToTy)) {
    EltToTy = VT->getElementType();
  }

  if (EltToTy != SrcVal->getType()) {
    Instruction::CastOps castOp =
        static_cast<Instruction::CastOps>(HLModule::FindCastOp(
            IsUnsigned(SrcTy), IsUnsigned(Ty), SrcVal->getType(), ToTy));

    SrcVal = Builder.CreateCast(castOp, SrcVal, EltToTy);
  }

  if (llvm::VectorType *VT = dyn_cast<llvm::VectorType>(ToTy)) {
    llvm::VectorType *VT1 = llvm::VectorType::get(EltToTy, 1);
    Value *V1 =
        Builder.CreateInsertElement(UndefValue::get(VT1), SrcVal, (uint64_t)0);
    std::vector<int> shufIdx(VT->getNumElements(), 0);
    Value *Vec = Builder.CreateShuffleVector(V1, V1, shufIdx);
    Builder.CreateStore(Vec, DestGEP);
  } else
    Builder.CreateStore(SrcVal, DestGEP);
}

void CGMSHLSLRuntime::EmitHLSLFlatConversionToAggregate(
    CodeGenFunction &CGF, Value *SrcVal, llvm::Value *DestPtr,
    SmallVector<Value *, 4> &idxList, QualType Type, QualType SrcType,
    llvm::Type *Ty) {
  if (llvm::PointerType *PT = dyn_cast<llvm::PointerType>(Ty)) {
    Constant *idx = Constant::getIntegerValue(
        IntegerType::get(Ty->getContext(), 32), APInt(32, 0));
    idxList.emplace_back(idx);

    EmitHLSLFlatConversionToAggregate(CGF, SrcVal, DestPtr, idxList, Type,
                                      SrcType, PT->getElementType());

    idxList.pop_back();
  } else if (HLMatrixLower::IsMatrixType(Ty)) {
    // Use matLd/St for matrix.
    Value *dstGEP = CGF.Builder.CreateInBoundsGEP(DestPtr, idxList);
    unsigned row, col;
    llvm::Type *EltTy = HLMatrixLower::GetMatrixInfo(Ty, col, row);

    llvm::VectorType *VT1 = llvm::VectorType::get(EltTy, 1);
    if (EltTy != SrcVal->getType()) {
      Instruction::CastOps castOp =
          static_cast<Instruction::CastOps>(HLModule::FindCastOp(
              IsUnsigned(SrcType), IsUnsigned(Type), SrcVal->getType(), EltTy));

      SrcVal = CGF.Builder.CreateCast(castOp, SrcVal, EltTy);
    }

    Value *V1 = CGF.Builder.CreateInsertElement(UndefValue::get(VT1), SrcVal,
                                                (uint64_t)0);
    std::vector<int> shufIdx(col * row, 0);

    Value *VecMat = CGF.Builder.CreateShuffleVector(V1, V1, shufIdx);
    Value *MatInit = EmitHLSLMatrixOperationCallImp(
        CGF.Builder, HLOpcodeGroup::HLInit, 0, Ty, {VecMat}, TheModule);
    EmitHLSLMatrixStore(CGF, MatInit, dstGEP, Type);
  } else if (StructType *ST = dyn_cast<StructType>(Ty)) {
    DXASSERT(!HLModule::IsHLSLObjectType(ST), "cannot cast to hlsl object, Sema should reject");

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
          EmitHLSLFlatConversionToAggregate(CGF, SrcVal, DestPtr, idxList,
                                            parentTy, SrcType, ET);
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

      EmitHLSLFlatConversionToAggregate(CGF, SrcVal, DestPtr, idxList,
                                        fieldIter->getType(), SrcType, ET);

      idxList.pop_back();
    }

  } else if (llvm::ArrayType *AT = dyn_cast<llvm::ArrayType>(Ty)) {
    llvm::Type *ET = AT->getElementType();

    QualType EltType = CGF.getContext().getBaseElementType(Type);

    for (uint32_t i = 0; i < AT->getNumElements(); i++) {
      Constant *idx = Constant::getIntegerValue(
          IntegerType::get(Ty->getContext(), 32), APInt(32, i));
      idxList.emplace_back(idx);

      EmitHLSLFlatConversionToAggregate(CGF, SrcVal, DestPtr, idxList, EltType,
                                        SrcType, ET);

      idxList.pop_back();
    }
  } else {
    SimpleFlatValCopy(DestPtr, SrcVal, Type, SrcType, idxList, CGF.Builder);
  }
}

void CGMSHLSLRuntime::EmitHLSLFlatConversionToAggregate(CodeGenFunction &CGF,
                                                        Value *Val,
                                                        Value *DestPtr,
                                                        QualType Ty,
                                                        QualType SrcTy) {
  if (SrcTy->isBuiltinType()) {
    SmallVector<Value *, 4> idxList;
    // Add first 0 for DestPtr.
    Constant *idx = Constant::getIntegerValue(
        IntegerType::get(Val->getContext(), 32), APInt(32, 0));
    idxList.emplace_back(idx);

    EmitHLSLFlatConversionToAggregate(
        CGF, Val, DestPtr, idxList, Ty, SrcTy,
        DestPtr->getType()->getPointerElementType());
  }
  else {
    SmallVector<Value *, 4> idxList;
    SmallVector<Value *, 4> DestGEPList;
    SmallVector<QualType, 4> DestEltTyList;
    FlattenAggregatePtrToGepList(CGF, DestPtr, idxList, Ty, DestPtr->getType(), DestGEPList, DestEltTyList);

    SmallVector<Value *, 4> EltList;
    SmallVector<QualType, 4> EltTyList;
    FlattenValToInitList(CGF, EltList, EltTyList, SrcTy, Val);

    StoreFlattenedGepList(CGF, DestGEPList, DestEltTyList, EltList, EltTyList);
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

  clang::CompileRootSignature(StrRef, Diags, SLoc, rootSigVer, &m_pHLModule->GetRootSignature());
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
    bool RValOnRef = false;
    if (!Param->isModifierOut()) {
      if (!ParamTy->isAggregateType() || hlsl::IsHLSLMatType(ParamTy)) {
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
      }
    }

    // get original arg
    LValue argLV = CGF.EmitLValue(Arg);

    if (!Param->isModifierOut() && !RValOnRef) {
      bool isDefaultAddrSpace = true;
      if (argLV.isSimple()) {
        isDefaultAddrSpace =
            argLV.getAddress()->getType()->getPointerAddressSpace() ==
            DXIL::kDefaultAddrSpace;
      }
      bool isHLSLIntrinsic = false;
      if (const FunctionDecl *Callee = E->getDirectCallee()) {
        isHLSLIntrinsic = Callee->hasAttr<HLSLIntrinsicAttr>();
      }
      // Copy in arg which is not default address space and not on hlsl intrinsic.
      if (isDefaultAddrSpace || isHLSLIntrinsic)
        continue;
    }

    // create temp Var
    VarDecl *tmpArg =
        VarDecl::Create(CGF.getContext(), const_cast<FunctionDecl *>(FD),
                        SourceLocation(), SourceLocation(),
                        /*IdentifierInfo*/ nullptr, ParamTy,
                        CGF.getContext().getTrivialTypeSourceInfo(ParamTy),
                        StorageClass::SC_Auto);

    // Aggregate type will be indirect param convert to pointer type.
    // So don't update to ReferenceType, use RValue for it.
    bool isAggregateType = (ParamTy->isArrayType() || ParamTy->isRecordType()) &&
      !hlsl::IsHLSLVecMatType(ParamTy);

    const DeclRefExpr *tmpRef = DeclRefExpr::Create(
        CGF.getContext(), NestedNameSpecifierLoc(), SourceLocation(), tmpArg,
        /*enclosing*/ false, tmpArg->getLocation(), ParamTy,
        isAggregateType ? VK_RValue : VK_LValue);

    // update the arg
    argList[i] = tmpRef;

    // create alloc for the tmp arg
    Value *tmpArgAddr = nullptr;
    BasicBlock *InsertBlock = CGF.Builder.GetInsertBlock();
    Function *F = InsertBlock->getParent();

    if (ParamTy->isBooleanType()) {
      // Create i32 for bool.
      ParamTy = CGM.getContext().IntTy;
    }
    // Make sure the alloca is in entry block to stop inline create stacksave.
    IRBuilder<> AllocaBuilder(dxilutil::FindAllocaInsertionPt(F));
    tmpArgAddr = AllocaBuilder.CreateAlloca(CGF.ConvertType(ParamTy));

      
    // add it to local decl map
    TmpArgMap(tmpArg, tmpArgAddr);

    LValue tmpLV = LValue::MakeAddr(tmpArgAddr, ParamTy, argLV.getAlignment(),
                                    CGF.getContext());

    // save for cast after call
    if (Param->isModifierOut()) {
      castArgList.emplace_back(tmpLV);
      castArgList.emplace_back(argLV);
    }

    bool isObject = HLModule::IsHLSLObjectType(
        tmpArgAddr->getType()->getPointerElementType());

    // cast before the call
    if (Param->isModifierIn() &&
        // Don't copy object
        !isObject) {
      QualType ArgTy = Arg->getType();
      Value *outVal = nullptr;
      bool isAggrageteTy = ParamTy->isAggregateType();
      isAggrageteTy &= !IsHLSLVecMatType(ParamTy);
      if (!isAggrageteTy) {
        if (!IsHLSLMatType(ParamTy)) {
          RValue outRVal = CGF.EmitLoadOfLValue(argLV, SourceLocation());
          outVal = outRVal.getScalarVal();
        } else {
          Value *argAddr = argLV.getAddress();
          outVal = EmitHLSLMatrixLoad(CGF, argAddr, ArgTy);
        }

        llvm::Type *ToTy = tmpArgAddr->getType()->getPointerElementType();
        Instruction::CastOps castOp =
            static_cast<Instruction::CastOps>(HLModule::FindCastOp(
                IsUnsigned(argLV.getType()), IsUnsigned(tmpLV.getType()),
                outVal->getType(), ToTy));

        Value *castVal = CGF.Builder.CreateCast(castOp, outVal, ToTy);
        if (!HLMatrixLower::IsMatrixType(ToTy))
          CGF.Builder.CreateStore(castVal, tmpArgAddr);
        else
          EmitHLSLMatrixStore(CGF, castVal, tmpArgAddr, ParamTy);
      } else {
        SmallVector<Value *, 4> idxList;
        EmitHLSLAggregateCopy(CGF, argLV.getAddress(), tmpLV.getAddress(),
                              idxList, ArgTy, ParamTy,
                              argLV.getAddress()->getType());
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

    bool isAggrageteTy = ArgTy->isAggregateType();
    isAggrageteTy &= !IsHLSLVecMatType(ArgTy);

    bool isObject = HLModule::IsHLSLObjectType(
       tmpArgAddr->getType()->getPointerElementType());
    if (!isObject) {
      if (!isAggrageteTy) {
        if (!IsHLSLMatType(ParamTy))
          outVal = CGF.Builder.CreateLoad(tmpArgAddr);
        else
          outVal = EmitHLSLMatrixLoad(CGF, tmpArgAddr, ParamTy);

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
          Instruction::CastOps castOp =
              static_cast<Instruction::CastOps>(HLModule::FindCastOp(
                  IsUnsigned(tmpLV.getType()), IsUnsigned(argLV.getType()),
                  outVal->getType(), ToTy));

          castVal = CGF.Builder.CreateCast(castOp, outVal, ToTy);
        }
        if (!HLMatrixLower::IsMatrixType(ToTy))
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
