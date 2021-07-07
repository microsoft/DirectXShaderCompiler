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
    const PSVSignatureElement0 &inE = sigPC[inId];
    const PSVSignatureElement0 &outE = sigOutputs[outId];
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
  TEST_METHOD(ClipCull)
  TEST_METHOD(LargeVertices)

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
      bool bExpandInputOnly = false;
      if (finalStageIdx == 1 && psvLeft > 1) {
        bExpandInputOnly = true;
      }
      ViewIDValidator::Result result = validator->ValidateStage(*pPSV, !(finalStageIdx--), bExpandInputOnly, mismatchElementId);
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

static void AddVS1(MockPSV &mock, DXIL::SignatureKind sigKind, unsigned streamIndex = 0) {
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
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(gs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(ps.PSV, true, false, mismatchElementId));
  }

  // pos.x dependent on ViewID in VS
  vs.SetOutputViewIDDependency(pos, 0, 0);

  {
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(3, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(gs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(ps.PSV, true, false, mismatchElementId));
  }

  { // Detected at input to GS
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(4, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientInputSpace, val->ValidateStage(gs.PSV, false, false, mismatchElementId));
  }

  { // Detected at output of VS if final stage
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(4, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientOutputSpace, val->ValidateStage(vs.PSV, true, false, mismatchElementId));
  }

  MockPSV gsPrimID(gs);
  gsPrimID.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::PrimitiveID);
  gsPrimID.InitPSV();
  gsPrimID.SetInputOutputDependency(pos, 0, 0, pos, 0, 0);
  gsPrimID.PSV.GetPSVRuntimeInfo1()->MaxVertexCount = 8;

  { // Detected at output from GS (outputs checked immediately in GS)
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(3, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientOutputSpace, val->ValidateStage(gsPrimID.PSV, false, false, mismatchElementId));
  }

  MockPSV gsMaxVertexCount(gs);
  gsMaxVertexCount.PSV.GetPSVRuntimeInfo1()->MaxVertexCount = 9;

  { // Too many components due to MaxVertexCount
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(3, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientOutputSpace, val->ValidateStage(gsMaxVertexCount.PSV, false, false, mismatchElementId));
  }

  MockPSV psPrimID(ps);
  psPrimID.AddSimple(DXIL::SignatureKind::Input, PSVSemanticKind::PrimitiveID);
  psPrimID.InitPSV();

  { // Detected at input to PS with merged signature
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(3, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(gs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientInputSpace, val->ValidateStage(psPrimID.PSV, true, false, mismatchElementId));
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
  // Output array is not dynamically indexed in HS
  hs.AddArb(DXIL::SignatureKind::Output, 8, 1, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/0);
  hs.InitPSV();

  // control point output array[2] affected by pos.x, that's affected by ViewID in VS
  hs.SetInputOutputDependency(posId, 0, 0, arrayId, 2, 0);

  MockPSV ds(PSVShaderKind::Domain);
  ds.AddSimple(DXIL::SignatureKind::Input, PSVSemanticKind::Position);
  // Input array is dynamically indexed in DS
  ds.AddArb(DXIL::SignatureKind::Input, 8, 1, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/1);
  ds.InitPSV();

  unsigned mismatchElementId;

  {
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(4, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(hs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientInputSpace, val->ValidateStage(ds.PSV, true, false, mismatchElementId));
  }

  hs.AddSimple(DXIL::SignatureKind::PatchConstant, PSVSemanticKind::Position, 1, 4, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Undefined);
  hs.AddArb(DXIL::SignatureKind::PatchConstant, 9, 2, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Undefined, /*dynMask*/1);
  hs.InitPSV();

  // PC output array[2].x affected by pos.x, that's affected by ViewID in VS (dynamically indexed in HS)
  hs.SetInputPCOutputDependency(posId, 0, 0, arrayId, 2, 0);

  ds.AddSimple(DXIL::SignatureKind::PatchConstant, PSVSemanticKind::Position, 1, 4, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Undefined);
  // PC input array[].x is not dynamically indexed by DS, but output is dynamically indexed by HS.
  ds.AddArb(DXIL::SignatureKind::PatchConstant, 9, 2, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Undefined, /*dynMask*/0);
  ds.InitPSV();

  {
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(4, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(hs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::InsufficientPCSpace, val->ValidateStage(ds.PSV, true, false, mismatchElementId));
  }

  hs.InitPSV();

  // PC output array[1].y affected by pos.x, that's affected by ViewID in VS, but array[].y is not dynamically indexed
  hs.SetInputPCOutputDependency(posId, 0, 0, arrayId, 1, 1);

  {
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(4, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(hs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(ds.PSV, true, false, mismatchElementId));
  }

}

TEST_F(ViewIDValidationTest, ClipCull) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  MockPSV vs(PSVShaderKind::Vertex);
  MockPSV ps(PSVShaderKind::Pixel);

  unsigned posId = vs.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::Position);
  ps.AddSimple(DXIL::SignatureKind::Input, PSVSemanticKind::Position);

  unsigned clipId = vs.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::ClipDistance, 1, 2);
  ps.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::ClipDistance, 1, 2);
  vs.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::ClipDistance, 1, 2);
  ps.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::ClipDistance, 1, 2);

  unsigned cullId = vs.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::CullDistance, 1, 2);
  ps.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::CullDistance, 1, 2);
  vs.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::CullDistance, 1, 2);
  ps.AddSimple(DXIL::SignatureKind::Output, PSVSemanticKind::CullDistance, 1, 2);

  vs.InitPSV();
  ps.InitPSV();

  (void)posId; (void)clipId; (void)cullId;

  unsigned mismatchElementId;

  {
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(2, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(ps.PSV, true, false, mismatchElementId));
  }

  // clip.x dependent on ViewID in VS
  vs.SetOutputViewIDDependency(clipId, 0, 0);

  {
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(2, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(ps.PSV, true, false, mismatchElementId));
  }

  {
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(4, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(ps.PSV, true, false, mismatchElementId));
  }

}

TEST_F(ViewIDValidationTest, LargeVertices) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

#define FILLER_ARRAY_SIZE 30
#define FILLER_ARRAY_SIZE2 6
#define FILLER_ARRAY_SIZE3 17
#define FILLER_ARRAY_SIZE4 4
#define FILLER_ARRAY_SIZE5 5
#define FILLER_ARRAY_SIZE6 12
#define FILLER_ARRAY_SIZE7 2
#define FILLER_ARRAY_SIZE8 3
#define FILLER_ARRAY_SIZE9 3

  MockPSV vs(PSVShaderKind::Vertex);
  MockPSV hs(PSVShaderKind::Hull);
  MockPSV ds(PSVShaderKind::Domain);
  MockPSV gs(PSVShaderKind::Geometry);
  MockPSV ps(PSVShaderKind::Pixel);

  // struct vert6
  // {
  //     float4 pos : SV_POSITION;
  //     float2 tex : TEX;
  //     float4 filler[FILLER_ARRAY_SIZE] : FILLER;
  // };
  unsigned vs_pos, vs_tex, vs_filler;
  auto Add_vert = [&] (MockPSV &mock, DXIL::SignatureKind sigKind, unsigned streamIndex = 0) {
    vs_pos = mock.AddSimple(sigKind, PSVSemanticKind::Position);
    vs_tex = mock.AddArb(sigKind, 1, 2);
    vs_filler = mock.AddArb(sigKind, FILLER_ARRAY_SIZE, 4, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/0);
  };

  Add_vert(vs, DXIL::SignatureKind::Output);
  Add_vert(hs, DXIL::SignatureKind::Input);
  Add_vert(hs, DXIL::SignatureKind::Output);
  Add_vert(ds, DXIL::SignatureKind::Input);

  // struct PCOUT6
  // {
  //     float OutsideTF[4] : SV_TessFactor;
  //     float InsideTF[2] : SV_InsideTessFactor;
  //     float4 color : COLOR; // viewId dependent
  // };
  unsigned pc_OutsideTF, pc_InsideTF, pc_color;
  auto Add_pc = [&](MockPSV &mock, DXIL::SignatureKind sigKind, unsigned streamIndex = 0) {
    pc_OutsideTF = mock.AddSimple(sigKind, PSVSemanticKind::TessFactor, 4);
    pc_InsideTF = mock.AddSimple(sigKind, PSVSemanticKind::InsideTessFactor, 2);
    pc_color = mock.AddArb(sigKind, 1, 4);
  };

  Add_pc(hs, DXIL::SignatureKind::PatchConstant);
  Add_pc(ds, DXIL::SignatureKind::PatchConstant);

  // struct DSVert6
  // {
  //     float4 pos : SV_POSITION; // x is view ID dependent
  //     float4 color : COLOR; // view Id dependent
  //     float4 filler2[FILLER_ARRAY_SIZE2] : FILLER_A; // x is viewID dependent
  //     float3 filler3[FILLER_ARRAY_SIZE3] : FILLER_B;
  //     noperspective float3 filler9[FILLER_ARRAY_SIZE9] : FILLER_C;
  //     float  pad : PAD;
  //     float2 tex : TEX;
  // };
  unsigned ds_pos, ds_color, ds_filler2, ds_filler3, ds_filler9, ds_pad, ds_tex;
  auto Add_ds = [&](MockPSV &mock, DXIL::SignatureKind sigKind, unsigned streamIndex = 0) {
    ds_pos = mock.AddSimple(sigKind, PSVSemanticKind::Position);
    ds_color = mock.AddArb(sigKind, 1, 4, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/0);
    ds_filler2 = mock.AddArb(sigKind, FILLER_ARRAY_SIZE2, 4, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/15);
    ds_filler3 = mock.AddArb(sigKind, FILLER_ARRAY_SIZE3, 3, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/0);
    ds_filler9 = mock.AddArb(sigKind, FILLER_ARRAY_SIZE9, 3, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::LinearNoperspective, /*dynMask*/0);
    ds_pad = mock.AddArb(sigKind, 1, 1, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/0);
    ds_tex = mock.AddArb(sigKind, 1, 2, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/0);
  };
  Add_ds(ds, DXIL::SignatureKind::Output);
  Add_ds(gs, DXIL::SignatureKind::Input);

  // struct GSVert6
  // {
  //     float4 pos : SV_POSITION; // x is view ID dependent
  //     float4 color : COLOR; // viewID dependent
  //     float2 tex : TEX;
  // #ifdef EMULATE_VIEW_INSTANCING
  //     uint vpai : SV_ViewportArrayIndex;
  // #else
  //     nointerpolation float pad2 : FILLER_G;
  // #endif
  //     uint rtai : SV_RenderTargetArrayIndex; // viewID dependent
  //     float4 filler4[FILLER_ARRAY_SIZE4] : FILLER_A; // y is viewID dependent
  //     nointerpolation float3 filler5[FILLER_ARRAY_SIZE5] : FILLER_B;
  //     float3 filler6[FILLER_ARRAY_SIZE6] : FILLER_C;
  //     float3 filler7[FILLER_ARRAY_SIZE7] : FILLER_D;
  //     noperspective float3 filler8[FILLER_ARRAY_SIZE8] : FILLER_E;
  //     noperspective float4 pad : PAD;
  // };
  unsigned gs_pos, gs_color, gs_tex, gs_pad2, gs_rtai, gs_filler4, gs_filler5, gs_filler6, gs_filler7, gs_filler8, gs_pad;
  auto Add_gs = [&](MockPSV &mock, DXIL::SignatureKind sigKind, unsigned streamIndex = 0) {
    gs_pos = mock.AddSimple(sigKind, PSVSemanticKind::Position);
    gs_color = mock.AddArb(sigKind, 1, 4, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/0);
    gs_tex = mock.AddArb(sigKind, 1, 2, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/0);
    gs_pad2 = mock.AddArb(sigKind, 1, 1, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Constant, /*dynMask*/0);
    gs_rtai = mock.AddArb(sigKind, 1, 1, DxilProgramSigCompType::UInt32, DXIL::InterpolationMode::Constant, /*dynMask*/0);
    gs_filler4 = mock.AddArb(sigKind, FILLER_ARRAY_SIZE4, 4, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/0);
    gs_filler5 = mock.AddArb(sigKind, FILLER_ARRAY_SIZE5, 3, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Constant, /*dynMask*/0);
    gs_filler6 = mock.AddArb(sigKind, FILLER_ARRAY_SIZE6, 3, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/0);
    gs_filler7 = mock.AddArb(sigKind, FILLER_ARRAY_SIZE7, 3, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::Linear, /*dynMask*/0);
    gs_filler8 = mock.AddArb(sigKind, FILLER_ARRAY_SIZE8, 3, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::LinearNoperspective, /*dynMask*/0);
    gs_pad = mock.AddArb(sigKind, 1, 4, DxilProgramSigCompType::Float32, DXIL::InterpolationMode::LinearNoperspective, /*dynMask*/0);
  };

  Add_gs(gs, DXIL::SignatureKind::Output);
  Add_gs(ps, DXIL::SignatureKind::Input);


  vs.InitPSV();

  hs.InitPSV();
  for (unsigned i = 0; i < 4; i++) {
    hs.SetPCViewIDDependency(pc_color, 0, i);
  }

  ds.InitPSV();
  ds.SetOutputViewIDDependency(ds_pos, 0, 0);
  for (unsigned i = 1; i < 4; i++) {
    ds.SetInputOutputDependency(vs_pos, 0, i, ds_pos, 0, i);
  }
  // ret.color = tessFactors.color;
  for (unsigned i = 0; i < 4; i++) {
    ds.SetPCInputOutputDependency(pc_color, 0, i, ds_color, 0, i);
  }
  // for (uint i = 0; i < FILLER_ARRAY_SIZE2; i++)
  // {
  //     ret.filler2[i].x = tessFactors.color.x;
  //     ret.filler2[i].y = patch[0].filler[i].y;
  //     ret.filler2[i].z = patch[0].filler[i].z;
  //     ret.filler2[i].w = patch[0].filler[i].w;
  // }
  for (unsigned i = 0; i < FILLER_ARRAY_SIZE2; i++) {
    ds.SetPCInputOutputDependency(ds_color, 0, 0, ds_filler2, i, 0);
    for (unsigned j = 1; j < 4; j++) {
      ds.SetInputOutputDependency(vs_filler, i, j, ds_filler2, i, j);
    }
  }
  // for (i = 0; i < FILLER_ARRAY_SIZE3; i++)
  // {
  //     ret.filler3[i].x = patch[0].filler[i].y;
  //     ret.filler3[i].y = patch[0].filler[i].z;
  //     ret.filler3[i].z = patch[0].filler[i].w;
  // }
  for (unsigned i = 0; i < FILLER_ARRAY_SIZE3; i++) {
    for (unsigned j = 0; j < 3; j++) {
      ds.SetInputOutputDependency(vs_filler, i, j + 1, ds_filler3, i, j);
    }
  }
  // for (i = 0; i < FILLER_ARRAY_SIZE9; i++)
  // {
  //     ret.filler9[i].x = patch[0].filler[i].y;
  //     ret.filler9[i].y = patch[0].filler[i].z;
  //     ret.filler9[i].z = patch[0].filler[i].w;
  // }
  for (unsigned i = 0; i < FILLER_ARRAY_SIZE9; i++) {
    for (unsigned j = 0; j < 3; j++) {
      ds.SetInputOutputDependency(vs_filler, i, j + 1, ds_filler9, i, j);
    }
  }

  gs.InitPSV();
  // outVert.pos = pts[i].pos; // .x viewID dependent
  for (unsigned i = 0; i < 4; i++) {
    gs.SetInputOutputDependency(ds_pos, 0, i, gs_pos, 0, i);
  }
  // outVert.tex = pts[i].tex;
  for (unsigned i = 0; i < 2; i++) {
    gs.SetInputOutputDependency(ds_tex, 0, i, gs_tex, 0, i);
  }
  // outVert.color = pts[i].color; // viewID dependent
  for (unsigned i = 0; i < 4; i++) {
    gs.SetInputOutputDependency(ds_color, 0, i, gs_color, 0, i);
  }
  // outVert.rtai = viewID / 2; // 0 0 1 1
  gs.SetOutputViewIDDependency(gs_rtai, 0, 0);
  // for (uint i = 0; i < FILLER_ARRAY_SIZE4; i++)
  // {
  //     outVert.filler4[i].x = pts[0].filler2[i].y;
  //     outVert.filler4[i].y = pts[0].filler2[i].x; // viewID dependent
  //     outVert.filler4[i].z = pts[0].filler2[i].z;
  //     outVert.filler4[i].w = pts[0].filler2[i].w;
  // }
  for (unsigned i = 0; i < FILLER_ARRAY_SIZE4; i++) {
    gs.SetInputOutputDependency(ds_filler2, i, 1, gs_filler4, i, 0);
    gs.SetInputOutputDependency(ds_filler2, i, 0, gs_filler4, i, 1);
    gs.SetInputOutputDependency(ds_filler2, i, 2, gs_filler4, i, 2);
    gs.SetInputOutputDependency(ds_filler2, i, 3, gs_filler4, i, 3);
  }
  // for (i = 0; i < FILLER_ARRAY_SIZE5; i++)
  // {
  //     outVert.filler5[i].x = pts[0].filler2[0].y;
  //     outVert.filler5[i].y = pts[0].filler2[0].z;
  //     outVert.filler5[i].z = pts[0].filler2[0].w;
  // }
  for (unsigned i = 0; i < FILLER_ARRAY_SIZE5; i++) {
    for (unsigned j = 0; j < 3; j++) {
      gs.SetInputOutputDependency(ds_filler2, i, j + 1, gs_filler5, i, j);
    }
  }
  // for (i = 0; i < FILLER_ARRAY_SIZE6; i++)
  // {
  //     outVert.filler6[i].x = pts[0].filler2[i%FILLER_ARRAY_SIZE2].y;
  //     outVert.filler6[i].y = pts[0].filler2[i%FILLER_ARRAY_SIZE2].z;
  //     outVert.filler6[i].z = pts[0].filler2[i%FILLER_ARRAY_SIZE2].w;
  // }
  for (unsigned i = 0; i < FILLER_ARRAY_SIZE6; i++) {
    for (unsigned j = 0; j < 3; j++) {
      gs.SetInputOutputDependency(ds_filler2, i % FILLER_ARRAY_SIZE2, j + 1, gs_filler6, i, j);
    }
  }
  // for (i = 0; i < FILLER_ARRAY_SIZE7; i++)
  // {
  //     outVert.filler7[i].x = pts[0].filler2[i%FILLER_ARRAY_SIZE2].y;
  //     outVert.filler7[i].y = pts[0].filler2[i%FILLER_ARRAY_SIZE2].z;
  //     outVert.filler7[i].z = pts[0].filler2[i%FILLER_ARRAY_SIZE2].w;
  // }
  for (unsigned i = 0; i < FILLER_ARRAY_SIZE7; i++) {
    for (unsigned j = 0; j < 3; j++) {
      gs.SetInputOutputDependency(ds_filler2, i % FILLER_ARRAY_SIZE2, j + 1, gs_filler7, i, j);
    }
  }
  // for (i = 0; i < FILLER_ARRAY_SIZE8; i++)
  // {
  //     outVert.filler8[i].x = pts[0].filler2[i%FILLER_ARRAY_SIZE2].y;
  //     outVert.filler8[i].y = pts[0].filler2[i%FILLER_ARRAY_SIZE2].z;
  //     outVert.filler8[i].z = pts[0].filler2[i%FILLER_ARRAY_SIZE2].w;
  // }
  for (unsigned i = 0; i < FILLER_ARRAY_SIZE8; i++) {
    for (unsigned j = 0; j < 3; j++) {
      gs.SetInputOutputDependency(ds_filler2, i % FILLER_ARRAY_SIZE2, j + 1, gs_filler8, i, j);
    }
  }

  ps.InitPSV();


  unsigned mismatchElementId;

  {
    std::unique_ptr<ViewIDValidator> val(NewViewIDValidator(4, 0));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(vs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(hs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(ds.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(gs.PSV, false, false, mismatchElementId));
    VERIFY_ARE_EQUAL(ViewIDValidator::Result::Success, val->ValidateStage(ps.PSV, true, false, mismatchElementId));
  }
}


// TODO:
//  - test final stage validation for each stage
//  - test input only validation for each stage
//  - test multi-stream GS
//  - test tessfactor ViewID dependence in HS

