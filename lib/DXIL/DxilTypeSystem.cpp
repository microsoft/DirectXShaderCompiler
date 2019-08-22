///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilTypeSystem.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilTypeSystem.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/WinFunctions.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using std::unique_ptr;
using std::string;
using std::vector;
using std::map;


namespace hlsl {

//------------------------------------------------------------------------------
//
// DxilMatrixAnnotation class methods.
//
DxilMatrixAnnotation::DxilMatrixAnnotation()
: Rows(0)
, Cols(0)
, Orientation(MatrixOrientation::Undefined) {
}


//------------------------------------------------------------------------------
//
// DxilFieldAnnotation class methods.
//
DxilFieldAnnotation::DxilFieldAnnotation()
: m_bPrecise(false)
, m_ResourceAttribute(nullptr)
, m_CBufferOffset(UINT_MAX)
, m_bCBufferVarUsed(false)
{}

bool DxilFieldAnnotation::IsPrecise() const { return m_bPrecise; }
void DxilFieldAnnotation::SetPrecise(bool b) { m_bPrecise = b; }
bool DxilFieldAnnotation::HasMatrixAnnotation() const { return m_Matrix.Cols != 0; }
const DxilMatrixAnnotation &DxilFieldAnnotation::GetMatrixAnnotation() const { return m_Matrix; }
void DxilFieldAnnotation::SetMatrixAnnotation(const DxilMatrixAnnotation &MA) { m_Matrix = MA; }
bool DxilFieldAnnotation::HasResourceAttribute() const {
  return m_ResourceAttribute;
}
llvm::MDNode *DxilFieldAnnotation::GetResourceAttribute() const {
  return m_ResourceAttribute;
}
void DxilFieldAnnotation::SetResourceAttribute(llvm::MDNode *MD) {
  m_ResourceAttribute = MD;
}
bool DxilFieldAnnotation::HasCBufferOffset() const { return m_CBufferOffset != UINT_MAX; }
unsigned DxilFieldAnnotation::GetCBufferOffset() const { return m_CBufferOffset; }
void DxilFieldAnnotation::SetCBufferOffset(unsigned Offset) { m_CBufferOffset = Offset; }
bool DxilFieldAnnotation::HasCompType() const { return m_CompType.GetKind() != CompType::Kind::Invalid; }
const CompType &DxilFieldAnnotation::GetCompType() const { return m_CompType; }
void DxilFieldAnnotation::SetCompType(CompType::Kind kind) { m_CompType = CompType(kind); }
bool DxilFieldAnnotation::HasSemanticString() const { return !m_Semantic.empty(); }
const std::string &DxilFieldAnnotation::GetSemanticString() const { return m_Semantic; }
llvm::StringRef DxilFieldAnnotation::GetSemanticStringRef() const { return llvm::StringRef(m_Semantic); }
void DxilFieldAnnotation::SetSemanticString(const std::string &SemString) { m_Semantic = SemString; }
bool DxilFieldAnnotation::HasInterpolationMode() const { return !m_InterpMode.IsUndefined(); }
const InterpolationMode &DxilFieldAnnotation::GetInterpolationMode() const { return m_InterpMode; }
void DxilFieldAnnotation::SetInterpolationMode(const InterpolationMode &IM) { m_InterpMode = IM; }
bool DxilFieldAnnotation::HasFieldName() const { return !m_FieldName.empty(); }
const std::string &DxilFieldAnnotation::GetFieldName() const { return m_FieldName; }
void DxilFieldAnnotation::SetFieldName(const std::string &FieldName) { m_FieldName = FieldName; }
bool DxilFieldAnnotation::IsCBVarUsed() const { return m_bCBufferVarUsed; }
void DxilFieldAnnotation::SetCBVarUsed(bool used) { m_bCBufferVarUsed = used; }



//------------------------------------------------------------------------------
//
// DxilStructAnnotation class methods.
//
DxilTemplateArgAnnotation::DxilTemplateArgAnnotation()
    : DxilFieldAnnotation(), m_Type(nullptr), m_Integral(0)
{}

bool DxilTemplateArgAnnotation::IsType() const { return m_Type != nullptr; }
const llvm::Type *DxilTemplateArgAnnotation::GetType() const { return m_Type; }
void DxilTemplateArgAnnotation::SetType(const llvm::Type *pType) { m_Type = pType; }

bool DxilTemplateArgAnnotation::IsIntegral() const { return m_Type == nullptr; }
int64_t DxilTemplateArgAnnotation::GetIntegral() const { return m_Integral; }
void DxilTemplateArgAnnotation::SetIntegral(int64_t i64) { m_Type = nullptr; m_Integral = i64; }

//------------------------------------------------------------------------------
//
// DxilStructAnnotation class methods.
//
unsigned DxilStructAnnotation::GetNumFields() const {
  return (unsigned)m_FieldAnnotations.size();
}

DxilFieldAnnotation &DxilStructAnnotation::GetFieldAnnotation(unsigned FieldIdx) {
  return m_FieldAnnotations[FieldIdx];
}

const DxilFieldAnnotation &DxilStructAnnotation::GetFieldAnnotation(unsigned FieldIdx) const {
  return m_FieldAnnotations[FieldIdx];
}

const StructType *DxilStructAnnotation::GetStructType() const {
  return m_pStructType;
}
void DxilStructAnnotation::SetStructType(const llvm::StructType *Ty) {
  m_pStructType = Ty;
}


unsigned DxilStructAnnotation::GetCBufferSize() const { return m_CBufferSize; }
void DxilStructAnnotation::SetCBufferSize(unsigned size) { m_CBufferSize = size; }
void DxilStructAnnotation::MarkEmptyStruct() { m_FieldAnnotations.clear(); }
bool DxilStructAnnotation::IsEmptyStruct() { return m_FieldAnnotations.empty(); }

// For template args, GetNumTemplateArgs() will return 0 if not a template
unsigned DxilStructAnnotation::GetNumTemplateArgs() const {
  return (unsigned)m_TemplateAnnotations.size();
}
void DxilStructAnnotation::SetNumTemplateArgs(unsigned count) {
  DXASSERT(m_TemplateAnnotations.empty(), "template args already initialized");
  m_TemplateAnnotations.resize(count);
}
DxilTemplateArgAnnotation &DxilStructAnnotation::GetTemplateArgAnnotation(unsigned argIdx) {
  return m_TemplateAnnotations[argIdx];
}
const DxilTemplateArgAnnotation &DxilStructAnnotation::GetTemplateArgAnnotation(unsigned argIdx) const {
  return m_TemplateAnnotations[argIdx];
}


//------------------------------------------------------------------------------
//
// DxilParameterAnnotation class methods.
//
DxilParameterAnnotation::DxilParameterAnnotation()
: DxilFieldAnnotation(), m_inputQual(DxilParamInputQual::In) {
}

DxilParamInputQual DxilParameterAnnotation::GetParamInputQual() const {
  return m_inputQual;
}
void DxilParameterAnnotation::SetParamInputQual(DxilParamInputQual qual) {
  m_inputQual = qual;
}

const std::vector<unsigned> &DxilParameterAnnotation::GetSemanticIndexVec() const {
  return m_semanticIndex;
}

void DxilParameterAnnotation::SetSemanticIndexVec(const std::vector<unsigned> &Vec) {
  m_semanticIndex = Vec;
}

void DxilParameterAnnotation::AppendSemanticIndex(unsigned SemIdx) {
  m_semanticIndex.emplace_back(SemIdx);
}

//------------------------------------------------------------------------------
//
// DxilFunctionAnnotation class methods.
//
unsigned DxilFunctionAnnotation::GetNumParameters() const {
  return (unsigned)m_parameterAnnotations.size();
}

DxilParameterAnnotation &DxilFunctionAnnotation::GetParameterAnnotation(unsigned ParamIdx) {
  return m_parameterAnnotations[ParamIdx];
}

const DxilParameterAnnotation &DxilFunctionAnnotation::GetParameterAnnotation(unsigned ParamIdx) const {
  return m_parameterAnnotations[ParamIdx];
}

DxilParameterAnnotation &DxilFunctionAnnotation::GetRetTypeAnnotation() {
  return m_retTypeAnnotation;
}

const DxilParameterAnnotation &DxilFunctionAnnotation::GetRetTypeAnnotation() const {
  return m_retTypeAnnotation;
}

const Function *DxilFunctionAnnotation::GetFunction() const {
  return m_pFunction;
}

//------------------------------------------------------------------------------
//
// DxilStructAnnotationSystem class methods.
//
DxilTypeSystem::DxilTypeSystem(Module *pModule)
    : m_pModule(pModule),
      m_LowPrecisionMode(DXIL::LowPrecisionMode::Undefined) {}

DxilStructAnnotation *DxilTypeSystem::AddStructAnnotation(const StructType *pStructType, unsigned numTemplateArgs) {
  DXASSERT_NOMSG(m_StructAnnotations.find(pStructType) == m_StructAnnotations.end());
  DxilStructAnnotation *pA = new DxilStructAnnotation();
  m_StructAnnotations[pStructType] = unique_ptr<DxilStructAnnotation>(pA);
  pA->m_pStructType = pStructType;
  pA->m_FieldAnnotations.resize(pStructType->getNumElements());
  pA->SetNumTemplateArgs(numTemplateArgs);
  return pA;
}

DxilStructAnnotation *DxilTypeSystem::GetStructAnnotation(const StructType *pStructType) {
  auto it = m_StructAnnotations.find(pStructType);
  if (it != m_StructAnnotations.end()) {
    return it->second.get();
  } else {
    return nullptr;
  }
}

const DxilStructAnnotation *
DxilTypeSystem::GetStructAnnotation(const StructType *pStructType) const {
  auto it = m_StructAnnotations.find(pStructType);
  if (it != m_StructAnnotations.end()) {
    return it->second.get();
  } else {
    return nullptr;
  }
}

void DxilTypeSystem::EraseStructAnnotation(const StructType *pStructType) {
  DXASSERT_NOMSG(m_StructAnnotations.count(pStructType));
  m_StructAnnotations.remove_if([pStructType](
      const std::pair<const StructType *, std::unique_ptr<DxilStructAnnotation>>
          &I) { return pStructType == I.first; });
}

DxilTypeSystem::StructAnnotationMap &DxilTypeSystem::GetStructAnnotationMap() {
  return m_StructAnnotations;
}

DxilFunctionAnnotation *DxilTypeSystem::AddFunctionAnnotation(const Function *pFunction) {
  DXASSERT_NOMSG(m_FunctionAnnotations.find(pFunction) == m_FunctionAnnotations.end());
  DxilFunctionAnnotation *pA = new DxilFunctionAnnotation();
  m_FunctionAnnotations[pFunction] = unique_ptr<DxilFunctionAnnotation>(pA);
  pA->m_pFunction = pFunction;
  pA->m_parameterAnnotations.resize(pFunction->getFunctionType()->getNumParams());
  return pA;
}

DxilFunctionAnnotation *DxilTypeSystem::GetFunctionAnnotation(const Function *pFunction) {
  auto it = m_FunctionAnnotations.find(pFunction);
  if (it != m_FunctionAnnotations.end()) {
    return it->second.get();
  } else {
    return nullptr;
  }
}

const DxilFunctionAnnotation *
DxilTypeSystem::GetFunctionAnnotation(const Function *pFunction) const {
  auto it = m_FunctionAnnotations.find(pFunction);
  if (it != m_FunctionAnnotations.end()) {
    return it->second.get();
  } else {
    return nullptr;
  }
}

void DxilTypeSystem::EraseFunctionAnnotation(const Function *pFunction) {
  DXASSERT_NOMSG(m_FunctionAnnotations.count(pFunction));
  m_FunctionAnnotations.remove_if([pFunction](
      const std::pair<const Function *, std::unique_ptr<DxilFunctionAnnotation>>
          &I) { return pFunction == I.first; });
}

DxilTypeSystem::FunctionAnnotationMap &DxilTypeSystem::GetFunctionAnnotationMap() {
  return m_FunctionAnnotations;
}

StructType *DxilTypeSystem::GetSNormF32Type(unsigned NumComps) {
  return GetNormFloatType(CompType::getSNormF32(), NumComps);
}

StructType *DxilTypeSystem::GetUNormF32Type(unsigned NumComps) {
  return GetNormFloatType(CompType::getUNormF32(), NumComps);
}

StructType *DxilTypeSystem::GetNormFloatType(CompType CT, unsigned NumComps) {
  Type *pCompType = CT.GetLLVMType(m_pModule->getContext());
  DXASSERT_NOMSG(pCompType->isFloatTy());
  Type *pFieldType = pCompType;
  string TypeName;
  raw_string_ostream NameStream(TypeName);
  if (NumComps > 1) {
    (NameStream << "dx.types." << NumComps << "x" << CT.GetName()).flush();
    pFieldType = VectorType::get(pFieldType, NumComps);
  } else {
    (NameStream << "dx.types." << CT.GetName()).flush();
  }
  StructType *pStructType = m_pModule->getTypeByName(TypeName);
  if (pStructType == nullptr) {
    pStructType = StructType::create(m_pModule->getContext(), pFieldType, TypeName);
    DxilStructAnnotation &TA = *AddStructAnnotation(pStructType);
    DxilFieldAnnotation &FA = TA.GetFieldAnnotation(0);
    FA.SetCompType(CT.GetKind());
    DXASSERT_NOMSG(CT.IsSNorm() || CT.IsUNorm());
  }
  return pStructType;
}

void DxilTypeSystem::CopyTypeAnnotation(const llvm::Type *Ty,
                                        const DxilTypeSystem &src) {
  if (isa<PointerType>(Ty))
    Ty = Ty->getPointerElementType();

  while (isa<ArrayType>(Ty))
    Ty = Ty->getArrayElementType();

  // Only struct type has annotation.
  if (!isa<StructType>(Ty))
    return;

  const StructType *ST = cast<StructType>(Ty);
  // Already exist.
  if (GetStructAnnotation(ST))
    return;

  if (const DxilStructAnnotation *annot = src.GetStructAnnotation(ST)) {
    DxilStructAnnotation *dstAnnot = AddStructAnnotation(ST);
    // Copy the annotation.
    *dstAnnot = *annot;
    // Copy field type annotations.
    for (Type *Ty : ST->elements()) {
      CopyTypeAnnotation(Ty, src);
    }
  }
}

void DxilTypeSystem::CopyFunctionAnnotation(const llvm::Function *pDstFunction,
                                            const llvm::Function *pSrcFunction,
                                            const DxilTypeSystem &src) {
  const DxilFunctionAnnotation *annot = src.GetFunctionAnnotation(pSrcFunction);
  // Don't have annotation.
  if (!annot)
    return;
  // Already exist.
  if (GetFunctionAnnotation(pDstFunction))
    return;

  DxilFunctionAnnotation *dstAnnot = AddFunctionAnnotation(pDstFunction);

  // Copy the annotation.
  *dstAnnot = *annot;
  dstAnnot->m_pFunction = pDstFunction;
  // Clone ret type annotation.
  CopyTypeAnnotation(pDstFunction->getReturnType(), src);
  // Clone param type annotations.
  for (const Argument &arg : pDstFunction->args()) {
    CopyTypeAnnotation(arg.getType(), src);
  }
}

DXIL::SigPointKind SigPointFromInputQual(DxilParamInputQual Q, DXIL::ShaderKind SK, bool isPC) {
  DXASSERT(Q != DxilParamInputQual::Inout, "Inout not expected for SigPointFromInputQual");
  switch (SK) {
  case DXIL::ShaderKind::Vertex:
    switch (Q) {
    case DxilParamInputQual::In:
      return DXIL::SigPointKind::VSIn;
    case DxilParamInputQual::Out:
      return DXIL::SigPointKind::VSOut;
    default:
      break;
    }
    break;
  case DXIL::ShaderKind::Hull:
    switch (Q) {
    case DxilParamInputQual::In:
      if (isPC)
        return DXIL::SigPointKind::PCIn;
      else
        return DXIL::SigPointKind::HSIn;
    case DxilParamInputQual::Out:
      if (isPC)
        return DXIL::SigPointKind::PCOut;
      else
        return DXIL::SigPointKind::HSCPOut;
    case DxilParamInputQual::InputPatch:
      return DXIL::SigPointKind::HSCPIn;
    case DxilParamInputQual::OutputPatch:
      return DXIL::SigPointKind::HSCPOut;
    default:
      break;
    }
    break;
  case DXIL::ShaderKind::Domain:
    switch (Q) {
    case DxilParamInputQual::In:
      return DXIL::SigPointKind::DSIn;
    case DxilParamInputQual::Out:
      return DXIL::SigPointKind::DSOut;
    case DxilParamInputQual::InputPatch:
    case DxilParamInputQual::OutputPatch:
      return DXIL::SigPointKind::DSCPIn;
    default:
      break;
    }
    break;
  case DXIL::ShaderKind::Geometry:
    switch (Q) {
    case DxilParamInputQual::In:
      return DXIL::SigPointKind::GSIn;
    case DxilParamInputQual::InputPrimitive:
      return DXIL::SigPointKind::GSVIn;
    case DxilParamInputQual::OutStream0:
    case DxilParamInputQual::OutStream1:
    case DxilParamInputQual::OutStream2:
    case DxilParamInputQual::OutStream3:
      return DXIL::SigPointKind::GSOut;
    default:
      break;
    }
    break;
  case DXIL::ShaderKind::Pixel:
    switch (Q) {
    case DxilParamInputQual::In:
      return DXIL::SigPointKind::PSIn;
    case DxilParamInputQual::Out:
      return DXIL::SigPointKind::PSOut;
    default:
      break;
    }
    break;
  case DXIL::ShaderKind::Compute:
    switch (Q) {
    case DxilParamInputQual::In:
      return DXIL::SigPointKind::CSIn;
    default:
      break;
    }
    break;
  case DXIL::ShaderKind::Mesh:
    switch (Q) {
    case DxilParamInputQual::In:
    case DxilParamInputQual::InPayload:
      return DXIL::SigPointKind::MSIn;
    case DxilParamInputQual::OutIndices:
    case DxilParamInputQual::OutVertices:
      return DXIL::SigPointKind::MSOut;
    case DxilParamInputQual::OutPrimitives:
      return DXIL::SigPointKind::MSPOut;
    default:
      break;
    }
    break;
  case DXIL::ShaderKind::Amplification:
    switch (Q) {
    case DxilParamInputQual::In:
      return DXIL::SigPointKind::ASIn;
    default:
      break;
    }
    break;
  default:
    break;
  }
  return DXIL::SigPointKind::Invalid;
}

void RemapSemantic(llvm::StringRef &oldSemName, llvm::StringRef &oldSemFullName, const char *newSemName,
  DxilParameterAnnotation &paramInfo, llvm::LLVMContext &Context) {
  // format deprecation warning
  Context.emitWarning(Twine("DX9-style semantic \"") + oldSemName + Twine("\" mapped to DX10 system semantic \"") + newSemName +
    Twine("\" due to -Gec flag. This functionality is deprecated in newer language versions."));

  // create new semantic name with the same index
  std::string newSemNameStr(newSemName);
  unsigned indexLen = oldSemFullName.size() - oldSemName.size();
  if (indexLen > 0) {
    newSemNameStr = newSemNameStr.append(oldSemFullName.data() + oldSemName.size(), indexLen);
  }

  paramInfo.SetSemanticString(newSemNameStr);
}

void RemapObsoleteSemantic(DxilParameterAnnotation &paramInfo, DXIL::SigPointKind sigPoint, llvm::LLVMContext &Context) {
  DXASSERT(paramInfo.HasSemanticString(), "expected paramInfo with semantic");
  //*ppWarningMsg = nullptr;

  llvm::StringRef semFullName = paramInfo.GetSemanticStringRef();
  llvm::StringRef semName;
  unsigned semIndex;
  Semantic::DecomposeNameAndIndex(semFullName, &semName, &semIndex);

  if (sigPoint == DXIL::SigPointKind::PSOut) {
    if (semName.size() == 5) {
      if (_strnicmp(semName.data(), "COLOR", 5) == 0) {
        RemapSemantic(semName, semFullName, "SV_Target", paramInfo, Context);
      }
      else if (_strnicmp(semName.data(), "DEPTH", 5) == 0) {
        RemapSemantic(semName, semFullName, "SV_Depth", paramInfo, Context);
      }
    }
  }
  else if ((sigPoint == DXIL::SigPointKind::VSOut && semName.size() == 8 && _strnicmp(semName.data(), "POSITION", 8) == 0) ||
           (sigPoint == DXIL::SigPointKind::PSIn  && semName.size() == 4 && _strnicmp(semName.data(), "VPOS", 4) == 0)) {
    RemapSemantic(semName, semFullName, "SV_Position", paramInfo, Context);
  }
}

bool DxilTypeSystem::UseMinPrecision() {
  return m_LowPrecisionMode == DXIL::LowPrecisionMode::UseMinPrecision;
}

void DxilTypeSystem::SetMinPrecision(bool bMinPrecision) {
  DXIL::LowPrecisionMode mode =
      bMinPrecision ? DXIL::LowPrecisionMode::UseMinPrecision
                    : DXIL::LowPrecisionMode::UseNativeLowPrecision;
  DXASSERT((mode == m_LowPrecisionMode ||
            m_LowPrecisionMode == DXIL::LowPrecisionMode::Undefined),
           "LowPrecisionMode should only be set once.");

  m_LowPrecisionMode = mode;
}

DxilStructTypeIterator::DxilStructTypeIterator(llvm::StructType *sTy, DxilStructAnnotation *sAnnotation,
  unsigned idx)
  : STy(sTy), SAnnotation(sAnnotation), index(idx) {
  DXASSERT(
    sTy->getNumElements() == sAnnotation->GetNumFields(),
    "Otherwise the pairing of annotation and struct type does not match.");
}

// prefix
DxilStructTypeIterator &DxilStructTypeIterator::operator++() {
  index++;
  return *this;
}
// postfix
DxilStructTypeIterator DxilStructTypeIterator::operator++(int) {
  DxilStructTypeIterator iter(STy, SAnnotation, index);
  index++;
  return iter;
}

bool DxilStructTypeIterator::operator==(DxilStructTypeIterator iter) {
  return iter.STy == STy && iter.SAnnotation == SAnnotation &&
    iter.index == index;
}

bool DxilStructTypeIterator::operator!=(DxilStructTypeIterator iter) { return !(operator==(iter)); }

std::pair<llvm::Type *, DxilFieldAnnotation *> DxilStructTypeIterator::operator*() {
  return std::pair<llvm::Type *, DxilFieldAnnotation *>(
    STy->getElementType(index), &SAnnotation->GetFieldAnnotation(index));
}

DxilStructTypeIterator begin(llvm::StructType *STy, DxilStructAnnotation *SAnno) {
  return { STy, SAnno, 0 };
}

DxilStructTypeIterator end(llvm::StructType *STy, DxilStructAnnotation *SAnno) {
  return { STy, SAnno, STy->getNumElements() };
}

} // namespace hlsl
