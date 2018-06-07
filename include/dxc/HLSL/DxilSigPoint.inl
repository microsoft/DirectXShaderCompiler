///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSigPoint.inl                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

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
//  Semantic            VSIn,           VSOut,  PCIn,            HSIn,            HSCPIn, HSCPOut, PCOut,         DSIn,            DSCPIn, DSOut,  GSVIn,  GSIn,            GSOut,  PSIn,             PSOut,            CSIn
#define DO_INTERPRETATION_TABLE(D) \
  {/*Arbitrary*/        D(Arb),         D(Arb), D(NA),           D(NA),           D(Arb), D(Arb),  D(Arb),        D(Arb),          D(Arb), D(Arb), D(Arb), D(NA),           D(Arb), D(Arb),           D(NA),            D(NA)}, \
  {/*VertexID*/         D(SV),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NA),            D(NA)}, \
  {/*InstanceID*/       D(SV),          D(Arb), D(NA),           D(NA),           D(Arb), D(Arb),  D(NA),         D(NA),           D(Arb), D(Arb), D(Arb), D(NA),           D(Arb), D(Arb),           D(NA),            D(NA)}, \
  {/*Position*/         D(Arb),         D(SV),  D(NA),           D(NA),           D(SV),  D(SV),   D(Arb),        D(Arb),          D(SV),  D(SV),  D(SV),  D(NA),           D(SV),  D(SV),            D(NA),            D(NA)}, \
  {/*RenderTgArrayIdx*/ D(Arb),         D(SV),  D(NA),           D(NA),           D(SV),  D(SV),   D(Arb),        D(Arb),          D(SV),  D(SV),  D(SV),  D(NA),           D(SV),  D(SV),            D(NA),            D(NA)}, \
  {/*ViewPortArrayIdx*/ D(Arb),         D(SV),  D(NA),           D(NA),           D(SV),  D(SV),   D(Arb),        D(Arb),          D(SV),  D(SV),  D(SV),  D(NA),           D(SV),  D(SV),            D(NA),            D(NA)}, \
  {/*ClipDistance*/     D(Arb),         D(SV),  D(NA),           D(NA),           D(SV),  D(SV),   D(Arb),        D(Arb),          D(SV),  D(SV),  D(SV),  D(NA),           D(SV),  D(SV),            D(NA),            D(NA)}, \
  {/*CullDistance*/     D(Arb),         D(SV),  D(NA),           D(NA),           D(SV),  D(SV),   D(Arb),        D(Arb),          D(SV),  D(SV),  D(SV),  D(NA),           D(SV),  D(SV),            D(NA),            D(NA)}, \
  {/*OutputControlPtID*/D(NA),          D(NA),  D(NA),           D(NotInSig),     D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NA),            D(NA)}, \
  {/*DomainLocation*/   D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NotInSig),     D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NA),            D(NA)}, \
  {/*PrimitiveID*/      D(NA),          D(NA),  D(NotInSig),     D(NotInSig),     D(NA),  D(NA),   D(NA),         D(NotInSig),     D(NA),  D(NA),  D(NA),  D(Shadow),       D(SGV), D(SGV),           D(NA),            D(NA)}, \
  {/*GSInstanceID*/     D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NotInSig),     D(NA),  D(NA),            D(NA),            D(NA)}, \
  {/*SampleIndex*/      D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(Shadow _41),    D(NA),            D(NA)}, \
  {/*IsFrontFace*/      D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(SGV), D(SGV),           D(NA),            D(NA)}, \
  {/*Coverage*/         D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NotInSig _50),  D(NotPacked _41), D(NA)}, \
  {/*InnerCoverage*/    D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NotInSig _50),  D(NA),            D(NA)}, \
  {/*Target*/           D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(Target),        D(NA)}, \
  {/*Depth*/            D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NotPacked),     D(NA)}, \
  {/*DepthLessEqual*/   D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NotPacked _50), D(NA)}, \
  {/*DepthGreaterEqual*/D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NotPacked _50), D(NA)}, \
  {/*StencilRef*/       D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NotPacked _50), D(NA)}, \
  {/*DispatchThreadID*/ D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NA),            D(NotInSig)}, \
  {/*GroupID*/          D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NA),            D(NotInSig)}, \
  {/*GroupIndex*/       D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NA),            D(NotInSig)}, \
  {/*GroupThreadID*/    D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NA),            D(NotInSig)}, \
  {/*TessFactor*/       D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(TessFactor), D(TessFactor),   D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NA),            D(NA)}, \
  {/*InsideTessFactor*/ D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(TessFactor), D(TessFactor),   D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NA),            D(NA),            D(NA)}, \
  {/*ViewID*/           D(NotInSig _61),D(NA),  D(NotInSig _61), D(NotInSig _61), D(NA),  D(NA),   D(NA),         D(NotInSig _61), D(NA),  D(NA),  D(NA),  D(NotInSig _61), D(NA),  D(NotInSig _61),  D(NA),            D(NA)}, \
  {/*Barycentrics*/     D(NA),          D(NA),  D(NA),           D(NA),           D(NA),  D(NA),   D(NA),         D(NA),           D(NA),  D(NA),  D(NA),  D(NA),           D(NA),  D(NotPacked _61), D(NA),            D(NA)}, \
// INTERPRETATION-TABLE:END

const VersionedSemanticInterpretation SigPoint::ms_SemanticInterpretationTable[(unsigned)DXIL::SemanticKind::Invalid][(unsigned)SigPoint::Kind::Invalid] = {
#define _41 ,4,1
#define _50 ,5,0
#define _61 ,6,1
#define DO(k) VersionedSemanticInterpretation(DXIL::SemanticInterpretationKind::k)
  DO_INTERPRETATION_TABLE(DO)
#undef DO
};

// -----------------------
// SigPoint Implementation

SigPoint::SigPoint(DXIL::SigPointKind spk, const char *name, DXIL::SigPointKind rspk, DXIL::ShaderKind shk, DXIL::SignatureKind sigk, DXIL::PackingKind pk) :
  m_Kind(spk), m_RelatedKind(rspk), m_ShaderKind(shk), m_SignatureKind(sigk), m_pszName(name), m_PackingKind(pk)
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
    default:
      break;
    }
  }

  switch (shaderKind) {
  case DXIL::ShaderKind::Vertex:
    switch (sigKind) {
    case DXIL::SignatureKind::Input: return DXIL::SigPointKind::VSIn;
    case DXIL::SignatureKind::Output: return DXIL::SigPointKind::VSOut;
    default:
      break;
    }
    break;
  case DXIL::ShaderKind::Hull:
    switch (sigKind) {
    case DXIL::SignatureKind::Input: return DXIL::SigPointKind::HSCPIn;
    case DXIL::SignatureKind::Output: return DXIL::SigPointKind::HSCPOut;
    case DXIL::SignatureKind::PatchConstant: return DXIL::SigPointKind::PCOut;
    default:
      break;
    }
    break;
  case DXIL::ShaderKind::Domain:
    switch (sigKind) {
    case DXIL::SignatureKind::Input: return DXIL::SigPointKind::DSCPIn;
    case DXIL::SignatureKind::Output: return DXIL::SigPointKind::DSOut;
    case DXIL::SignatureKind::PatchConstant: return DXIL::SigPointKind::DSIn;
    default:
      break;
    }
    break;
  case DXIL::ShaderKind::Geometry:
    switch (sigKind) {
    case DXIL::SignatureKind::Input: return DXIL::SigPointKind::GSVIn;
    case DXIL::SignatureKind::Output: return DXIL::SigPointKind::GSOut;
    default:
      break;
    }
    break;
  case DXIL::ShaderKind::Pixel:
    switch (sigKind) {
    case DXIL::SignatureKind::Input: return DXIL::SigPointKind::PSIn;
    case DXIL::SignatureKind::Output: return DXIL::SigPointKind::PSOut;
    default:
      break;
    }
    break;
  case DXIL::ShaderKind::Compute:
    switch (sigKind) {
    case DXIL::SignatureKind::Input: return DXIL::SigPointKind::CSIn;
    default:
      break;
    }
    break;
  default:
    break;
  }

  return DXIL::SigPointKind::Invalid;
}

} // namespace hlsl
