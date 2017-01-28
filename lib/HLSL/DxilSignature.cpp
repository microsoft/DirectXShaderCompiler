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

using std::vector;
using std::unique_ptr;


namespace hlsl {

//------------------------------------------------------------------------------
//
// Singnature methods.
//
DxilSignature::DxilSignature(DXIL::ShaderKind shaderKind, DXIL::SignatureKind sigKind)
: m_sigPointKind(SigPoint::GetKind(shaderKind, sigKind, /*isPatchConstantFunction*/false, /*isSpecialInput*/false)) {}

DxilSignature::DxilSignature(DXIL::SigPointKind sigPointKind)
: m_sigPointKind(sigPointKind) {}

DxilSignature::~DxilSignature() {
}

bool DxilSignature::IsInput() const {
  return SigPoint::GetSigPoint(m_sigPointKind)->IsInput();
}

bool DxilSignature::IsOutput() const {
  return SigPoint::GetSigPoint(m_sigPointKind)->IsOutput();
}

unique_ptr<DxilSignatureElement> DxilSignature::CreateElement() {
  return unique_ptr<DxilSignatureElement>(new DxilSignatureElement(m_sigPointKind));
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
  return *m_Elements[idx].get();
}

const DxilSignatureElement &DxilSignature::GetElement(unsigned idx) const {
  return *m_Elements[idx].get();
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
  }
  return true;
}

} // anonymous namespace


bool DxilSignature::IsFullyAllocated() {
  for (auto &SE : m_Elements) {
    if (!ShouldBeAllocated(SE.get()))
      continue;
    if (!SE->IsAllocated())
      return false;
  }
  return true;
}

unsigned DxilSignature::PackElements() {
  unsigned rowsUsed = 0;

  if (m_sigPointKind == DXIL::SigPointKind::GSOut) {
    // Special case due to support for multiple streams
    DxilSignatureAllocator alloc[4] = {32, 32, 32, 32};
    std::vector<DxilSignatureElement*> elements[4];
    for (auto &SE : m_Elements) {
      if (!ShouldBeAllocated(SE.get()))
        continue;
      elements[SE->GetOutputStream()].push_back(SE.get());
    }
    for (unsigned i = 0; i < 4; ++i) {
      if (!elements[i].empty()) {
        unsigned streamRowsUsed = alloc[i].PackMain(elements[i], 0, 32);
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
    for (auto &SE : m_Elements) {
      if (!ShouldBeAllocated(SE.get()))
        continue;
      SE->SetStartRow(rowsUsed);
      SE->SetStartCol(0);
      rowsUsed += SE->GetRows();
    }
    break;

  case DXIL::PackingKind::Vertex:
  case DXIL::PackingKind::PatchConstant: {
      DxilSignatureAllocator alloc(32);
      std::vector<DxilSignatureElement*> elements;
      elements.reserve(m_Elements.size());
      for (auto &SE : m_Elements){
        if (!ShouldBeAllocated(SE.get()))
          continue;
        elements.push_back(SE.get());
      }
      rowsUsed = alloc.PackMain(elements, 0, 32);
    }
    break;

  case DXIL::PackingKind::Target:
    // for SV_Target, assign rows according to semantic index, the rest are unassigned (-1)
    // Note: Overlapping semantic indices should be checked elsewhere
    for (auto &SE : m_Elements) {
      if (SE->GetKind() != DXIL::SemanticKind::Target)
        continue;
      unsigned row = SE->GetSemanticStartIndex();
      SE->SetStartRow(row);
      SE->SetStartCol(0);
      DXASSERT(SE->GetRows() == 1, "otherwise, SV_Target output not broken into separate rows earlier");
      row += SE->GetRows();
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

} // namespace hlsl
