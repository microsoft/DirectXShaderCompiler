///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ViewIDPipelineValidation.inl                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// This file implements inter-stage validation for ViewID.                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////


namespace hlsl {

namespace {

typedef std::vector<DxilSignatureAllocator::DummyElement> ElementVec;

struct ComponentMask : public PSVComponentMask {
  uint32_t Data[4];
  ComponentMask() : PSVComponentMask(Data, 0) {
    memset(Data, 0, sizeof(Data));
  }
  ComponentMask(const ComponentMask &other) : PSVComponentMask(Data, other.NumVectors) {
    *this = other;
  }
  ComponentMask(const PSVComponentMask &other) : PSVComponentMask(Data, other.NumVectors) {
    *this = other;
  }
  ComponentMask &operator=(const PSVComponentMask &other) {
    NumVectors = other.NumVectors;
    if (other.Mask && NumVectors) {
      memcpy(Data, other.Mask, sizeof(uint32_t) * PSVComputeMaskDwordsFromVectors(NumVectors));
    }
    else {
      memset(Data, 0, sizeof(Data));
    }
    return *this;
  }
  ComponentMask &operator|=(const PSVComponentMask &other) {
    NumVectors = std::max(NumVectors, other.NumVectors);
    PSVComponentMask::operator|=(other);
    return *this;
  }
};

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
}

static void CopyElements( ElementVec &outElements,
                          DXIL::SigPointKind sigPoint,
                          unsigned numElements,
                          unsigned streamIndex,
                          std::function<PSVSignatureElement(unsigned)> getElement) {
  outElements.clear();
  outElements.reserve(numElements);
  for (unsigned i = 0; i < numElements; i++) {
    auto inEl = getElement(i);
    if (!inEl.IsAllocated() || inEl.GetOutputStream() != streamIndex)
      continue;
    outElements.emplace_back(i);
    DxilSignatureAllocator::DummyElement &el = outElements.back();
    InitElement(el, inEl, sigPoint);
  }
}

static void AddViewIDElements(ElementVec &outElements,
                              ElementVec &inElements,
                              PSVComponentMask &mask,
                              unsigned viewIDCount) {
  // Compute needed elements
  for (unsigned adding = 0; adding < 2; adding++) {
    uint32_t numElements = 0;
    for (auto &E : inElements) {
      for (uint32_t row = 0; row < E.rows; row++) {
        for (uint32_t col = 0; col < E.cols; col++) {
          bool bDynIndex = E.indexFlags & (1 << col);
          if (row > 0 && bDynIndex)
            continue;
          if (adding) {
            uint32_t componentIndex = (E.GetStartRow() + row) * 4 + E.GetStartCol() + col;
            DxilSignatureAllocator::DummyElement NE(numElements);
            NE.rows = bDynIndex ? E.GetRows() : 1;
            if (mask.Get(componentIndex)) {
              NE.rows *= viewIDCount;
            }
            NE.kind = E.kind;
            NE.interpolation = E.interpolation;
            NE.interpretation = E.interpretation;
            outElements.push_back(NE);
          }
          numElements++;
        }
      }
    }
    if (!adding)
      outElements.resize(numElements);
  }
}

static bool CheckFit(ElementVec &elements) {
  std::vector<DxilSignatureAllocator::PackElement*> packElements;
  packElements.reserve(elements.size());
  for (auto &E : elements)
    packElements.push_back(&E);
  DxilSignatureAllocator alloc(32);
  alloc.PackOptimized(packElements, 0, 32);
  for (auto &E : elements) {
    if (!E.IsAllocated())
      return false;
  }
  return true;
}

static bool MergeElements(const ElementVec &priorElements,
                          ElementVec &inputElements,
                          uint32_t &numVectors,
                          unsigned &mismatchElementId) {
  inputElements.reserve(std::max(priorElements.size(), inputElements.size()));
  unsigned minElements = (unsigned)std::min(priorElements.size(), inputElements.size());
  for (unsigned i = 0; i < minElements; i++) {
    const DxilSignatureAllocator::DummyElement &priorEl = priorElements[i];
    DxilSignatureAllocator::DummyElement &inputEl = inputElements[i];
    // Verify elements match
    if (priorEl.rows != inputEl.rows ||
      priorEl.cols != inputEl.cols ||
      priorEl.row != inputEl.row ||
      priorEl.col != inputEl.col ||
      priorEl.kind != inputEl.kind ||
      priorEl.interpolation != inputEl.interpolation ||
      priorEl.interpretation != inputEl.interpretation) {
      mismatchElementId = inputEl.id;
      return false;
    }
    // OR prior dynamic index flags into input element
    inputEl.indexFlags |= priorEl.indexFlags;
  }

  // Add extra incoming elements if there are more
  for (unsigned i = (unsigned)inputElements.size(); i < (unsigned)priorElements.size(); i++) {
    inputElements.push_back(priorElements[i]);
  }

  // Update numVectors to max
  for (unsigned i = 0; i < inputElements.size(); i++) {
    DxilSignatureAllocator::DummyElement &inputEl = inputElements[i];
    numVectors = std::max(numVectors, inputEl.row + inputEl.rows);
  }
  return true;
}

static void PropegateMask(const ComponentMask &priorMask,
                          ElementVec &inputElements,
                          ComponentMask &outMask,
                          std::function<PSVComponentMask(unsigned)> getMask) {
  // Iterate elements
  for (auto &E : inputElements) {
    for (unsigned row = 0; row < E.GetRows(); row++) {
      for (unsigned col = 0; col < E.GetCols(); col++) {
        uint32_t componentIndex = (E.GetStartRow() + row) * 4 + E.GetStartCol() + col;
        // If bit set in priorMask
        if (priorMask.Get(componentIndex)) {
          // get mask of outputs affected by inputs and OR into outMask
          outMask |= getMask(componentIndex);
        }
      }
    }
  }
}

class ViewIDValidator_impl : public hlsl::ViewIDValidator {
  ComponentMask m_PriorOutputMask;
  ComponentMask m_PriorPCMask;
  ElementVec m_PriorOutputSignature;
  ElementVec m_PriorPCSignature;
  unsigned m_ViewIDCount;
  unsigned m_GSRastStreamIndex;

public:
  ViewIDValidator_impl(unsigned viewIDCount, unsigned gsRastStreamIndex)
    : m_PriorOutputMask(),
      m_ViewIDCount(viewIDCount),
      m_GSRastStreamIndex(gsRastStreamIndex)
  {}
  virtual ~ViewIDValidator_impl() {}
  __override Result ValidateStage(const DxilPipelineStateValidation &PSV,
                                  unsigned &mismatchElementId) {
    switch (PSV.GetShaderKind()) {
    case PSVShaderKind::Vertex: {
      // Initialize mask with direct ViewID dependent outputs
      ComponentMask mask(PSV.GetViewIDOutputMask(0));

      // capture output signature
      ElementVec outSig;
      CopyElements( outSig, DXIL::SigPointKind::VSOut, PSV.GetSigOutputElements(), 0,
                    [&](unsigned i) -> PSVSignatureElement {
                      return PSV.GetSignatureElement(PSV.GetOutputElement0(i));
                    });

      // Copy mask to prior mask
      m_PriorOutputMask = mask;

      // Capture output signature for next stage
      m_PriorOutputSignature = std::move(outSig);

      break;
    }
    case PSVShaderKind::Hull: {
      // Initialize mask with direct ViewID dependent outputs
      ComponentMask outputMask(PSV.GetViewIDOutputMask(0));
      ComponentMask pcMask(PSV.GetViewIDPCOutputMask());

      // capture signatures
      ElementVec inSig, outSig, pcSig;
      CopyElements( inSig, DXIL::SigPointKind::HSCPIn, PSV.GetSigInputElements(), 0,
                    [&](unsigned i) -> PSVSignatureElement {
                      return PSV.GetSignatureElement(PSV.GetInputElement0(i));
                    });
      CopyElements( outSig, DXIL::SigPointKind::HSCPOut, PSV.GetSigOutputElements(), 0,
                    [&](unsigned i) -> PSVSignatureElement {
                      return PSV.GetSignatureElement(PSV.GetOutputElement0(i));
                    });
      CopyElements( pcSig, DXIL::SigPointKind::PCOut, PSV.GetSigPatchConstantElements(), 0,
                    [&](unsigned i) -> PSVSignatureElement {
                      return PSV.GetSignatureElement(PSV.GetPatchConstantElement0(i));
                    });

      // Merge prior and input signatures, update prior mask size if necessary
      if (!MergeElements(m_PriorOutputSignature, inSig, m_PriorOutputMask.NumVectors, mismatchElementId))
        return Result::MismatchedSignatures;

      // Create new version with ViewID elements from merged signature
      ElementVec viewIDSig;
      AddViewIDElements(viewIDSig, inSig, m_PriorOutputMask, m_ViewIDCount);

      // Verify fit
      if (!CheckFit(viewIDSig))
        return Result::InsufficientSpace;

      // Propegate prior mask through input-output dependencies
      if (PSV.GetInputToOutputTable(0).IsValid()) {
        PropegateMask(m_PriorOutputMask, inSig, outputMask,
                      [&](unsigned i) -> PSVComponentMask { return PSV.GetInputToOutputTable(0).GetMaskForInput(i); });
      }
      if (PSV.GetInputToPCOutputTable().IsValid()) {
        PropegateMask(m_PriorOutputMask, inSig, pcMask,
                      [&](unsigned i) -> PSVComponentMask { return PSV.GetInputToPCOutputTable().GetMaskForInput(i); });
      }

      // Copy mask to prior mask
      m_PriorOutputMask = outputMask;
      m_PriorPCMask = pcMask;

      // Capture output signature for next stage
      m_PriorOutputSignature = std::move(outSig);
      m_PriorPCSignature = std::move(pcSig);

      break;
    }
    case PSVShaderKind::Domain: {
      // Initialize mask with direct ViewID dependent outputs
      ComponentMask mask(PSV.GetViewIDOutputMask(0));

      // capture signatures
      ElementVec inSig, pcSig, outSig;
      CopyElements( inSig, DXIL::SigPointKind::DSCPIn, PSV.GetSigInputElements(), 0,
                    [&](unsigned i) -> PSVSignatureElement {
                      return PSV.GetSignatureElement(PSV.GetInputElement0(i));
                    });
      CopyElements( pcSig, DXIL::SigPointKind::DSCPIn, PSV.GetSigPatchConstantElements(), 0,
                    [&](unsigned i) -> PSVSignatureElement {
                      return PSV.GetSignatureElement(PSV.GetPatchConstantElement0(i));
                    });
      CopyElements( outSig, DXIL::SigPointKind::DSOut, PSV.GetSigOutputElements(), 0,
                    [&](unsigned i) -> PSVSignatureElement {
                      return PSV.GetSignatureElement(PSV.GetOutputElement0(i));
                    });

      // Merge prior and input signatures, update prior mask size if necessary
      if (!MergeElements(m_PriorOutputSignature, inSig, m_PriorOutputMask.NumVectors, mismatchElementId))
        return Result::MismatchedSignatures;
      if (!MergeElements(m_PriorPCSignature, pcSig, m_PriorPCMask.NumVectors, mismatchElementId))
        return Result::MismatchedPCSignatures;

      {
        // Create new version with ViewID elements from merged signature
        ElementVec viewIDSig;
        AddViewIDElements(viewIDSig, inSig, m_PriorOutputMask, m_ViewIDCount);

        // Verify fit
        if (!CheckFit(viewIDSig))
          return Result::InsufficientSpace;
      }

      {
        // Create new version with ViewID elements from merged signature
        ElementVec viewIDSig;
        AddViewIDElements(viewIDSig, pcSig, m_PriorPCMask, m_ViewIDCount);

        // Verify fit
        if (!CheckFit(viewIDSig))
          return Result::InsufficientPCSpace;
      }

      // Propegate prior mask through input-output dependencies
      if (PSV.GetInputToOutputTable(0).IsValid()) {
        PropegateMask(m_PriorOutputMask, inSig, mask,
                      [&](unsigned i) -> PSVComponentMask { return PSV.GetInputToOutputTable(0).GetMaskForInput(i); });
      }
      if (PSV.GetPCInputToOutputTable().IsValid()) {
        PropegateMask(m_PriorPCMask, pcSig, mask,
                      [&](unsigned i) -> PSVComponentMask { return PSV.GetPCInputToOutputTable().GetMaskForInput(i); });
      }

      // Copy mask to prior mask
      m_PriorOutputMask = mask;
      m_PriorPCMask = ComponentMask();

      // Capture output signature for next stage
      m_PriorOutputSignature = std::move(outSig);
      m_PriorPCSignature.clear();

      break;
    }
    case PSVShaderKind::Geometry: {
      // Initialize mask with direct ViewID dependent outputs
      ComponentMask mask(PSV.GetViewIDOutputMask(m_GSRastStreamIndex));

      // capture signatures
      ElementVec inSig, outSig;
      CopyElements( inSig, DXIL::SigPointKind::GSVIn, PSV.GetSigInputElements(), 0,
                    [&](unsigned i) -> PSVSignatureElement {
                      return PSV.GetSignatureElement(PSV.GetInputElement0(i));
                    });
      CopyElements( outSig, DXIL::SigPointKind::GSOut, PSV.GetSigOutputElements(), m_GSRastStreamIndex,
                    [&](unsigned i) -> PSVSignatureElement {
                      return PSV.GetSignatureElement(PSV.GetOutputElement0(i));
                    });

      // Merge prior and input signatures, update prior mask size if necessary
      if (!MergeElements(m_PriorOutputSignature, inSig, m_PriorOutputMask.NumVectors, mismatchElementId))
        return Result::MismatchedSignatures;

      // Create new version with ViewID elements from merged signature
      ElementVec viewIDSig;
      AddViewIDElements(viewIDSig, inSig, m_PriorOutputMask, m_ViewIDCount);

      // Verify fit
      if (!CheckFit(viewIDSig))
        return Result::InsufficientSpace;

      // Propegate prior mask through input-output dependencies
      if (PSV.GetInputToOutputTable(0).IsValid()) {
        PropegateMask(m_PriorOutputMask, inSig, mask,
                      [&](unsigned i) -> PSVComponentMask { return PSV.GetInputToOutputTable(m_GSRastStreamIndex).GetMaskForInput(i); });
      }

      // Copy mask to prior mask
      m_PriorOutputMask = mask;

      // Capture output signature for next stage
      m_PriorOutputSignature = std::move(outSig);

      break;
    }
    case PSVShaderKind::Pixel: {
      // QUESTION: Do we propegate to PS SV_Target output?

      // capture signatures
      ElementVec inSig;
      CopyElements( inSig, DXIL::SigPointKind::PSIn, PSV.GetSigInputElements(), 0,
                    [&](unsigned i) -> PSVSignatureElement {
                      return PSV.GetSignatureElement(PSV.GetInputElement0(i));
                    });

      // Merge prior and input signatures, update prior mask size if necessary
      if (!MergeElements(m_PriorOutputSignature, inSig, m_PriorOutputMask.NumVectors, mismatchElementId))
        return Result::MismatchedSignatures;

      // Create new version with ViewID elements from merged signature
      ElementVec viewIDSig;
      AddViewIDElements(viewIDSig, inSig, m_PriorOutputMask, m_ViewIDCount);

      // Verify fit
      if (!CheckFit(viewIDSig))
        return Result::InsufficientSpace;

      // Final stage, so clear output state.
      m_PriorOutputMask = ComponentMask();
      m_PriorOutputSignature.clear();

      break;
    }
    case PSVShaderKind::Compute:
    default:
      return Result::InvalidUsage;
    }

    return Result::Success;
  }
};

} // namespace anonymous

ViewIDValidator* NewViewIDValidator(unsigned viewIDCount, unsigned gsRastStreamIndex) {
  return new ViewIDValidator_impl(viewIDCount, gsRastStreamIndex);
}

} // namespace hlsl
