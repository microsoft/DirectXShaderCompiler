///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcutil.cpp                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides helper class for dxcompiler.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/Support/WinIncludes.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/dxcapi.h"
#include "dxcutil.h"
#include "dxillib.h"
#include "clang/Basic/Diagnostic.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilResource.h"
#include "dxc/HLSL/HLMatrixLowerHelper.h"
#include "dxc/HLSL/DxilConstants.h"
#include "dxc/HLSL/DxilOperations.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/Format.h"
#include "dxc/HLSL/DxilPipelineStateValidation.h"

using namespace llvm;
using namespace hlsl;

// This declaration is used for the locally-linked validator.
HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID *ppv);
// This internal call allows the validator to avoid having to re-deserialize
// the module. It trusts that the caller didn't make any changes and is
// kept internal because the layout of the module class may change based
// on changes across modules, or picking a different compiler version or CRT.
HRESULT RunInternalValidator(_In_ IDxcValidator *pValidator,
                             _In_ llvm::Module *pModule,
                             _In_ llvm::Module *pDebugModule,
                             _In_ IDxcBlob *pShader, UINT32 Flags,
                             _In_ IDxcOperationResult **ppResult);

namespace {
// AssembleToContainer helper functions.

bool CreateValidator(CComPtr<IDxcValidator> &pValidator) {
  if (DxilLibIsEnabled()) {
    DxilLibCreateInstance(CLSID_DxcValidator, &pValidator);
  }
  bool bInternalValidator = false;
  if (pValidator == nullptr) {
    IFT(CreateDxcValidator(IID_PPV_ARGS(&pValidator)));
    bInternalValidator = true;
  }
  return bInternalValidator;
}

// Class to manage lifetime of llvm module and provide some utility
// functions used for generating compiler output.
class DxilCompilerLLVMModuleOutput {
public:
  DxilCompilerLLVMModuleOutput(std::unique_ptr<llvm::Module> module)
      : m_llvmModule(std::move(module)) {}

  void CloneForDebugInfo() {
    m_llvmModuleWithDebugInfo.reset(llvm::CloneModule(m_llvmModule.get()));
  }

  void WrapModuleInDxilContainer(IMalloc *pMalloc,
                                 AbstractMemoryStream *pModuleBitcode,
                                 CComPtr<IDxcBlob> &pDxilContainerBlob,
                                 SerializeDxilFlags Flags) {
    CComPtr<AbstractMemoryStream> pContainerStream;
    IFT(CreateMemoryStream(pMalloc, &pContainerStream));
    SerializeDxilContainerForModule(&m_llvmModule->GetOrCreateDxilModule(),
                                    pModuleBitcode, pContainerStream, Flags);

    pDxilContainerBlob.Release();
    IFT(pContainerStream.QueryInterface(&pDxilContainerBlob));
  }

  llvm::Module *get() { return m_llvmModule.get(); }
  llvm::Module *getWithDebugInfo() { return m_llvmModuleWithDebugInfo.get(); }

private:
  std::unique_ptr<llvm::Module> m_llvmModule;
  std::unique_ptr<llvm::Module> m_llvmModuleWithDebugInfo;
};

} // namespace

namespace {
// Disassemble helper functions.

void PrintDiagnosticHandler(const DiagnosticInfo &DI, void *Context) {
  DiagnosticPrinter *printer = reinterpret_cast<DiagnosticPrinter *>(Context);
  DI.print(*printer);
}

template <typename T>
const T *ByteOffset(LPCVOID p, uint32_t byteOffset) {
  return reinterpret_cast<const T *>((const uint8_t *)p + byteOffset);
}
bool SigElementHasStream(const DxilProgramSignatureElement &pSignature) {
  return pSignature.Stream != 0;
}

void PrintSignature(LPCSTR pName, const DxilProgramSignature *pSignature,
                           bool bIsInput, raw_string_ostream &OS,
                           StringRef comment) {
  OS << comment << "\n"
     << comment << " " << pName << " signature:\n"
     << comment << "\n"
     << comment
     << " Name                 Index   Mask Register SysValue  Format   Used\n"
     << comment
     << " -------------------- ----- ------ -------- -------- ------- ------\n";

  if (pSignature->ParamCount == 0) {
    OS << comment << " no parameters\n";
    return;
  }

  const DxilProgramSignatureElement *pSigBegin =
      ByteOffset<DxilProgramSignatureElement>(pSignature,
                                              pSignature->ParamOffset);
  const DxilProgramSignatureElement *pSigEnd =
      pSigBegin + pSignature->ParamCount;

  bool bHasStreams = std::any_of(pSigBegin, pSigEnd, SigElementHasStream);
  for (const DxilProgramSignatureElement *pSig = pSigBegin; pSig != pSigEnd;
       ++pSig) {
    OS << comment << " ";
    const char *pSemanticName =
        ByteOffset<char>(pSignature, pSig->SemanticName);
    if (bHasStreams) {
      OS << "m" << pSig->Stream << ":";
      OS << left_justify(pSemanticName, 17);
    } else {
      OS << left_justify(pSemanticName, 20);
    }

    OS << ' ' << format("%5u", pSig->SemanticIndex);

    char Mask[4];
    memset(Mask, ' ', sizeof(Mask));

    if (pSig->Mask & DxilProgramSigMaskX)
      Mask[0] = 'x';
    if (pSig->Mask & DxilProgramSigMaskY)
      Mask[1] = 'y';
    if (pSig->Mask & DxilProgramSigMaskZ)
      Mask[2] = 'z';
    if (pSig->Mask & DxilProgramSigMaskW)
      Mask[3] = 'w';

    if (pSig->Register == -1) {
      OS << "    N/A";
      if (!_stricmp(pSemanticName, "SV_Depth"))
        OS << "   oDepth";
      else if (0 == _stricmp(pSemanticName, "SV_DepthGreaterEqual"))
        OS << " oDepthGE";
      else if (0 == _stricmp(pSemanticName, "SV_DepthLessEqual"))
        OS << " oDepthLE";
      else if (0 == _stricmp(pSemanticName, "SV_Coverage"))
        OS << "    oMask";
      else if (0 == _stricmp(pSemanticName, "SV_StencilRef"))
        OS << "    oStencilRef";
      else if (pSig->SystemValue == DxilProgramSigSemantic::PrimitiveID)
        OS << "   primID";
      else
        OS << "  special";
    } else {
      OS << "   " << Mask[0] << Mask[1] << Mask[2] << Mask[3];
      OS << ' ' << format("%8u", pSig->Register);
    }

    LPCSTR pSysValue = "NONE";
    switch (pSig->SystemValue) {
    case DxilProgramSigSemantic::ClipDistance:
      pSysValue = "CLIPDST";
      break;
    case DxilProgramSigSemantic::CullDistance:
      pSysValue = "CULLDST";
      break;
    case DxilProgramSigSemantic::Position:
      pSysValue = "POS";
      break;
    case DxilProgramSigSemantic::RenderTargetArrayIndex:
      pSysValue = "RTINDEX";
      break;
    case DxilProgramSigSemantic::ViewPortArrayIndex:
      pSysValue = "VPINDEX";
      break;
    case DxilProgramSigSemantic::VertexID:
      pSysValue = "VERTID";
      break;
    case DxilProgramSigSemantic::PrimitiveID:
      pSysValue = "PRIMID";
      break;
    case DxilProgramSigSemantic::InstanceID:
      pSysValue = "INSTID";
      break;
    case DxilProgramSigSemantic::IsFrontFace:
      pSysValue = "FFACE";
      break;
    case DxilProgramSigSemantic::SampleIndex:
      pSysValue = "SAMPLE";
      break;
    case DxilProgramSigSemantic::Target:
      pSysValue = "TARGET";
      break;
    case DxilProgramSigSemantic::Depth:
      pSysValue = "DEPTH";
      break;
    case DxilProgramSigSemantic::DepthGE:
      pSysValue = "DEPTHGE";
      break;
    case DxilProgramSigSemantic::DepthLE:
      pSysValue = "DEPTHLE";
      break;
    case DxilProgramSigSemantic::Coverage:
      pSysValue = "COVERAGE";
      break;
    case DxilProgramSigSemantic::InnerCoverage:
      pSysValue = "INNERCOV";
      break;
    case DxilProgramSigSemantic::StencilRef:
      pSysValue = "STENCILREF";
      break;
    case DxilProgramSigSemantic::FinalQuadEdgeTessfactor:
      pSysValue = "QUADEDGE";
      break;
    case DxilProgramSigSemantic::FinalQuadInsideTessfactor:
      pSysValue = "QUADINT";
      break;
    case DxilProgramSigSemantic::FinalTriEdgeTessfactor:
      pSysValue = "TRIEDGE";
      break;
    case DxilProgramSigSemantic::FinalTriInsideTessfactor:
      pSysValue = "TRIINT";
      break;
    case DxilProgramSigSemantic::FinalLineDetailTessfactor:
      pSysValue = "LINEDET";
      break;
    case DxilProgramSigSemantic::FinalLineDensityTessfactor:
      pSysValue = "LINEDEN";
      break;
    case DxilProgramSigSemantic::Barycentrics:
      pSysValue = "BARYCEN";
    }
    OS << right_justify(pSysValue, 9);

    LPCSTR pFormat = "unknown";
    switch (pSig->CompType) {
    case DxilProgramSigCompType::Float32:
      pFormat = "float";
      break;
    case DxilProgramSigCompType::SInt32:
      pFormat = "int";
      break;
    case DxilProgramSigCompType::UInt32:
      pFormat = "uint";
      break;
    case DxilProgramSigCompType::UInt16:
      pFormat = "min16u";
      break;
    case DxilProgramSigCompType::SInt16:
      pFormat = "min16i";
      break;
    case DxilProgramSigCompType::Float16:
      pFormat = "min16f";
      break;
    case DxilProgramSigCompType::UInt64:
      pFormat = "uint64";
      break;
    case DxilProgramSigCompType::SInt64:
      pFormat = "int64";
      break;
    case DxilProgramSigCompType::Float64:
      pFormat = "double";
      break;
    }

    OS << right_justify(pFormat, 8);

    memset(Mask, ' ', sizeof(Mask));

    BYTE rwMask = pSig->AlwaysReads_Mask;
    if (!bIsInput)
      rwMask = ~rwMask;

    if (rwMask & DxilProgramSigMaskX)
      Mask[0] = 'x';
    if (rwMask & DxilProgramSigMaskY)
      Mask[1] = 'y';
    if (rwMask & DxilProgramSigMaskZ)
      Mask[2] = 'z';
    if (rwMask & DxilProgramSigMaskW)
      Mask[3] = 'w';

    if (pSig->Register == -1)
      OS << (rwMask ? "    YES" : "     NO");
    else
      OS << "   " << Mask[0] << Mask[1] << Mask[2] << Mask[3];

    OS << "\n";
  }
  OS << comment << "\n";
}

void PintCompMaskNameCompact(raw_string_ostream &OS, unsigned CompMask) {
  char Mask[5];
  memset(Mask, '\0', sizeof(Mask));
  unsigned idx = 0;
  if (CompMask & DxilProgramSigMaskX)
    Mask[idx++] = 'x';
  if (CompMask & DxilProgramSigMaskY)
    Mask[idx++] = 'y';
  if (CompMask & DxilProgramSigMaskZ)
    Mask[idx++] = 'z';
  if (CompMask & DxilProgramSigMaskW)
    Mask[idx++] = 'w';
  OS << right_justify(Mask, 4);
}

void PrintDxilSignature(LPCSTR pName, const DxilSignature &Signature,
                               raw_string_ostream &OS, StringRef comment) {
  const std::vector<std::unique_ptr<DxilSignatureElement>> &sigElts =
      Signature.GetElements();
  if (sigElts.size() == 0)
    return;
  // TODO: Print all the data in DxilSignature.
  OS << comment << "\n"
     << comment << " " << pName << " signature:\n"
     << comment << "\n"
     << comment << " Name                 Index             InterpMode DynIdx\n"
     << comment
     << " -------------------- ----- ---------------------- ------\n";

  for (auto &sigElt : sigElts) {
    OS << comment << " ";

    OS << left_justify(sigElt->GetName(), 20);
    OS << ' ' << format("%5u", sigElt->GetSemanticIndexVec()[0]);
    sigElt->GetInterpolationMode()->GetName();
    OS << ' ' << right_justify(sigElt->GetInterpolationMode()->GetName(), 22);
    OS << "   ";
    PintCompMaskNameCompact(OS, sigElt->GetDynIdxCompMask());
    OS << "\n";
  }
}

PCSTR g_pFeatureInfoNames[] = {
    "Double-precision floating point",
    "Raw and Structured buffers",
    "UAVs at every shader stage",
    "64 UAV slots",
    "Minimum-precision data types",
    "Double-precision extensions for 11.1",
    "Shader extensions for 11.1",
    "Comparison filtering for feature level 9",
    "Tiled resources",
    "PS Output Stencil Ref",
    "PS Inner Coverage",
    "Typed UAV Load Additional Formats",
    "Raster Ordered UAVs",
    "SV_RenderTargetArrayIndex or SV_ViewportArrayIndex from any shader "
    "feeding rasterizer",
    "Wave level operations",
    "64-Bit integer",
};

void PrintFeatureInfo(const DxilShaderFeatureInfo *pFeatureInfo,
                             raw_string_ostream &OS, StringRef comment) {
  uint64_t featureFlags = pFeatureInfo->FeatureFlags;
  if (!featureFlags)
    return;
  OS << comment << "\n";
  OS << comment << " Note: shader requires additional functionality:\n";
  for (unsigned i = 0; i < ShaderFeatureInfoCount; i++) {
    if (featureFlags & (((uint64_t)1) << i))
      OS << comment << "       " << g_pFeatureInfoNames[i] << "\n";
  }
  OS << comment << "\n";
}

void PrintResourceFormat(DxilResourceBase &res, unsigned alignment,
                                raw_string_ostream &OS) {
  switch (res.GetClass()) {
  case DxilResourceBase::Class::CBuffer:
  case DxilResourceBase::Class::Sampler:
    OS << right_justify("NA", alignment);
    break;
  case DxilResourceBase::Class::UAV:
  case DxilResourceBase::Class::SRV:
    switch (res.GetKind()) {
    case DxilResource::Kind::RawBuffer:
      OS << right_justify("byte", alignment);
      break;
    case DxilResource::Kind::StructuredBuffer:
      OS << right_justify("struct", alignment);
      break;
    default:
      DxilResource *pRes = static_cast<DxilResource *>(&res);
      CompType &&compType = pRes->GetCompType();
      const char *compName = compType.GetName();
      // TODO: add vector size.
      OS << right_justify(compName, alignment);
      break;
    }
  }
}

void PrintResourceDim(DxilResourceBase &res, unsigned alignment,
                             raw_string_ostream &OS) {
  switch (res.GetClass()) {
  case DxilResourceBase::Class::CBuffer:
  case DxilResourceBase::Class::Sampler:
    OS << right_justify("NA", alignment);
    break;
  case DxilResourceBase::Class::UAV:
  case DxilResourceBase::Class::SRV:
    switch (res.GetKind()) {
    case DxilResource::Kind::RawBuffer:
    case DxilResource::Kind::StructuredBuffer:
      if (res.GetClass() == DxilResourceBase::Class::SRV)
        OS << right_justify("r/o", alignment);
      else {
        DxilResource &dxilRes = static_cast<DxilResource &>(res);
        if (!dxilRes.HasCounter())
          OS << right_justify("r/w", alignment);
        else
          OS << right_justify("r/w+cnt", alignment);
      }
      break;
    case DxilResource::Kind::TypedBuffer:
      OS << right_justify("buf", alignment);
      break;
    case DxilResource::Kind::Texture2DMS:
    case DxilResource::Kind::Texture2DMSArray: {
      DxilResource *pRes = static_cast<DxilResource *>(&res);
      std::string dimName = res.GetResDimName();
      if (pRes->GetSampleCount())
        dimName += pRes->GetSampleCount();
      OS << right_justify(dimName, alignment);
    } break;
    default:
      OS << right_justify(res.GetResDimName(), alignment);
      break;
    }
    break;
  }
}

void PrintResourceBinding(DxilResourceBase &res, raw_string_ostream &OS,
                                 StringRef comment) {
  OS << comment << " " << left_justify(res.GetGlobalName(), 31);

  OS << right_justify(res.GetResClassName(), 10);

  PrintResourceFormat(res, 8, OS);

  PrintResourceDim(res, 12, OS);

  std::string ID = res.GetResIDPrefix();
  ID += std::to_string(res.GetID());
  OS << right_justify(ID, 8);

  std::string bind = res.GetResBindPrefix();
  bind += std::to_string(res.GetLowerBound());
  if (res.GetSpaceID())
    bind += ",space" + std::to_string(res.GetSpaceID());

  OS << right_justify(bind, 15);
  if (res.GetRangeSize() != UINT_MAX)
    OS << right_justify(std::to_string(res.GetRangeSize()), 6) << "\n";
  else
    OS << right_justify("unbounded", 6) << "\n";
}

void PrintResourceBindings(DxilModule &M, raw_string_ostream &OS,
                                  StringRef comment) {
  OS << comment << "\n"
     << comment << " Resource Bindings:\n"
     << comment << "\n"
     << comment
     << " Name                                 Type  Format         Dim      "
        "ID      HLSL Bind  Count\n"
     << comment
     << " ------------------------------ ---------- ------- ----------- "
        "------- -------------- ------\n";

  for (auto &res : M.GetCBuffers()) {
    PrintResourceBinding(*res.get(), OS, comment);
  }
  for (auto &res : M.GetSamplers()) {
    PrintResourceBinding(*res.get(), OS, comment);
  }
  for (auto &res : M.GetSRVs()) {
    PrintResourceBinding(*res.get(), OS, comment);
  }
  for (auto &res : M.GetUAVs()) {
    PrintResourceBinding(*res.get(), OS, comment);
  }
  OS << comment << "\n";
}

void PrintOutputsDependentOnViewId(
    llvm::raw_ostream &OS, llvm::StringRef comment, llvm::StringRef SetName,
    unsigned NumOutputs,
    const DxilViewIdState::OutputsDependentOnViewIdType
        &OutputsDependentOnViewId) {
  OS << comment << " " << SetName << " dependent on ViewId: { ";
  bool bFirst = true;
  for (unsigned i = 0; i < NumOutputs; i++) {
    if (OutputsDependentOnViewId[i]) {
      if (!bFirst)
        OS << ", ";
      OS << i;
      bFirst = false;
    }
  }
  OS << " }\n";
}

void PrintInputsContributingToOutputs(
    llvm::raw_ostream &OS, llvm::StringRef comment,
    llvm::StringRef InputSetName, llvm::StringRef OutputSetName,
    const DxilViewIdState::InputsContributingToOutputType
        &InputsContributingToOutputs) {
  OS << comment << " " << InputSetName << " contributing to computation of "
     << OutputSetName << ":\n";
  for (auto &it : InputsContributingToOutputs) {
    unsigned outIdx = it.first;
    auto &Inputs = it.second;
    OS << comment << "   output " << outIdx << " depends on inputs: { ";
    bool bFirst = true;
    for (unsigned i : Inputs) {
      if (!bFirst)
        OS << ", ";
      OS << i;
      bFirst = false;
    }
    OS << " }\n";
  }
}

void PrintViewIdState(DxilModule &M, raw_string_ostream &OS,
                             StringRef comment) {
  if (!M.GetModule()->getNamedMetadata("dx.viewIdState"))
    return;

  const ShaderModel *pSM = M.GetShaderModel();
  DxilViewIdState &VID = M.GetViewIdState();
  OS << comment << "\n";
  OS << comment << " ViewId state:\n";
  OS << comment << "\n";
  OS << comment << " Number of inputs: " << VID.getNumInputSigScalars();
  if (!pSM->IsGS()) {
    OS << ", outputs: " << VID.getNumOutputSigScalars(0);
  } else {
    OS << ", outputs per stream: { " << VID.getNumOutputSigScalars(0) << ", "
       << VID.getNumOutputSigScalars(1) << ", " << VID.getNumOutputSigScalars(2)
       << ", " << VID.getNumOutputSigScalars(3) << " }";
  }
  if (pSM->IsHS() || pSM->IsDS()) {
    OS << ", patchconst: " << VID.getNumPCSigScalars();
  }
  OS << "\n";

  if (!pSM->IsGS()) {
    PrintOutputsDependentOnViewId(OS, comment, "Outputs",
                                  VID.getNumOutputSigScalars(0),
                                  VID.getOutputsDependentOnViewId(0));
  } else {
    for (unsigned i = 0; i < 4; i++) {
      if (VID.getNumOutputSigScalars(i) > 0) {
        std::string OutputsName =
            std::string("Outputs for Stream ") + std::to_string(i);
        PrintOutputsDependentOnViewId(OS, comment, OutputsName,
                                      VID.getNumOutputSigScalars(i),
                                      VID.getOutputsDependentOnViewId(i));
      }
    }
  }
  if (pSM->IsHS()) {
    PrintOutputsDependentOnViewId(OS, comment, "PCOutputs",
                                  VID.getNumPCSigScalars(),
                                  VID.getPCOutputsDependentOnViewId());
  }

  if (!pSM->IsGS()) {
    PrintInputsContributingToOutputs(OS, comment, "Inputs", "Outputs",
                                     VID.getInputsContributingToOutputs(0));
  } else {
    for (unsigned i = 0; i < 4; i++) {
      if (VID.getNumOutputSigScalars(i) > 0) {
        std::string OutputsName =
            std::string("Outputs for Stream ") + std::to_string(i);
        PrintInputsContributingToOutputs(OS, comment, "Inputs", OutputsName,
                                         VID.getInputsContributingToOutputs(i));
      }
    }
  }
  if (pSM->IsHS()) {
    PrintInputsContributingToOutputs(OS, comment, "Inputs", "PCOutputs",
                                     VID.getInputsContributingToPCOutputs());
  } else if (pSM->IsDS()) {
    PrintInputsContributingToOutputs(OS, comment, "PCInputs", "Outputs",
                                     VID.getPCInputsContributingToOutputs());
  }
  OS << comment << "\n";
}

void PrintStructLayout(StructType *ST, DxilTypeSystem &typeSys,
                              raw_string_ostream &OS, StringRef comment,
                              StringRef varName, unsigned offset,
                              unsigned indent, unsigned arraySize,
                              unsigned sizeOfStruct = 0);

void PrintTypeAndName(llvm::Type *Ty, DxilFieldAnnotation &annotation,
                             std::string &StreamStr, unsigned arraySize) {
  raw_string_ostream Stream(StreamStr);
  while (Ty->isArrayTy())
    Ty = Ty->getArrayElementType();

  const char *compTyName = annotation.GetCompType().GetHLSLName();
  if (annotation.HasMatrixAnnotation()) {
    const DxilMatrixAnnotation &Matrix = annotation.GetMatrixAnnotation();
    switch (Matrix.Orientation) {
    case MatrixOrientation::RowMajor:
      Stream << "row_major ";
      break;
    case MatrixOrientation::ColumnMajor:
      Stream << "column_major ";
      break;
    }
    Stream << compTyName << Matrix.Rows << "x" << Matrix.Cols;
  } else if (Ty->isVectorTy())
    Stream << compTyName << Ty->getVectorNumElements();
  else
    Stream << compTyName;

  Stream << " " << annotation.GetFieldName();
  if (arraySize)
    Stream << "[" << arraySize << "]";
  Stream << ";";
  Stream.flush();
}

void PrintFieldLayout(llvm::Type *Ty, DxilFieldAnnotation &annotation,
                             DxilTypeSystem &typeSys, raw_string_ostream &OS,
                             StringRef comment, unsigned offset,
                             unsigned indent, unsigned offsetIndent,
                             unsigned sizeToPrint = 0) {
  offset += annotation.GetCBufferOffset();
  if (Ty->isStructTy() && !annotation.HasMatrixAnnotation()) {
    PrintStructLayout(cast<StructType>(Ty), typeSys, OS, comment,
                      annotation.GetFieldName(), offset, indent, offsetIndent);
  } else {
    llvm::Type *EltTy = Ty;
    unsigned arraySize = 0;
    unsigned arrayLevel = 0;
    if (!HLMatrixLower::IsMatrixType(EltTy) && EltTy->isArrayTy()) {
      arraySize = 1;
      while (!HLMatrixLower::IsMatrixType(EltTy) && EltTy->isArrayTy()) {
        arraySize *= EltTy->getArrayNumElements();
        EltTy = EltTy->getArrayElementType();
        arrayLevel++;
      }
    }

    if (annotation.HasMatrixAnnotation()) {
      const DxilMatrixAnnotation &Matrix = annotation.GetMatrixAnnotation();
      switch (Matrix.Orientation) {
      case MatrixOrientation::RowMajor:
        arraySize /= Matrix.Rows;
        break;
      case MatrixOrientation::ColumnMajor:
        arraySize /= Matrix.Cols;
        break;
      }
      if (EltTy->isVectorTy()) {
        EltTy = EltTy->getVectorElementType();
      } else if (EltTy->isStructTy()) {
        unsigned col, row;
        EltTy = HLMatrixLower::GetMatrixInfo(EltTy, col, row);
      }
      if (arrayLevel == 1)
        arraySize = 0;
    }

    std::string StreamStr;
    if (!HLMatrixLower::IsMatrixType(EltTy) && EltTy->isStructTy()) {
      std::string NameTypeStr = annotation.GetFieldName();
      raw_string_ostream Stream(NameTypeStr);
      if (arraySize)
        Stream << "[" << std::to_string(arraySize) << "]";
      Stream << ";";
      Stream.flush();

      PrintStructLayout(cast<StructType>(EltTy), typeSys, OS, comment,
                        NameTypeStr, offset, indent, offsetIndent);
    } else {
      (OS << comment).indent(indent);
      std::string NameTypeStr;
      PrintTypeAndName(Ty, annotation, NameTypeStr, arraySize);
      OS << left_justify(NameTypeStr, offsetIndent);

      // Offset
      OS << comment << " Offset:" << right_justify(std::to_string(offset), 5);
      if (sizeToPrint)
        OS << " Size: " << right_justify(std::to_string(sizeToPrint), 5);
      OS << "\n";
    }
  }
}

void PrintStructLayout(StructType *ST, DxilTypeSystem &typeSys,
                              raw_string_ostream &OS, StringRef comment,
                              StringRef varName, unsigned offset,
                              unsigned indent, unsigned offsetIndent,
                              unsigned sizeOfStruct) {
  DxilStructAnnotation *annotation = typeSys.GetStructAnnotation(ST);
  (OS << comment).indent(indent) << "struct " << ST->getName() << "\n";
  (OS << comment).indent(indent) << "{\n";
  OS << comment << "\n";

  unsigned fieldIndent = indent + 4;

  for (unsigned i = 0; i < ST->getNumElements(); i++) {
    PrintFieldLayout(ST->getElementType(i), annotation->GetFieldAnnotation(i),
                     typeSys, OS, comment, offset, fieldIndent,
                     offsetIndent - 4);
  }
  (OS << comment).indent(indent) << "\n";
  // The 2 in offsetIndent-indent-2 is for "} ".
  (OS << comment).indent(indent)
      << "} " << left_justify(varName, offsetIndent - 2);
  OS << comment << " Offset:" << right_justify(std::to_string(offset), 5);
  if (sizeOfStruct)
    OS << " Size: " << right_justify(std::to_string(sizeOfStruct), 5);
  ;
  OS << "\n";

  OS << comment << "\n";
}

void PrintStructBufferDefinition(DxilResource *buf,
                                        DxilTypeSystem &typeSys,
                                        const DataLayout &DL,
                                        raw_string_ostream &OS,
                                        StringRef comment) {
  const unsigned offsetIndent = 50;

  OS << comment << " Resource bind info for " << buf->GetGlobalName() << "\n";
  OS << comment << " {\n";
  OS << comment << "\n";
  llvm::Type *RetTy = buf->GetRetType();
  // Skip none struct type.
  if (!RetTy->isStructTy() || HLMatrixLower::IsMatrixType(RetTy)) {
    Value *GV = buf->GetGlobalSymbol();
    llvm::Type *Ty = GV->getType()->getPointerElementType();
    // For resource array, use element type.
    if (Ty->isArrayTy())
      Ty = Ty->getArrayElementType();
    // Get the struct buffer type like this %class.StructuredBuffer = type {
    // %struct.mat }.
    StructType *ST = cast<StructType>(Ty);
    DxilStructAnnotation *annotation = typeSys.GetStructAnnotation(ST);
    if (nullptr == annotation) {
      OS << comment << "   [" << DL.getTypeAllocSize(ST)
         << " x i8] (type annotation not present)\n";
    } else {
      DxilFieldAnnotation &fieldAnnotation = annotation->GetFieldAnnotation(0);
      fieldAnnotation.SetFieldName("$Element");
      PrintFieldLayout(RetTy, fieldAnnotation, typeSys, OS, comment,
                       /*offset*/ 0, /*indent*/ 3, offsetIndent,
                       DL.getTypeAllocSize(ST));
    }
    OS << comment << "\n";
  } else {
    StructType *ST = cast<StructType>(RetTy);

    // TODO: struct buffer has different layout.
    // Cannot use cbuffer layout here.
    DxilStructAnnotation *annotation = typeSys.GetStructAnnotation(ST);
    if (nullptr == annotation) {
      OS << comment << "   [" << DL.getTypeAllocSize(ST)
         << " x i8] (type annotation not present)\n";
    } else {
      PrintStructLayout(ST, typeSys, OS, comment, "$Element;",
                        /*offset*/ 0, /*indent*/ 3, offsetIndent,
                        DL.getTypeAllocSize(ST));
    }
  }
  OS << comment << " }\n";
  OS << comment << "\n";
}

void PrintTBufferDefinition(DxilResource *buf, DxilTypeSystem &typeSys,
                                   raw_string_ostream &OS, StringRef comment) {
  const unsigned offsetIndent = 50;
  Value *GV = buf->GetGlobalSymbol();
  llvm::Type *Ty = GV->getType()->getPointerElementType();
  // For TextureBuffer<> buf[2], the array size is in Resource binding count
  // part.
  if (Ty->isArrayTy())
    Ty = Ty->getArrayElementType();

  DxilStructAnnotation *annotation =
      typeSys.GetStructAnnotation(cast<StructType>(Ty));
  OS << comment << " tbuffer " << buf->GetGlobalName() << "\n";
  OS << comment << " {\n";
  OS << comment << "\n";
  if (nullptr == annotation) {
    OS << comment << "   (type annotation not present)\n";
    OS << comment << "\n";
  } else {
    PrintStructLayout(cast<StructType>(Ty), typeSys, OS, comment,
                      buf->GetGlobalName(), /*offset*/ 0, /*indent*/ 3,
                      offsetIndent, annotation->GetCBufferSize());
  }
  OS << comment << " }\n";
  OS << comment << "\n";
}

void PrintCBufferDefinition(DxilCBuffer *buf, DxilTypeSystem &typeSys,
                                   raw_string_ostream &OS, StringRef comment) {
  const unsigned offsetIndent = 50;
  Value *GV = buf->GetGlobalSymbol();
  llvm::Type *Ty = GV->getType()->getPointerElementType();
  // For ConstantBuffer<> buf[2], the array size is in Resource binding count
  // part.
  if (Ty->isArrayTy())
    Ty = Ty->getArrayElementType();

  DxilStructAnnotation *annotation =
      typeSys.GetStructAnnotation(cast<StructType>(Ty));
  OS << comment << " cbuffer " << buf->GetGlobalName() << "\n";
  OS << comment << " {\n";
  OS << comment << "\n";
  if (nullptr == annotation) {
    OS << comment << "   [" << buf->GetSize()
       << " x i8] (type annotation not present)\n";
    OS << comment << "\n";
  } else {
    PrintStructLayout(cast<StructType>(Ty), typeSys, OS, comment,
                      buf->GetGlobalName(), /*offset*/ 0, /*indent*/ 3,
                      offsetIndent, buf->GetSize());
  }
  OS << comment << " }\n";
  OS << comment << "\n";
}

void PrintBufferDefinitions(DxilModule &M, raw_string_ostream &OS,
                                   StringRef comment) {
  OS << comment << "\n"
     << comment << " Buffer Definitions:\n"
     << comment << "\n";
  DxilTypeSystem &typeSys = M.GetTypeSystem();

  for (auto &CBuf : M.GetCBuffers())
    PrintCBufferDefinition(CBuf.get(), typeSys, OS, comment);
  const DataLayout &layout = M.GetModule()->getDataLayout();
  for (auto &res : M.GetSRVs()) {
    if (res->IsStructuredBuffer())
      PrintStructBufferDefinition(res.get(), typeSys, layout, OS, comment);
    else if (res->IsTBuffer())
      PrintTBufferDefinition(res.get(), typeSys, OS, comment);
  }
  for (auto &res : M.GetUAVs()) {
    if (res->IsStructuredBuffer())
      PrintStructBufferDefinition(res.get(), typeSys, layout, OS, comment);
  }
}

/* <py::lines('OPCODE-SIGS')>hctdb_instrhelp.get_opsigs()</py>*/
// OPCODE-SIGS:BEGIN
static const char *OpCodeSignatures[] = {
  "(index)",  // TempRegLoad
  "(index,value)",  // TempRegStore
  "(regIndex,index,component)",  // MinPrecXRegLoad
  "(regIndex,index,component,value)",  // MinPrecXRegStore
  "(inputSigId,rowIndex,colIndex,gsVertexAxis)",  // LoadInput
  "(outputtSigId,rowIndex,colIndex,value)",  // StoreOutput
  "(value)",  // FAbs
  "(value)",  // Saturate
  "(value)",  // IsNaN
  "(value)",  // IsInf
  "(value)",  // IsFinite
  "(value)",  // IsNormal
  "(value)",  // Cos
  "(value)",  // Sin
  "(value)",  // Tan
  "(value)",  // Acos
  "(value)",  // Asin
  "(value)",  // Atan
  "(value)",  // Hcos
  "(value)",  // Hsin
  "(value)",  // Htan
  "(value)",  // Exp
  "(value)",  // Frc
  "(value)",  // Log
  "(value)",  // Sqrt
  "(value)",  // Rsqrt
  "(value)",  // Round_ne
  "(value)",  // Round_ni
  "(value)",  // Round_pi
  "(value)",  // Round_z
  "(value)",  // Bfrev
  "(value)",  // Countbits
  "(value)",  // FirstbitLo
  "(value)",  // FirstbitHi
  "(value)",  // FirstbitSHi
  "(a,b)",  // FMax
  "(a,b)",  // FMin
  "(a,b)",  // IMax
  "(a,b)",  // IMin
  "(a,b)",  // UMax
  "(a,b)",  // UMin
  "(a,b)",  // IMul
  "(a,b)",  // UMul
  "(a,b)",  // UDiv
  "(a,b)",  // UAddc
  "(a,b)",  // USubb
  "(a,b,c)",  // FMad
  "(a,b,c)",  // Fma
  "(a,b,c)",  // IMad
  "(a,b,c)",  // UMad
  "(a,b,c)",  // Msad
  "(a,b,c)",  // Ibfe
  "(a,b,c)",  // Ubfe
  "(width,offset,value,replaceCount)",  // Bfi
  "(ax,ay,bx,by)",  // Dot2
  "(ax,ay,az,bx,by,bz)",  // Dot3
  "(ax,ay,az,aw,bx,by,bz,bw)",  // Dot4
  "(resourceClass,rangeId,index,nonUniformIndex)",  // CreateHandle
  "(handle,byteOffset,alignment)",  // CBufferLoad
  "(handle,regIndex)",  // CBufferLoadLegacy
  "(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,clamp)",  // Sample
  "(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,bias,clamp)",  // SampleBias
  "(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)",  // SampleLevel
  "(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp)",  // SampleGrad
  "(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,clamp)",  // SampleCmp
  "(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)",  // SampleCmpLevelZero
  "(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)",  // TextureLoad
  "(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)",  // TextureStore
  "(srv,index,wot)",  // BufferLoad
  "(uav,coord0,coord1,value0,value1,value2,value3,mask)",  // BufferStore
  "(uav,inc)",  // BufferUpdateCounter
  "(status)",  // CheckAccessFullyMapped
  "(handle,mipLevel)",  // GetDimensions
  "(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,channel)",  // TextureGather
  "(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,channel,compareVale)",  // TextureGatherCmp
  "(srv,index)",  // Texture2DMSGetSamplePosition
  "(index)",  // RenderTargetGetSamplePosition
  "()",  // RenderTargetGetSampleCount
  "(handle,atomicOp,offset0,offset1,offset2,newValue)",  // AtomicBinOp
  "(handle,offset0,offset1,offset2,compareValue,newValue)",  // AtomicCompareExchange
  "(barrierMode)",  // Barrier
  "(handle,sampler,coord0,coord1,coord2,clamped)",  // CalculateLOD
  "(condition)",  // Discard
  "(value)",  // DerivCoarseX
  "(value)",  // DerivCoarseY
  "(value)",  // DerivFineX
  "(value)",  // DerivFineY
  "(inputSigId,inputRowIndex,inputColIndex,offsetX,offsetY)",  // EvalSnapped
  "(inputSigId,inputRowIndex,inputColIndex,sampleIndex)",  // EvalSampleIndex
  "(inputSigId,inputRowIndex,inputColIndex)",  // EvalCentroid
  "()",  // SampleIndex
  "()",  // Coverage
  "()",  // InnerCoverage
  "(component)",  // ThreadId
  "(component)",  // GroupId
  "(component)",  // ThreadIdInGroup
  "()",  // FlattenedThreadIdInGroup
  "(streamId)",  // EmitStream
  "(streamId)",  // CutStream
  "(streamId)",  // EmitThenCutStream
  "()",  // GSInstanceID
  "(lo,hi)",  // MakeDouble
  "(value)",  // SplitDouble
  "(inputSigId,row,col,index)",  // LoadOutputControlPoint
  "(inputSigId,row,col)",  // LoadPatchConstant
  "(component)",  // DomainLocation
  "(outputSigID,row,col,value)",  // StorePatchConstant
  "()",  // OutputControlPointID
  "()",  // PrimitiveID
  "()",  // CycleCounterLegacy
  "()",  // WaveIsFirstLane
  "()",  // WaveGetLaneIndex
  "()",  // WaveGetLaneCount
  "(cond)",  // WaveAnyTrue
  "(cond)",  // WaveAllTrue
  "(value)",  // WaveActiveAllEqual
  "(cond)",  // WaveActiveBallot
  "(value,lane)",  // WaveReadLaneAt
  "(value)",  // WaveReadLaneFirst
  "(value,op,sop)",  // WaveActiveOp
  "(value,op)",  // WaveActiveBit
  "(value,op,sop)",  // WavePrefixOp
  "(value,quadLane)",  // QuadReadLaneAt
  "(value,op)",  // QuadOp
  "(value)",  // BitcastI16toF16
  "(value)",  // BitcastF16toI16
  "(value)",  // BitcastI32toF32
  "(value)",  // BitcastF32toI32
  "(value)",  // BitcastI64toF64
  "(value)",  // BitcastF64toI64
  "(value)",  // LegacyF32ToF16
  "(value)",  // LegacyF16ToF32
  "(value)",  // LegacyDoubleToFloat
  "(value)",  // LegacyDoubleToSInt32
  "(value)",  // LegacyDoubleToUInt32
  "(value)",  // WaveAllBitCount
  "(value)",  // WavePrefixBitCount
  "(inputSigId,inputRowIndex,inputColIndex,VertexID)",  // AttributeAtVertex
  "()"  // ViewID
};
// OPCODE-SIGS:END

class DxcAssemblyAnnotationWriter : public llvm::AssemblyAnnotationWriter {
public:
  ~DxcAssemblyAnnotationWriter() {}
  __override void printInfoComment(const Value &V, formatted_raw_ostream &OS) {
    const CallInst *CI = dyn_cast<const CallInst>(&V);
    if (!CI) {
      return;
    }
    // TODO: annotate high-level operations where possible as well
    if (CI->getNumArgOperands() == 0 ||
        !CI->getCalledFunction()->getName().startswith("dx.op.")) {
      return;
    }
    const ConstantInt *CInt = dyn_cast<const ConstantInt>(CI->getArgOperand(0));
    if (!CInt) {
      // At this point, we know this is malformed; ignore.
      return;
    }

    unsigned opcodeVal = CInt->getZExtValue();
    if (opcodeVal >= (unsigned)DXIL::OpCode::NumOpCodes) {
      OS << "  ; invalid DXIL opcode #" << opcodeVal;
      return;
    }

    // TODO: if an argument references a resource, look it up and write the
    // name/binding
    DXIL::OpCode opcode = (DXIL::OpCode)opcodeVal;
    OS << "  ; " << hlsl::OP::GetOpCodeName(opcode)
       << OpCodeSignatures[opcodeVal];
  }
};

void PrintPipelineStateValidationRuntimeInfo(const char *pBuffer,
                                                    DXIL::ShaderKind shaderKind,
                                                    raw_string_ostream &OS,
                                                    StringRef comment) {
  OS << comment << "\n"
     << comment << " Pipeline Runtime Information: \n"
     << comment << "\n";

  const unsigned offset = sizeof(unsigned);
  const PSVRuntimeInfo0 *pInfo = (PSVRuntimeInfo0 *)(pBuffer + offset);

  switch (shaderKind) {
  case DXIL::ShaderKind::Hull: {
    OS << comment << " Hull Shader\n";
    OS << comment
       << " InputControlPointCount=" << pInfo->HS.InputControlPointCount
       << "\n";
    OS << comment
       << " OutputControlPointCount=" << pInfo->HS.OutputControlPointCount
       << "\n";
    OS << comment << " Domain=";
    DXIL::TessellatorDomain domain =
        static_cast<DXIL::TessellatorDomain>(pInfo->HS.TessellatorDomain);
    switch (domain) {
    case DXIL::TessellatorDomain::IsoLine:
      OS << "isoline\n";
      break;
    case DXIL::TessellatorDomain::Tri:
      OS << "tri\n";
      break;
    case DXIL::TessellatorDomain::Quad:
      OS << "quad\n";
      break;
    default:
      OS << "invalid\n";
      break;
    }
    OS << comment << " OutputPrimitive=";
    DXIL::TessellatorOutputPrimitive primitive =
        static_cast<DXIL::TessellatorOutputPrimitive>(
            pInfo->HS.TessellatorOutputPrimitive);
    switch (primitive) {
    case DXIL::TessellatorOutputPrimitive::Point:
      OS << "point\n";
      break;
    case DXIL::TessellatorOutputPrimitive::Line:
      OS << "line\n";
      break;
    case DXIL::TessellatorOutputPrimitive::TriangleCW:
      OS << "triangle_cw\n";
      break;
    case DXIL::TessellatorOutputPrimitive::TriangleCCW:
      OS << "triangle_ccw\n";
      break;
    default:
      OS << "invalid\n";
      break;
    }
  } break;
  case DXIL::ShaderKind::Domain:
    OS << comment << " Domain Shader\n";
    OS << comment
       << " InputControlPointCount=" << pInfo->DS.InputControlPointCount
       << "\n";
    OS << comment
       << " OutputPositionPresent=" << (bool)pInfo->DS.OutputPositionPresent
       << "\n";
    break;
  case DXIL::ShaderKind::Geometry: {
    OS << comment << " Geometry Shader\n";
    OS << comment << " InputPrimitive=";
    DXIL::InputPrimitive primitive =
        static_cast<DXIL::InputPrimitive>(pInfo->GS.InputPrimitive);
    switch (primitive) {
    case DXIL::InputPrimitive::Point:
      OS << "point\n";
      break;
    case DXIL::InputPrimitive::Line:
      OS << "line\n";
      break;
    case DXIL::InputPrimitive::LineWithAdjacency:
      OS << "lineadj\n";
      break;
    case DXIL::InputPrimitive::Triangle:
      OS << "triangle\n";
      break;
    case DXIL::InputPrimitive::TriangleWithAdjacency:
      OS << "triangleadj\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch1:
      OS << "patch1\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch2:
      OS << "patch2\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch3:
      OS << "patch3\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch4:
      OS << "patch4\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch5:
      OS << "patch5\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch6:
      OS << "patch6\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch7:
      OS << "patch7\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch8:
      OS << "patch8\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch9:
      OS << "patch9\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch10:
      OS << "patch10\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch11:
      OS << "patch11\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch12:
      OS << "patch12\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch13:
      OS << "patch13\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch14:
      OS << "patch14\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch15:
      OS << "patch15\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch16:
      OS << "patch16\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch17:
      OS << "patch17\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch18:
      OS << "patch18\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch19:
      OS << "patch19\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch20:
      OS << "patch20\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch21:
      OS << "patch21\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch22:
      OS << "patch22\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch23:
      OS << "patch23\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch24:
      OS << "patch24\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch25:
      OS << "patch25\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch26:
      OS << "patch26\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch27:
      OS << "patch27\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch28:
      OS << "patch28\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch29:
      OS << "patch29\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch30:
      OS << "patch30\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch31:
      OS << "patch31\n";
      break;
    case DXIL::InputPrimitive::ControlPointPatch32:
      OS << "patch32\n";
      break;
    default:
      OS << "invalid\n";
      break;
    }
    OS << comment << " OutputTopology=";
    DXIL::PrimitiveTopology topology =
        static_cast<DXIL::PrimitiveTopology>(pInfo->GS.OutputTopology);
    switch (topology) {
    case DXIL::PrimitiveTopology::PointList:
      OS << "point\n";
      break;
    case DXIL::PrimitiveTopology::LineStrip:
      OS << "line\n";
      break;
    case DXIL::PrimitiveTopology::TriangleStrip:
      OS << "triangle\n";
      break;
    default:
      OS << "invalid\n";
      break;
    }
    OS << comment << " OutputStreamMask=" << pInfo->GS.OutputStreamMask << "\n";
    OS << comment
       << " OutputPositionPresent=" << (bool)pInfo->GS.OutputPositionPresent
       << "\n";
  } break;
  case DXIL::ShaderKind::Vertex:
    OS << comment << " Vertex Shader\n";
    OS << comment
       << " OutputPositionPresent=" << (bool)pInfo->VS.OutputPositionPresent
       << "\n";
    break;
  case DXIL::ShaderKind::Pixel:
    OS << comment << " Pixel Shader\n";
    OS << comment << " DepthOutput=" << (bool)pInfo->PS.DepthOutput << "\n";
    OS << comment << " SampleFrequency=" << (bool)pInfo->PS.SampleFrequency
       << "\n";
    break;
  }

  OS << comment << "\n";
}
}

namespace dxcutil {
void GetValidatorVersion(unsigned *pMajor, unsigned *pMinor) {
  if (pMajor == nullptr || pMinor == nullptr)
    return;

  CComPtr<IDxcValidator> pValidator;
  CreateValidator(pValidator);

  CComPtr<IDxcVersionInfo> pVersionInfo;
  if (SUCCEEDED(pValidator.QueryInterface(&pVersionInfo))) {
    IFT(pVersionInfo->GetVersion(pMajor, pMinor));
  } else {
    // Default to 1.0
    *pMajor = 1;
    *pMinor = 0;
  }
}

void AssembleToContainer(std::unique_ptr<llvm::Module> pM,
                         CComPtr<IDxcBlob> &pOutputBlob,
                         CComPtr<IMalloc> &pMalloc,
                         SerializeDxilFlags SerializeFlags,
                         CComPtr<AbstractMemoryStream> &pOutputStream) {
  // Take ownership of the module from the action.
  DxilCompilerLLVMModuleOutput llvmModule(std::move(pM));

  llvmModule.WrapModuleInDxilContainer(pMalloc, pOutputStream, pOutputBlob,
                                       SerializeFlags);
}

HRESULT ValidateAndAssembleToContainer(
    std::unique_ptr<llvm::Module> pM, CComPtr<IDxcBlob> &pOutputBlob,
    CComPtr<IMalloc> &pMalloc, SerializeDxilFlags SerializeFlags,
    CComPtr<AbstractMemoryStream> &pOutputStream, bool bDebugInfo,
    clang::DiagnosticsEngine &Diag) {
  HRESULT valHR = S_OK;

  // Take ownership of the module from the action.
  DxilCompilerLLVMModuleOutput llvmModule(std::move(pM));

  CComPtr<IDxcValidator> pValidator;
  bool bInternalValidator = CreateValidator(pValidator);
  // If using the internal validator, we'll use the modules directly.
  // In this case, we'll want to make a clone to avoid
  // SerializeDxilContainerForModule stripping all the debug info. The debug
  // info will be stripped from the orginal module, but preserved in the cloned
  // module.
  if (bInternalValidator && bDebugInfo)
    llvmModule.CloneForDebugInfo();

  llvmModule.WrapModuleInDxilContainer(pMalloc, pOutputStream, pOutputBlob,
                                       SerializeFlags);

  CComPtr<IDxcOperationResult> pValResult;
  // Important: in-place edit is required so the blob is reused and thus
  // dxil.dll can be released.
  if (bInternalValidator) {
    IFT(RunInternalValidator(pValidator, llvmModule.get(),
                             llvmModule.getWithDebugInfo(), pOutputBlob,
                             DxcValidatorFlags_InPlaceEdit, &pValResult));
  } else {
    IFT(pValidator->Validate(pOutputBlob, DxcValidatorFlags_InPlaceEdit,
                             &pValResult));
  }
  IFT(pValResult->GetStatus(&valHR));
  if (FAILED(valHR)) {
    CComPtr<IDxcBlobEncoding> pErrors;
    CComPtr<IDxcBlobEncoding> pErrorsUtf8;
    IFT(pValResult->GetErrorBuffer(&pErrors));
    IFT(hlsl::DxcGetBlobAsUtf8(pErrors, &pErrorsUtf8));
    StringRef errRef((const char *)pErrorsUtf8->GetBufferPointer(),
                     pErrorsUtf8->GetBufferSize());
    unsigned DiagID = Diag.getCustomDiagID(clang::DiagnosticsEngine::Error,
                                           "validation errors\r\n%0");
    Diag.Report(DiagID) << errRef;
  }
  CComPtr<IDxcBlob> pValidatedBlob;
  IFT(pValResult->GetResult(&pValidatedBlob));
  if (pValidatedBlob != nullptr) {
    std::swap(pOutputBlob, pValidatedBlob);
  }
  pValidator.Release();

  return valHR;
}

HRESULT Disassemble(IDxcBlob *pProgram, raw_string_ostream &Stream) {
  const char *pIL = (const char *)pProgram->GetBufferPointer();
  uint32_t pILLength = pProgram->GetBufferSize();
  if (const DxilContainerHeader *pContainer =
          IsDxilContainerLike(pIL, pILLength)) {
    if (!IsValidDxilContainer(pContainer, pILLength)) {
      return DXC_E_CONTAINER_INVALID;
    }

    DxilPartIterator it = std::find_if(begin(pContainer), end(pContainer),
                                       DxilPartIsType(DFCC_FeatureInfo));
    if (it != end(pContainer)) {
      PrintFeatureInfo(
          reinterpret_cast<const DxilShaderFeatureInfo *>(GetDxilPartData(*it)),
          Stream, /*comment*/ ";");
    }

    it = std::find_if(begin(pContainer), end(pContainer),
                      DxilPartIsType(DFCC_InputSignature));
    if (it != end(pContainer)) {
      PrintSignature(
          "Input",
          reinterpret_cast<const DxilProgramSignature *>(GetDxilPartData(*it)),
          true, Stream, /*comment*/ ";");
    }
    it = std::find_if(begin(pContainer), end(pContainer),
                      DxilPartIsType(DFCC_OutputSignature));
    if (it != end(pContainer)) {
      PrintSignature(
          "Output",
          reinterpret_cast<const DxilProgramSignature *>(GetDxilPartData(*it)),
          false, Stream, /*comment*/ ";");
    }
    it = std::find_if(begin(pContainer), end(pContainer),
                      DxilPartIsType(DFCC_PatchConstantSignature));
    if (it != end(pContainer)) {
      PrintSignature(
          "Patch Constant signature",
          reinterpret_cast<const DxilProgramSignature *>(GetDxilPartData(*it)),
          false, Stream, /*comment*/ ";");
    }

    it = std::find_if(begin(pContainer), end(pContainer),
                      DxilPartIsType(DFCC_ShaderDebugName));
    if (it != end(pContainer)) {
      const char *pDebugName;
      if (!GetDxilShaderDebugName(*it, &pDebugName, nullptr)) {
        Stream << "; shader debug name present; corruption detected\n";
      } else if (pDebugName && *pDebugName) {
        Stream << "; shader debug name: " << pDebugName << "\n";
      }
    }

    it = std::find_if(begin(pContainer), end(pContainer),
                      DxilPartIsType(DFCC_DXIL));
    if (it == end(pContainer)) {
      return DXC_E_CONTAINER_MISSING_DXIL;
    }

    DxilPartIterator dbgit =
        std::find_if(begin(pContainer), end(pContainer),
                     DxilPartIsType(DFCC_ShaderDebugInfoDXIL));
    // Use dbg module if exist.
    if (dbgit != end(pContainer))
      it = dbgit;

    const DxilProgramHeader *pProgramHeader =
        reinterpret_cast<const DxilProgramHeader *>(GetDxilPartData(*it));
    if (!IsValidDxilProgramHeader(pProgramHeader, (*it)->PartSize)) {
      return DXC_E_CONTAINER_INVALID;
    }

    it = std::find_if(begin(pContainer), end(pContainer),
                      DxilPartIsType(DFCC_PipelineStateValidation));
    if (it != end(pContainer)) {
      PrintPipelineStateValidationRuntimeInfo(
          GetDxilPartData(*it),
          GetVersionShaderType(pProgramHeader->ProgramVersion), Stream,
          /*comment*/ ";");
    }
    GetDxilProgramBitcode(pProgramHeader, &pIL, &pILLength);
  } else {
    const DxilProgramHeader *pProgramHeader =
        reinterpret_cast<const DxilProgramHeader *>(pIL);
    if (IsValidDxilProgramHeader(pProgramHeader, pILLength)) {
      GetDxilProgramBitcode(pProgramHeader, &pIL, &pILLength);
    }
  }

  std::string DiagStr;
  raw_string_ostream DiagStream(DiagStr);
  llvm::LLVMContext llvmContext;
  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  llvmContext.setDiagnosticHandler(PrintDiagnosticHandler, &DiagPrinter, true);
  std::unique_ptr<llvm::MemoryBuffer> pBitcodeBuf(
      llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(pIL, pILLength), "",
                                       false));
  ErrorOr<std::unique_ptr<llvm::Module>> pModule(
      llvm::parseBitcodeFile(pBitcodeBuf->getMemBufferRef(), llvmContext));
  if (std::error_code ec = pModule.getError()) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  if (pModule->get()->getNamedMetadata("dx.version")) {
    DxilModule &dxilModule = pModule->get()->GetOrCreateDxilModule();
    PrintDxilSignature("Input", dxilModule.GetInputSignature(), Stream,
                       /*comment*/ ";");
    PrintDxilSignature("Output", dxilModule.GetOutputSignature(), Stream,
                       /*comment*/ ";");
    PrintDxilSignature("Patch Constant signature",
                       dxilModule.GetPatchConstantSignature(), Stream,
                       /*comment*/ ";");
    PrintBufferDefinitions(dxilModule, Stream, /*comment*/ ";");
    PrintResourceBindings(dxilModule, Stream, /*comment*/ ";");
    PrintViewIdState(dxilModule, Stream, /*comment*/ ";");
  }
  DxcAssemblyAnnotationWriter w;
  pModule.get()->print(Stream, &w);
  Stream.flush();
  return S_OK;
}

} // namespace dxcutil