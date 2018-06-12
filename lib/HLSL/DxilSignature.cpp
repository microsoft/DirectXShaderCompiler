///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSignature.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilSignature.h"
#include "dxc/HLSL/DxilSignatureAllocator.h"
#include "dxc/HLSL/DxilSigPoint.h"
#include "llvm/ADT/STLExtras.h"

using std::vector;
using std::unique_ptr;


namespace hlsl {

//------------------------------------------------------------------------------
//
// Singnature methods.
//
DxilSignature::DxilSignature(DXIL::ShaderKind shaderKind,
                             DXIL::SignatureKind sigKind,
                             bool useMinPrecision)
    : m_sigPointKind(SigPoint::GetKind(shaderKind, sigKind,
                                       /*isPatchConstantFunction*/ false,
                                       /*isSpecialInput*/ false)),
      m_UseMinPrecision(useMinPrecision) {}

DxilSignature::DxilSignature(DXIL::SigPointKind sigPointKind,
                             bool useMinPrecision)
    : m_sigPointKind(sigPointKind),
      m_UseMinPrecision(useMinPrecision) {}

DxilSignature::DxilSignature(const DxilSignature &src)
    : m_sigPointKind(src.m_sigPointKind),
      m_UseMinPrecision(src.m_UseMinPrecision) {
  const bool bSetID = false;
  for (auto &Elt : src.GetElements()) {
    std::unique_ptr<DxilSignatureElement> newElt = CreateElement();
    newElt->Initialize(Elt->GetName(), Elt->GetCompType(),
                       Elt->GetInterpolationMode()->GetKind(), Elt->GetRows(),
                       Elt->GetCols(), Elt->GetStartRow(), Elt->GetStartCol(),
                       Elt->GetID(), Elt->GetSemanticIndexVec());
    AppendElement(std::move(newElt), bSetID);
  }
}

DxilSignature::~DxilSignature() {
}

bool DxilSignature::IsInput() const {
  return SigPoint::GetSigPoint(m_sigPointKind)->IsInput();
}

bool DxilSignature::IsOutput() const {
  return SigPoint::GetSigPoint(m_sigPointKind)->IsOutput();
}

unique_ptr<DxilSignatureElement> DxilSignature::CreateElement() {
  return llvm::make_unique<DxilSignatureElement>(m_sigPointKind);
}

unsigned DxilSignature::AppendElement(std::unique_ptr<DxilSignatureElement> pSE, bool bSetID) {
  DXASSERT_NOMSG((unsigned)m_Elements.size() < UINT_MAX);
  unsigned Id = (unsigned)m_Elements.size();
  if (bSetID) {
    pSE->SetID(Id);
  }
  m_Elements.emplace_back(std::move(pSE));
  return Id;
}

DxilSignatureElement &DxilSignature::GetElement(unsigned idx) {
  return *m_Elements[idx];
}

const DxilSignatureElement &DxilSignature::GetElement(unsigned idx) const {
  return *m_Elements[idx];
}

const std::vector<std::unique_ptr<DxilSignatureElement> > &DxilSignature::GetElements() const {
  return m_Elements;
}

namespace {

static bool ShouldBeAllocated(const DxilSignatureElement *SE) {
  DXIL::SemanticInterpretationKind I = SE->GetInterpretation();
  switch (I) {
  case DXIL::SemanticInterpretationKind::NA:
  case DXIL::SemanticInterpretationKind::NotInSig:
  case DXIL::SemanticInterpretationKind::NotPacked:
  case DXIL::SemanticInterpretationKind::Shadow:
    return false;
  default:
    break;
  }
  return true;
}

} // anonymous namespace


bool DxilSignature::IsFullyAllocated() const {
  for (auto &SE : m_Elements) {
    if (!ShouldBeAllocated(SE.get()))
      continue;
    if (!SE->IsAllocated())
      return false;
  }
  return true;
}

unsigned DxilSignature::NumVectorsUsed(unsigned streamIndex) const {
  unsigned NumVectors = 0;
  for (auto &SE : m_Elements) {
    if (SE->IsAllocated() && SE->GetOutputStream() == streamIndex)
      NumVectors = std::max(NumVectors, (unsigned)SE->GetStartRow() + SE->GetRows());
  }
  return NumVectors;
}

unsigned DxilSignature::PackElements(DXIL::PackingStrategy packing) {
  unsigned rowsUsed = 0;

  // Transfer to elements derived from DxilSignatureAllocator::PackElement
  std::vector<DxilPackElement> packElements;
  for (auto &SE : m_Elements) {
    if (ShouldBeAllocated(SE.get()))
      packElements.emplace_back(SE.get(), m_UseMinPrecision);
  }

  if (m_sigPointKind == DXIL::SigPointKind::GSOut) {
    // Special case due to support for multiple streams
    DxilSignatureAllocator alloc[4] = {{32, UseMinPrecision()},
                                       {32, UseMinPrecision()},
                                       {32, UseMinPrecision()},
                                       {32, UseMinPrecision()}};
    std::vector<DxilSignatureAllocator::PackElement*> elements[4];
    for (auto &SE : packElements) {
      elements[SE.Get()->GetOutputStream()].push_back(&SE);
    }
    for (unsigned i = 0; i < 4; ++i) {
      if (!elements[i].empty()) {
        unsigned streamRowsUsed = 0;
        switch (packing) {
        case DXIL::PackingStrategy::PrefixStable:
          streamRowsUsed = alloc[i].PackPrefixStable(elements[i], 0, 32);
          break;
        case DXIL::PackingStrategy::Optimized:
          streamRowsUsed = alloc[i].PackOptimized(elements[i], 0, 32);
          break;
        default:
          DXASSERT(false, "otherwise, invalid packing strategy supplied");
        }
        if (streamRowsUsed > rowsUsed)
          rowsUsed = streamRowsUsed;
      }
    }
    // rowsUsed isn't really meaningful in this case.
    return rowsUsed;
  }

  const SigPoint *SP = SigPoint::GetSigPoint(m_sigPointKind);
  DXIL::PackingKind PK = SP->GetPackingKind();

  switch (PK) {
  case DXIL::PackingKind::None:
    // no packing.
    break;

  case DXIL::PackingKind::InputAssembler:
    // incrementally assign each element that belongs in the signature to the start of the next free row
    for (auto &SE : packElements) {
      SE.SetLocation(rowsUsed, 0);
      rowsUsed += SE.GetRows();
    }
    break;

  case DXIL::PackingKind::Vertex:
  case DXIL::PackingKind::PatchConstant: {
      DxilSignatureAllocator alloc(32, UseMinPrecision());
      std::vector<DxilSignatureAllocator::PackElement*> elements;
      elements.reserve(packElements.size());
      for (auto &SE : packElements){
        elements.push_back(&SE);
      }
      switch (packing) {
      case DXIL::PackingStrategy::PrefixStable:
        rowsUsed = alloc.PackPrefixStable(elements, 0, 32);
        break;
      case DXIL::PackingStrategy::Optimized:
        rowsUsed = alloc.PackOptimized(elements, 0, 32);
        break;
      default:
        DXASSERT(false, "otherwise, invalid packing strategy supplied");
      }
    }
    break;

  case DXIL::PackingKind::Target:
    // for SV_Target, assign rows according to semantic index, the rest are unassigned (-1)
    // Note: Overlapping semantic indices should be checked elsewhere
    for (auto &SE : packElements) {
      if (SE.GetKind() != DXIL::SemanticKind::Target)
        continue;
      unsigned row = SE.Get()->GetSemanticStartIndex();
      SE.SetLocation(row, 0);
      DXASSERT(SE.GetRows() == 1, "otherwise, SV_Target output not broken into separate rows earlier");
      row += SE.GetRows();
      if (rowsUsed < row)
        rowsUsed = row;
    }
    break;

  case DXIL::PackingKind::Invalid:
  default:
    DXASSERT(false, "unexpected PackingKind.");
  }

  return rowsUsed;
}

//------------------------------------------------------------------------------
//
// EntrySingnature methods.
//
DxilEntrySignature::DxilEntrySignature(const DxilEntrySignature &src)
    : InputSignature(src.InputSignature), OutputSignature(src.OutputSignature),
      PatchConstantSignature(src.PatchConstantSignature) {}

} // namespace hlsl

#include <algorithm>
#include "dxc/HLSL/DxilSignatureAllocator.inl"
#include "dxc/HLSL/DxilSigPoint.inl"
#include "dxc/HLSL/DxilPipelineStateValidation.h"
#include <functional>
#include "dxc/HLSL/ViewIDPipelineValidation.inl"
