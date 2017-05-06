///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSigPoint.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilSigPoint.h"
#include "dxc/Support/Global.h"

#include <string>

using std::string;

/* <py>
import hctdb_instrhelp
</py> */

namespace hlsl {

// Related points to a SigPoint that would contain the signature element for a "Shadow" element.
// A "Shadow" element isn't actually accessed through that signature's Load/Store Input/Output.
// Instead, it uses a dedicated intrinsic, but still requires that an entry exist in the signature
// for compatibility purposes.
// <py::lines('SIGPOINT-TABLE')>hctdb_instrhelp.get_sigpoint_table()</py>
// SIGPOINT-TABLE:BEGIN
//   SigPoint, Related, ShaderKind, PackingKind,    SignatureKind
#define DO_SIGPOINTS(DO) \
  DO(VSIn,     Invalid, Vertex,     InputAssembler, Input) \
  DO(VSOut,    Invalid, Vertex,     Vertex,         Output) \
  DO(PCIn,     HSCPIn,  Hull,       None,           Invalid) \
  DO(HSIn,     HSCPIn,  Hull,       None,           Invalid) \
  DO(HSCPIn,   Invalid, Hull,       Vertex,         Input) \
  DO(HSCPOut,  Invalid, Hull,       Vertex,         Output) \
  DO(PCOut,    Invalid, Hull,       PatchConstant,  PatchConstant) \
  DO(DSIn,     Invalid, Domain,     PatchConstant,  PatchConstant) \
  DO(DSCPIn,   Invalid, Domain,     Vertex,         Input) \
  DO(DSOut,    Invalid, Domain,     Vertex,         Output) \
  DO(GSVIn,    Invalid, Geometry,   Vertex,         Input) \
  DO(GSIn,     GSVIn,   Geometry,   None,           Invalid) \
  DO(GSOut,    Invalid, Geometry,   Vertex,         Output) \
  DO(PSIn,     Invalid, Pixel,      Vertex,         Input) \
  DO(PSOut,    Invalid, Pixel,      Target,         Output) \
  DO(CSIn,     Invalid, Compute,    None,           Invalid) \
  DO(Invalid,  Invalid, Invalid,    Invalid,        Invalid)
// SIGPOINT-TABLE:END

const SigPoint SigPoint::ms_SigPoints[kNumSigPointRecords] = {
#define DEF_SIGPOINT(spk, rspk, shk, pk, sigk) \
  SigPoint(DXIL::SigPointKind::spk, #spk, DXIL::SigPointKind::rspk, DXIL::ShaderKind::shk, DXIL::SignatureKind::sigk, DXIL::PackingKind::pk),
  DO_SIGPOINTS(DEF_SIGPOINT)
#undef DEF_SIGPOINT
};

// <py::lines('INTERPRETATION-TABLE')>hctdb_instrhelp.get_interpretation_table()</py>
// INTERPRETATION-TABLE:BEGIN
//   Semantic,               VSIn,         VSOut, PCIn,         HSIn,         HSCPIn, HSCPOut, PCOut,      DSIn,         DSCPIn, DSOut, GSVIn, GSIn,         GSOut, PSIn,          PSOut,         CSIn
#define DO_INTERPRETATION_TABLE(DO) \
  DO(Arbitrary,              Arb,          Arb,   NA,           NA,           Arb,    Arb,     Arb,        Arb,          Arb,    Arb,   Arb,   NA,           Arb,   Arb,           NA,            NA) \
  DO(VertexID,               SV,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NA,            NA,            NA) \
  DO(InstanceID,             SV,           Arb,   NA,           NA,           Arb,    Arb,     NA,         NA,           Arb,    Arb,   Arb,   NA,           Arb,   Arb,           NA,            NA) \
  DO(Position,               Arb,          SV,    NA,           NA,           SV,     SV,      Arb,        Arb,          SV,     SV,    SV,    NA,           SV,    SV,            NA,            NA) \
  DO(RenderTargetArrayIndex, Arb,          SV,    NA,           NA,           SV,     SV,      Arb,        Arb,          SV,     SV,    SV,    NA,           SV,    SV,            NA,            NA) \
  DO(ViewPortArrayIndex,     Arb,          SV,    NA,           NA,           SV,     SV,      Arb,        Arb,          SV,     SV,    SV,    NA,           SV,    SV,            NA,            NA) \
  DO(ClipDistance,           Arb,          SV,    NA,           NA,           SV,     SV,      Arb,        Arb,          SV,     SV,    SV,    NA,           SV,    SV,            NA,            NA) \
  DO(CullDistance,           Arb,          SV,    NA,           NA,           SV,     SV,      Arb,        Arb,          SV,     SV,    SV,    NA,           SV,    SV,            NA,            NA) \
  DO(OutputControlPointID,   NA,           NA,    NA,           NotInSig,     NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NA,            NA,            NA) \
  DO(DomainLocation,         NA,           NA,    NA,           NA,           NA,     NA,      NA,         NotInSig,     NA,     NA,    NA,    NA,           NA,    NA,            NA,            NA) \
  DO(PrimitiveID,            NA,           NA,    NotInSig,     NotInSig,     NA,     NA,      NA,         NotInSig,     NA,     NA,    NA,    Shadow,       SGV,   SGV,           NA,            NA) \
  DO(GSInstanceID,           NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NotInSig,     NA,    NA,            NA,            NA) \
  DO(SampleIndex,            NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    Shadow _41,    NA,            NA) \
  DO(IsFrontFace,            NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           SGV,   SGV,           NA,            NA) \
  DO(Coverage,               NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NotInSig _50,  NotPacked _41, NA) \
  DO(InnerCoverage,          NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NotInSig _50,  NA,            NA) \
  DO(Target,                 NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NA,            Target,        NA) \
  DO(Depth,                  NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NA,            NotPacked,     NA) \
  DO(DepthLessEqual,         NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NA,            NotPacked _50, NA) \
  DO(DepthGreaterEqual,      NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NA,            NotPacked _50, NA) \
  DO(StencilRef,             NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NA,            NotPacked _50, NA) \
  DO(DispatchThreadID,       NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NA,            NA,            NotInSig) \
  DO(GroupID,                NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NA,            NA,            NotInSig) \
  DO(GroupIndex,             NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NA,            NA,            NotInSig) \
  DO(GroupThreadID,          NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NA,            NA,            NotInSig) \
  DO(TessFactor,             NA,           NA,    NA,           NA,           NA,     NA,      TessFactor, TessFactor,   NA,     NA,    NA,    NA,           NA,    NA,            NA,            NA) \
  DO(InsideTessFactor,       NA,           NA,    NA,           NA,           NA,     NA,      TessFactor, TessFactor,   NA,     NA,    NA,    NA,           NA,    NA,            NA,            NA) \
  DO(ViewID,                 NotInSig _61, NA,    NotInSig _61, NotInSig _61, NA,     NA,      NA,         NotInSig _61, NA,     NA,    NA,    NotInSig _61, NA,    NotInSig _61,  NA,            NA) \
  DO(Barycentrics,           NA,           NA,    NA,           NA,           NA,     NA,      NA,         NA,           NA,     NA,    NA,    NA,           NA,    NotPacked _61, NA,            NA)
// INTERPRETATION-TABLE:END

const VersionedSemanticInterpretation SigPoint::ms_SemanticInterpretationTable[(unsigned)DXIL::SemanticKind::Invalid][(unsigned)SigPoint::Kind::Invalid] = {
#define _41 ,4,1
#define _50 ,5,0
#define _61 ,6,1
#define DO(k) VersionedSemanticInterpretation(DXIL::SemanticInterpretationKind::k)
#define DO_ROW(SEM, VSIn, VSOut, PCIn, HSIn, HSCPIn, HSCPOut, PCOut, DSIn, DSCPIn, DSOut, GSVIn, GSIn, GSOut, PSIn, PSOut, CSIn) \
  { DO(VSIn), DO(VSOut), DO(PCIn), DO(HSIn), DO(HSCPIn), DO(HSCPOut), DO(PCOut), DO(DSIn), DO(DSCPIn), DO(DSOut), DO(GSVIn), DO(GSIn), DO(GSOut), DO(PSIn), DO(PSOut), DO(CSIn) },
  DO_INTERPRETATION_TABLE(DO_ROW)
#undef DO_ROW
#undef DO
};

// -----------------------
// SigPoint Implementation

SigPoint::SigPoint(DXIL::SigPointKind spk, const char *name, DXIL::SigPointKind rspk, DXIL::ShaderKind shk, DXIL::SignatureKind sigk, DXIL::PackingKind pk) :
  m_Kind(spk), m_pszName(name), m_RelatedKind(rspk), m_ShaderKind(shk), m_SignatureKind(sigk), m_PackingKind(pk)
{}

DXIL::SignatureKind SigPoint::GetSignatureKindWithFallback() const {
  DXIL::SignatureKind sigKind = GetSignatureKind();
  if (sigKind == DXIL::SignatureKind::Invalid) {
    DXIL::SigPointKind RK = GetRelatedKind();
    if (RK != DXIL::SigPointKind::Invalid)
      sigKind = SigPoint::GetSigPoint(RK)->GetSignatureKind();
  }
  return sigKind;
}

DXIL::SemanticInterpretationKind SigPoint::GetInterpretation(DXIL::SemanticKind SK, Kind K, unsigned MajorVersion, unsigned MinorVersion) {
  if (SK < DXIL::SemanticKind::Invalid && K < Kind::Invalid) {
    const VersionedSemanticInterpretation& VSI = ms_SemanticInterpretationTable[(unsigned)SK][(unsigned)K];
    if (VSI.Kind != DXIL::SemanticInterpretationKind::NA) {
      if (MajorVersion > (unsigned)VSI.Major ||
          (MajorVersion == (unsigned)VSI.Major && MinorVersion >= (unsigned)VSI.Minor))
        return VSI.Kind;
    }
  }
  return DXIL::SemanticInterpretationKind::NA;
}

SigPoint::Kind SigPoint::RecoverKind(DXIL::SemanticKind SK, Kind K) {
  if (SK == DXIL::SemanticKind::PrimitiveID && K == Kind::GSVIn)
    return Kind::GSIn;
  return K;
}

// --------------
// Static methods

const SigPoint* SigPoint::GetSigPoint(Kind K) {
  return ((unsigned)K < kNumSigPointRecords) ? &ms_SigPoints[(unsigned)K] : &ms_SigPoints[(unsigned)Kind::Invalid];
}

DXIL::SigPointKind SigPoint::GetKind(DXIL::ShaderKind shaderKind, DXIL::SignatureKind sigKind, bool isPatchConstantFunction, bool isSpecialInput) {
  if (isSpecialInput) {
    switch (shaderKind) {
    case DXIL::ShaderKind::Hull:
    if (sigKind == DXIL::SignatureKind::Input)
      return isPatchConstantFunction ? DXIL::SigPointKind::PCIn : DXIL::SigPointKind::HSIn;
    case DXIL::ShaderKind::Geometry:
      if (sigKind == DXIL::SignatureKind::Input)
        return DXIL::SigPointKind::GSIn;
    }
  }

  switch (shaderKind) {
  case DXIL::ShaderKind::Vertex:
    switch (sigKind) {
    case DXIL::SignatureKind::Input: return DXIL::SigPointKind::VSIn;
    case DXIL::SignatureKind::Output: return DXIL::SigPointKind::VSOut;
    }
    break;
  case DXIL::ShaderKind::Hull:
    switch (sigKind) {
    case DXIL::SignatureKind::Input: return DXIL::SigPointKind::HSCPIn;
    case DXIL::SignatureKind::Output: return DXIL::SigPointKind::HSCPOut;
    case DXIL::SignatureKind::PatchConstant: return DXIL::SigPointKind::PCOut;
    }
    break;
  case DXIL::ShaderKind::Domain:
    switch (sigKind) {
    case DXIL::SignatureKind::Input: return DXIL::SigPointKind::DSCPIn;
    case DXIL::SignatureKind::Output: return DXIL::SigPointKind::DSOut;
    case DXIL::SignatureKind::PatchConstant: return DXIL::SigPointKind::DSIn;
    }
    break;
  case DXIL::ShaderKind::Geometry:
    switch (sigKind) {
    case DXIL::SignatureKind::Input: return DXIL::SigPointKind::GSVIn;
    case DXIL::SignatureKind::Output: return DXIL::SigPointKind::GSOut;
    }
    break;
  case DXIL::ShaderKind::Pixel:
    switch (sigKind) {
    case DXIL::SignatureKind::Input: return DXIL::SigPointKind::PSIn;
    case DXIL::SignatureKind::Output: return DXIL::SigPointKind::PSOut;
    }
    break;
  case DXIL::ShaderKind::Compute:
    switch (sigKind) {
    case DXIL::SignatureKind::Input: return DXIL::SigPointKind::CSIn;
    }
    break;
  }

  return DXIL::SigPointKind::Invalid;
}

} // namespace hlsl
