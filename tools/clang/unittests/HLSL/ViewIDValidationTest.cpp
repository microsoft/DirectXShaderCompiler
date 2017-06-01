///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ViewIDValidationTest.cpp                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Test ViewID pipeline validation: ViewIDPipelineValidation.inl             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <vector>
#include <string>
#include <cassert>
#include <sstream>
#include <algorithm>
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"

#include "WexTestClass.h"
#include "HlslTestUtils.h"
#include "DxcTestUtils.h"

#include "llvm/Support/raw_os_ostream.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/Unicode.h"

#include "dxc/HLSL/DxilConstants.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/HLSL/DxilSemantic.h"
#include "dxc/HLSL/DxilSigPoint.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilPipelineStateValidation.h"
#include "dxc/HLSL/DxilSignatureAllocator.h"

#include <fstream>

using namespace std;
using namespace hlsl_test;
using namespace hlsl;

namespace {

typedef std::vector<PSVSignatureElement0> ElementVec;
typedef std::vector<DxilSignatureAllocator::DummyElement> DummyElementVec;

static unsigned MaskIndex(const PSVSignatureElement0 &E, unsigned row, unsigned col) {
  VERIFY_IS_LESS_THAN(row, (unsigned)E.Rows);
  VERIFY_IS_LESS_THAN(col, (unsigned)(E.ColsAndStart & 0xF));
  return (E.StartRow + row) * 4 + ((E.ColsAndStart >> 4) & 0x3) + col;
}

static void InitElement(DxilSignatureAllocator::DummyElement &eOut,
  const PSVSignatureElement &eIn,
  DXIL::SigPointKind sigPoint) {
  eOut.rows = eIn.GetRows();
  eOut.cols = eIn.GetCols();
  eOut.row = eIn.GetStartRow();
  eOut.col = eIn.GetStartCol();
  eOut.kind = (DXIL::SemanticKind)eIn.GetSemanticKind();
  eOut.interpolation = (DXIL::InterpolationMode)eIn.GetInterpolationMode();
  eOut.interpretation = SigPoint::GetInterpretation(eOut.kind, sigPoint, 6, 1);
  eOut.indexFlags = eIn.GetDynamicIndexMask();

  // Tessfactors must be treated as dynamically indexed to prevent breaking them up
  if (eOut.interpretation == DXIL::SemanticInterpretationKind::TessFactor)
    eOut.indexFlags = (1 << eOut.cols) - 1;
}

struct MockPSV {
  PSVShaderKind shaderStage;
  ElementVec sigInputs;
  ElementVec sigOutputs;
  ElementVec sigPC;
  std::vector<char> stringBuffer;
  std::vector<uint32_t> semIndexBuffer;
  std::vector<char> psvBuffer;
  DxilPipelineStateValidation PSV;
  unsigned arbIndex;

  MockPSV(PSVShaderKind stage)
    : shaderStage(stage), arbIndex(0) {
    stringBuffer.push_back(0);
  }
  MockPSV(const MockPSV &other)
    : shaderStage(other.shaderStage),
      sigInputs(other.sigInputs),
      sigOutputs(other.sigOutputs),
      sigPC(other.sigPC),
      stringBuffer(other.stringBuffer),
      semIndexBuffer(other.semIndexBuffer),
      psvBuffer(other.psvBuffer),
      PSV(),
      arbIndex(other.arbIndex) {
    if (!psvBuffer.empty()) {
      PSV.InitFromPSV0(psvBuffer.data(), psvBuffer.size());
    }
  }

  void SetOutputViewIDDependency(unsigned elementId, unsigned row, unsigned col) {
    VERIFY_IS_LESS_THAN(elementId, sigOutputs.size());
    const PSVSignatureElement0 &E = sigOutputs[elementId];
    PSV.GetViewIDOutputMask(E.DynamicMaskAndStream >> 4).Set(MaskIndex(E, row, col));
  }
  void SetPCViewIDDependency(unsigned elementId, unsigned row, unsigned col) {
    VERIFY_IS_LESS_THAN(elementId, sigPC.size());
    const PSVSignatureElement0 &E = sigPC[elementId];
    PSV.GetViewIDPCOutputMask().Set(MaskIndex(E, row, col));
  }
  void SetInputOutputDependency(unsigned inId, unsigned inRow, unsigned inCol,
                                unsigned outId, unsigned outRow, unsigned outCol) {
    VERIFY_IS_LESS_THAN(inId, sigInputs.size());
    VERIFY_IS_LESS_THAN(outId, sigOutputs.size());
    const PSVSignatureElement0 &inE = sigInputs[inId];
    const PSVSignatureElement0 &outE = sigOutputs[outId];
    PSV.GetInputToOutputTable(outE.DynamicMaskAndStream >> 4).GetMaskForInput(MaskIndex(inE, inRow, inCol)).Set(MaskIndex(outE, outRow, outCol));
  }
  void SetInputPCOutputDependency(unsigned inId, unsigned inRow, unsigned inCol,
                                  unsigned outId, unsigned outRow, unsigned outCol) {
    VERIFY_IS_LESS_THAN(inId, sigInputs.size());
    VERIFY_IS_LESS_THAN(outId, sigPC.size());
    const PSVSignatureElement0 &inE = sigInputs[inId];
    const PSVSignatureElement0 &outE = sigPC[outId];
    PSV.GetInputToPCOutputTable().GetMaskForInput(MaskIndex(inE, inRow, inCol)).Set(MaskIndex(outE, outRow, outCol));
  }
  void SetPCInputOutputDependency(unsigned inId, unsigned inRow, unsigned inCol,
                                  unsigned outId, unsigned outRow, unsigned outCol) {
    VERIFY_IS_LESS_THAN(inId, sigPC.size());
    VERIFY_IS_LESS_THAN(outId, sigOutputs.size());
    const PSVSignatureElement0 &inE = sigInputs[inId];
    const PSVSignatureElement0 &outE = sigPC[outId];
    PSV.GetPCInputToOutputTable().GetMaskForInput(MaskIndex(inE, inRow, inCol)).Set(MaskIndex(outE, outRow, outCol));
  }

  unsigned AddSigElement(DXIL::SignatureKind sigKind,
                         const char *name,
                         llvm::ArrayRef<uint32_t> indexes,
                         unsigned rows,
                         unsigned cols,
                         int startRow,
                         int startCol,
                         PSVSemanticKind semKind,
                         hlsl::DxilProgramSigCompType compType = hlsl::DxilProgramSigCompType::Unknown,
                         DXIL::InterpolationMode interpMode = DXIL::InterpolationMode::Undefined,
                         unsigned streamIndex = 0,
                         int dynMask = -1) {
    ElementVec *pElements = nullptr;
    switch (sigKind)
    {
    case hlsl::DXIL::SignatureKind::Input:
      pElements = &sigInputs;
      break;
    case hlsl::DXIL::SignatureKind::Output:
      pElements = &sigOutputs;
      break;
    case hlsl::DXIL::SignatureKind::PatchConstant:
      pElements = &sigPC;
      break;
    case hlsl::DXIL::SignatureKind::Invalid:
    default:
      VERIFY_IS_TRUE(false && "Invalid SignatureKind");
    }
    PSVSignatureElement0 E;
    memset(&E, 0, sizeof(E));
    unsigned nameSize = name ? strlen(name) : 0;
    if (semKind == PSVSemanticKind::Arbitrary && nameSize > 0) {
      E.SemanticName = (uint32_t)stringBuffer.size();
      stringBuffer.insert(stringBuffer.end(), nameSize + 1, 0);
      memcpy(stringBuffer.data() + E.SemanticName, name, nameSize);
    }
    E.SemanticIndexes = semIndexBuffer.size();
    for (auto &idx : indexes) {
      semIndexBuffer.push_back(idx);
    }
    if (rows > 1 && dynMask == -1)
      dynMask = (indexes[E.SemanticIndexes] * 37) % 16;
    E.Rows = (uint8_t)rows;
    E.ColsAndStart = (uint8_t)cols & 0xF;
    if (startRow >= 0) {
      E.ColsAndStart |= 0x40 | ((startCol & 0x3) << 4);
      E.StartRow = (uint8_t)startRow;
    }
    E.SemanticKind = (uint8_t)semKind;
    E.ComponentType = (uint8_t)compType;
    E.InterpolationMode = (uint8_t)interpMode;
    E.DynamicMaskAndStream = (uint8_t)((streamIndex & 0x3) << 4);
    E.DynamicMaskAndStream |= dynMask & 0xF;
    unsigned id = pElements->size();
    pElements->push_back(E);
    return id;
  }

  unsigned AddSimple( DXIL::SignatureKind sigKind,
                      PSVSemanticKind semKind,
                      unsigned rows = 1,
                      unsigned cols = 0,
                      hlsl::DxilProgramSigCompType compType = hlsl::DxilProgramSigCompType::Unknown,
                      DXIL::InterpolationMode interpMode = DXIL::InterpolationMode::Undefined,
                      unsigned streamIndex = 0) {
    LPCSTR szSemantic = "";
    uint32_t index = 0;
    switch (semKind)
    {
    case PSVSemanticKind::Arbitrary:
      szSemantic = "Arb";
      index = arbIndex++;
      if (compType == hlsl::DxilProgramSigCompType::Unknown)
        compType = hlsl::DxilProgramSigCompType::Float32;
      if (interpMode == DXIL::InterpolationMode::Undefined)
        interpMode = DXIL::InterpolationMode::Linear;
      break;
    case PSVSemanticKind::VertexID:
    case PSVSemanticKind::InstanceID:
    case PSVSemanticKind::RenderTargetArrayIndex:
    case PSVSemanticKind::ViewPortArrayIndex:
    case PSVSemanticKind::PrimitiveID:
    case PSVSemanticKind::SampleIndex:
    case PSVSemanticKind::IsFrontFace:
    case PSVSemanticKind::Coverage:
    case PSVSemanticKind::InnerCoverage:
    case PSVSemanticKind::StencilRef:
      if (!cols)
        cols = 1;
      if (compType == hlsl::DxilProgramSigCompType::Unknown)
        compType = hlsl::DxilProgramSigCompType::UInt32;
      if (interpMode == DXIL::InterpolationMode::Undefined)
        interpMode = DXIL::InterpolationMode::Constant;
      break;
    case PSVSemanticKind::Position:
      if (!cols)
        cols = 4;
      if (compType == hlsl::DxilProgramSigCompType::Unknown)
        compType = hlsl::DxilProgramSigCompType::Float32;
      if (interpMode == DXIL::InterpolationMode::Undefined)
        interpMode = DXIL::InterpolationMode::LinearNoperspective;
      break;
    case PSVSemanticKind::TessFactor:
    case PSVSemanticKind::InsideTessFactor:
    case PSVSemanticKind::Depth:
    case PSVSemanticKind::DepthLessEqual:
    case PSVSemanticKind::DepthGreaterEqual:
      if (!cols)
        cols = 1;
      if (compType == hlsl::DxilProgramSigCompType::Unknown)
        compType = hlsl::DxilProgramSigCompType::Float32;
      break;
    case PSVSemanticKind::Barycentrics:
      if (!cols)
        cols = 3;
      if (compType == hlsl::DxilProgramSigCompType::Unknown)
        compType = hlsl::DxilProgramSigCompType::Float32;
      if (interpMode == DXIL::InterpolationMode::Undefined)
        interpMode = DXIL::InterpolationMode::Linear;
      break;
    case PSVSemanticKind::ClipDistance:
    case PSVSemanticKind::CullDistance:
      if (!cols)
        cols = 1;
      if (compType == hlsl::DxilProgramSigCompType::Unknown)
        compType = hlsl::DxilProgramSigCompType::Float32;
      if (interpMode == DXIL::InterpolationMode::Undefined)
        interpMode = DXIL::InterpolationMode::Linear;
      break;
    case PSVSemanticKind::Target:
      if (!cols)
        cols = 4;
      if (compType == hlsl::DxilProgramSigCompType::Unknown)
        compType = hlsl::DxilProgramSigCompType::Float32;
      break;
    case PSVSemanticKind::Invalid:
    case PSVSemanticKind::OutputControlPointID:
    case PSVSemanticKind::DomainLocation:
    case PSVSemanticKind::GSInstanceID:
    case PSVSemanticKind::DispatchThreadID:
    case PSVSemanticKind::GroupID:
    case PSVSemanticKind::GroupIndex:
    case PSVSemanticKind::GroupThreadID:
    case PSVSemanticKind::ViewID:
    default:
      VERIFY_IS_TRUE(false && "Invalid PSVSemanticKind for signature");
      break;
    }
    if (!cols)
      cols = (index % 4) + 1;
    return AddSigElement(sigKind, szSemantic, {index}, rows, cols, -1, -1, semKind, compType, interpMode, streamIndex, 0);
  }

  unsigned AddArb(DXIL::SignatureKind sigKind,
                  unsigned rows = 0,
                  unsigned cols = 0,
                  hlsl::DxilProgramSigCompType compType = hlsl::DxilProgramSigCompType::Float32,
                  DXIL::InterpolationMode interpMode = DXIL::InterpolationMode::Undefined,
                  int dynMask = -1,
                  unsigned streamIndex = 0) {
    unsigned index = arbIndex;
    if (!rows)
      rows = ((index * 23) % 16) + 1;
    arbIndex += rows;
    if (!cols)
      cols = (index % 4) + 1;
    if (interpMode == DXIL::InterpolationMode::Undefined) {
      switch (compType)
      {
      case hlsl::DxilProgramSigCompType::UInt32:
      case hlsl::DxilProgramSigCompType::SInt32:
      case hlsl::DxilProgramSigCompType::UInt16:
      case hlsl::DxilProgramSigCompType::SInt16:
        interpMode = DXIL::InterpolationMode::Constant;
        break;
      case hlsl::DxilProgramSigCompType::Float32:
      case hlsl::DxilProgramSigCompType::Float16:
        interpMode = DXIL::InterpolationMode::Linear;
        break;
      case hlsl::DxilProgramSigCompType::UInt64:
      case hlsl::DxilProgramSigCompType::SInt64:
      case hlsl::DxilProgramSigCompType::Float64:
      case hlsl::DxilProgramSigCompType::Unknown:
      default:
        VERIFY_IS_TRUE(false && "Invalid DxilProgramSigCompType for signature");
        break;
      }
    }
    std::vector<uint32_t> idxVec;
    idxVec.reserve(rows);
    for (uint32_t idx = index; idx < arbIndex; idx++)
      idxVec.push_back(idx);
    return AddSigElement(sigKind, "Arb", idxVec, rows, cols, -1, -1, PSVSemanticKind::Arbitrary, compType, interpMode, streamIndex, dynMask);
  }

  bool PackElements(ElementVec &elements, DXIL::SigPointKind sigPoint, unsigned streamIndex = 0) {
    DummyElementVec dummyElements;
    std::vector<DxilSignatureAllocator::PackElement*> packElements;
    dummyElements.reserve(elements.size());
    packElements.reserve(elements.size());
    PSVStringTable StringTable(stringBuffer.data(), stringBuffer.size());
    PSVSemanticIndexTable SemanticIndexTable(semIndexBuffer.data(), semIndexBuffer.size());
    for (auto &E : elements) {
      if (((E.DynamicMaskAndStream >> 4) & 0x3) != streamIndex)
        continue;
      DxilSignatureAllocator::DummyElement DE;
      InitElement(DE, PSVSignatureElement(StringTable, SemanticIndexTable, &E), sigPoint);
      dummyElements.push_back(DE);
      packElements.push_back(&dummyElements.back());
    }
    DxilSignatureAllocator alloc(32);
    alloc.PackPrefixStable(packElements, 0, 32);
    unsigned index = 0;
    for (auto &DE : dummyElements) {
      if (!DE.IsAllocated())
        return false;
      PSVSignatureElement0 &E = elements[index++];
      E.StartRow = DE.GetStartRow();
      E.ColsAndStart = 0x40 | (DE.GetCols() & 0xF) | ((DE.GetStartCol() & 0x3) << 4);
    }
    return true;
  }

  bool PackSignature(DXIL::SignatureKind sigKind) {
    DXIL::SigPointKind sigPointKind = SigPoint::GetKind((DXIL::ShaderKind)shaderStage, sigKind, false, false);
    const SigPoint *SP = SigPoint::GetSigPoint(sigPointKind);
    DXIL::PackingKind PK = SP->GetPackingKind();
    ElementVec *pElements = nullptr;
    switch (sigKind)
    {
    case hlsl::DXIL::SignatureKind::Input: pElements = &sigInputs; break;
    case hlsl::DXIL::SignatureKind::Output: pElements = &sigOutputs; break;
    case hlsl::DXIL::SignatureKind::PatchConstant: pElements = &sigPC; break;
    case hlsl::DXIL::SignatureKind::Invalid:
    default:
      return false;
    }
    switch (PK)
    {
    case hlsl::DXIL::PackingKind::InputAssembler: {
        unsigned row = 0;
        for (auto &E : *pElements) {
          E.StartRow = row;
          E.ColsAndStart = 0x40 | (E.ColsAndStart & 0xF);
          row += E.Rows;
        }
        break;
      }
    case hlsl::DXIL::PackingKind::Vertex:
    case hlsl::DXIL::PackingKind::PatchConstant:
      for (unsigned streamIndex = 0; streamIndex < 4; streamIndex++) {
        if (!PackElements(*pElements, sigPointKind, streamIndex))
          return false;
        if (sigPointKind != DXIL::SigPointKind::GSOut)
          break;
      }
      break;
    case hlsl::DXIL::PackingKind::Target:
      for (auto &E : *pElements) {
        E.StartRow = (uint8_t)semIndexBuffer[E.SemanticIndexes];
        E.ColsAndStart = 0x40 | (E.ColsAndStart & 0xF);
      }
      break;
    case hlsl::DXIL::PackingKind::None:
    case hlsl::DXIL::PackingKind::Invalid:
    default:
      break;
    }
    return true;
  }

  bool PackSignatures() {
    if (!PackSignature(DXIL::SignatureKind::Input))
      return false;
    if (!PackSignature(DXIL::SignatureKind::Output))
      return false;
    if (!PackSignature(DXIL::SignatureKind::PatchConstant))
      return false;
    return true;
  }

  void InitPSV() {
    PSVInitInfo initInfo(1);
    initInfo.ResourceCount = 0;
    initInfo.ShaderStage = shaderStage;
    initInfo.StringTable.Table = stringBuffer.data();
    initInfo.StringTable.Size = stringBuffer.size();
    initInfo.SemanticIndexTable.Table = semIndexBuffer.data();
    initInfo.SemanticIndexTable.Entries = semIndexBuffer.size();
    initInfo.UsesViewID = 1;  // Always true will make mask always available
    initInfo.SigInputElements = sigInputs.size();
    initInfo.SigOutputElements = sigOutputs.size();
    initInfo.SigPatchConstantElements = sigPC.size();

    VERIFY_IS_TRUE(PackSignatures());

    initInfo.SigInputVectors = 0;
    for (auto &E : sigInputs)
      initInfo.SigInputVectors = (uint8_t)std::max(initInfo.SigInputVectors, (uint8_t)(E.StartRow + E.Rows));

    initInfo.SigPatchConstantVectors = 0;
    for (auto &E : sigPC)
      initInfo.SigPatchConstantVectors = (uint8_t)std::max(initInfo.SigPatchConstantVectors, (uint8_t)(E.StartRow + E.Rows));

    for (auto &E : sigOutputs) {
      for (unsigned i = 0; i < 4; i++) {
        if ((E.DynamicMaskAndStream >> 4) == i)
          initInfo.SigOutputVectors[i] = (uint8_t)std::max(initInfo.SigOutputVectors[i], (uint8_t)(E.StartRow + E.Rows));
      }
    }

    uint32_t psvSize = 0;
    VERIFY_IS_TRUE(PSV.InitNew(initInfo, nullptr, &psvSize));
    psvBuffer.resize(psvSize);
    VERIFY_IS_TRUE(PSV.InitNew(initInfo, psvBuffer.data(), &psvSize));

    for (unsigned i = 0; i < sigInputs.size(); i++) {
      PSVSignatureElement0 *pEl = PSV.GetInputElement0(i);
      VERIFY_IS_NOT_NULL(pEl);
      memcpy(pEl, &sigInputs[i], sizeof(PSVSignatureElement0));
    }

    for (unsigned i = 0; i < sigPC.size(); i++) {
      PSVSignatureElement0 *pEl = PSV.GetPatchConstantElement0(i);
      VERIFY_IS_NOT_NULL(pEl);
      memcpy(pEl, &sigPC[i], sizeof(PSVSignatureElement0));
    }

    for (unsigned i = 0; i < sigOutputs.size(); i++) {
      PSVSignatureElement0 *pEl = PSV.GetOutputElement0(i);
      VERIFY_IS_NOT_NULL(pEl);
      memcpy(pEl, &sigOutputs[i], sizeof(PSVSignatureElement0));
    }
  }

};

} // namespace anonymous

class ViewIDValidationTest {
public:
  BEGIN_TEST_CLASS(ViewIDValidationTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  TEST_METHOD(Basic)
  TEST_METHOD(ArrayIndexed)

  dxc::DxcDllSupport m_dllSupport;
  VersionSupportInfo m_ver;

  void CheckPSVSequence(llvm::ArrayRef<DxilPipelineStateValidation*> psvVec,
                        unsigned viewIDCount,
                        ViewIDValidator::Result expectedResult,
                        unsigned gsRastStreamIndex = 0,
                        int finalStageIdx = -1) {
    std::unique_ptr<ViewIDValidator> validator(NewViewIDValidator(viewIDCount, gsRastStreamIndex));
    unsigned psvLeft = psvVec.size();
    if (finalStageIdx == -1)
      finalStageIdx = psvLeft - 1;
    for (auto &pPSV : psvVec) {
      unsigned mismatchElementId = (unsigned)-1;
      ViewIDValidator::Result result = validator->ValidateStage(*pPSV, !(finalStageIdx--), mismatchElementId);
      // expectedResult is for the last stage only, all others should succeed.
      if (--psvLeft) {
        VERIFY_ARE_EQUAL(result, ViewIDValidator::Result::Success);
      } else {
        VERIFY_ARE_EQUAL(result, expectedResult);
      }
    }
  }

};

bool ViewIDValidationTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    m_ver.Initialize(m_dllSupport);
  }
  return true;
}

void AddVS1(MockPSV &mock, DXIL::SignatureKind sigKind, unsigned streamIndex = 0) {
  mock.AddSimple(sigKind, PSVSemanticKind::Position);
  mock.AddArb(sigKind, 29, 4, hlsl::DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, 0, streamIndex);
  mock.AddArb(sigKind, 1, 2, hlsl::DxilProgramSigCompType::Float32, DXIL::InterpolationMode::LinearNoperspective, -1, streamIndex);
}

TEST_F(ViewIDValidationTest, Basic) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  unsigned pos = 0, pad = 1, tex = 2; // AddVS1 ids

  MockPSV vs(PSVShaderKind::Vertex);
  AddVS1(vs, DXIL::SignatureKind::Output);
  vs.InitPSV();

  MockPSV gs(PSVShaderKind::Geometry);
  AddVS1(gs, DXIL::SignatureKind::Input);
  AddVS1(gs, DXIL::SignatureKind::Output);
  gs.InitPSV();
  // Dependency and other PSV properties must be set after InitPSV()
  gs.SetInputOutputDependency(pos, 0, 0, pos, 0, 0);
  gs.SetInputOutputDependency(tex, 0, 0, tex, 0, 0);
  gs.SetInputOutputDependency(pad, 0, 0, pad, 0, 0);
  gs.SetInputOutputDependency(pad, 0, 1, pad, 0, 1);
  gs.PSV.GetPSVRuntimeInfo1()->MaxVertexCount = 8;

  MockPSV ps(PSVShaderKind::Pixel);
  AddVS1(ps, DXIL::SignatureKind::Input);
  //ps.AddArb(ps.sigInputs, 1, 2);
  ps.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::Target);
  ps.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::Depth);
  ps.InitPSV();

  unsigned mismatchElementId;

  {
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(4, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(gs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(ps.PSV, true, mismatchElementId));
  }

  // pos.x dependent on ViewID in VS
  vs.SetOutputViewIDDependency(pos, 0, 0);

  {
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(3, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(gs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(ps.PSV, true, mismatchElementId));
  }

  { // Detected at input to GS
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(4, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientSpace, val->ValidateStage(gs.PSV, false, mismatchElementId));
  }

  { // Detected at output of VS if final stage
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(4, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientSpace, val->ValidateStage(vs.PSV, true, mismatchElementId));
  }

  MockPSV gsPrimID(gs);
  gsPrimID.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::PrimitiveID);
  gsPrimID.InitPSV();
  gsPrimID.SetInputOutputDependency(pos, 0, 0, pos, 0, 0);
  gsPrimID.PSV.GetPSVRuntimeInfo1()->MaxVertexCount = 8;

  { // Detected at output from GS (outputs checked immediately in GS)
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(3, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientSpace, val->ValidateStage(gsPrimID.PSV, false, mismatchElementId));
  }

  MockPSV gsMaxVertexCount(gs);
  gsMaxVertexCount.PSV.GetPSVRuntimeInfo1()->MaxVertexCount = 9;

  { // Too many components due to MaxVertexCount
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(3, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientSpace, val->ValidateStage(gsMaxVertexCount.PSV, false, mismatchElementId));
  }

  MockPSV psPrimID(ps);
  psPrimID.AddSimple(DXIL::SignatureKind::Input, PSVSemanticKind::PrimitiveID);
  psPrimID.InitPSV();

  { // Detected at input to PS with merged signature
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(3, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(gs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientSpace, val->ValidateStage(psPrimID.PSV, true, mismatchElementId));
  }

}

TEST_F(ViewIDValidationTest, ArrayIndexed) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  MockPSV vs(PSVShaderKind::Vertex);
  unsigned posId = vs.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::Position);
  // Add dynamically indexed array that won't fit with position when duplicated 4 times
  unsigned arrayId = vs.AddArb(DXIL::SignatureKind::Output, 8, 1, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/1);
  vs.InitPSV();

  // pos.x dependent on ViewID in VS
  vs.SetOutputViewIDDependency(posId, 0, 0);

  MockPSV hs(PSVShaderKind::Hull);
  hs.AddSimple(DXIL::SignatureKind::Input, PSVSemanticKind::Position);
  hs.AddArb(DXIL::SignatureKind::Input, 8, 1, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/1);
  hs.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::Position);
  hs.AddArb(DXIL::SignatureKind::Output, 8, 1, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/1);
  hs.InitPSV();

  // control point output array[2] affected by pos.x, that's affected by ViewID in VS
  hs.SetInputOutputDependency(posId, 0, 0, arrayId, 2, 0);

  MockPSV ds(PSVShaderKind::Domain);
  ds.AddSimple(DXIL::SignatureKind::Input, PSVSemanticKind::Position);
  ds.AddArb(DXIL::SignatureKind::Input, 8, 1, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/1);
  ds.InitPSV();

  unsigned mismatchElementId;

  {
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(4, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(hs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientSpace, val->ValidateStage(ds.PSV, true, mismatchElementId));
  }

  hs.AddSimple(DXIL::SignatureKind::PatchConstant, PSVSemanticKind::Position, 1, 4, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Undefined);
  hs.AddArb(DXIL::SignatureKind::PatchConstant, 9, 1, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Undefined, /*dynMask*/1);
  hs.InitPSV();

  // PC output array[2] affected by pos.x, that's affected by ViewID in VS
  hs.SetInputPCOutputDependency(posId, 0, 0, arrayId, 2, 0);

  ds.AddSimple(DXIL::SignatureKind::PatchConstant, PSVSemanticKind::Position, 1, 4, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Undefined);
  ds.AddArb(DXIL::SignatureKind::PatchConstant, 9, 1, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Undefined, /*dynMask*/1);
  ds.InitPSV();

  {
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(4, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(hs.PSV, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientPCSpace, val->ValidateStage(ds.PSV, true, mismatchElementId));
  }

}
