///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilValidation.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// This file provides support for validating DXIL shaders.                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilValidation.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilUtil.h"
#include "dxc/HLSL/DxilInstructions.h"
#include "dxc/HLSL/ReducibilityAnalysis.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/FileIOHelper.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include <unordered_set>
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "dxc/HLSL/DxilSpanAllocator.h"
#include "dxc/HLSL/DxilSignatureAllocator.h"
#include "dxc/HLSL/DxilRootSignature.h"
#include <algorithm>


using namespace llvm;
using namespace std;

///////////////////////////////////////////////////////////////////////////////
// Error messages.

const char *hlsl::GetValidationRuleText(ValidationRule value) {
  /* <py::lines('VALRULE-TEXT')>hctdb_instrhelp.get_valrule_text()</py>*/
  // VALRULE-TEXT:BEGIN
  switch(value) {
    case hlsl::ValidationRule::BitcodeValid: return "Module bitcode is invalid";
    case hlsl::ValidationRule::ContainerPartMatches: return "Container part '%0' does not match expected for module.";
    case hlsl::ValidationRule::ContainerPartRepeated: return "More than one container part '%0'.";
    case hlsl::ValidationRule::ContainerPartMissing: return "Missing part '%0' required by module.";
    case hlsl::ValidationRule::ContainerPartInvalid: return "Unknown part '%0' found in DXIL container.";
    case hlsl::ValidationRule::ContainerRootSignatureIncompatible: return "Root Signature in DXIL container is not compatible with shader.";
    case hlsl::ValidationRule::MetaRequired: return "TODO - Required metadata missing";
    case hlsl::ValidationRule::MetaKnown: return "Named metadata '%0' is unknown";
    case hlsl::ValidationRule::MetaUsed: return "All metadata must be used by dxil";
    case hlsl::ValidationRule::MetaTarget: return "Unknown target triple '%0'";
    case hlsl::ValidationRule::MetaWellFormed: return "TODO - Metadata must be well-formed in operand count and types";
    case hlsl::ValidationRule::MetaSemanticLen: return "Semantic length must be at least 1 and at most 64";
    case hlsl::ValidationRule::MetaInterpModeValid: return "Invalid interpolation mode for '%0'";
    case hlsl::ValidationRule::MetaSemaKindValid: return "Semantic kind for '%0' is invalid";
    case hlsl::ValidationRule::MetaNoSemanticOverlap: return "Semantic '%0' overlap at %1";
    case hlsl::ValidationRule::MetaSemaKindMatchesName: return "Semantic name %0 does not match System Value kind %1";
    case hlsl::ValidationRule::MetaDuplicateSysValue: return "System value %0 appears more than once in the same signature.";
    case hlsl::ValidationRule::MetaSemanticIndexMax: return "%0 semantic index exceeds maximum (%1)";
    case hlsl::ValidationRule::MetaSystemValueRows: return "rows for system value semantic %0 must be 1";
    case hlsl::ValidationRule::MetaSemanticShouldBeAllocated: return "%0 Semantic '%1' should have a valid packing location";
    case hlsl::ValidationRule::MetaSemanticShouldNotBeAllocated: return "%0 Semantic '%1' should have a packing location of -1";
    case hlsl::ValidationRule::MetaValueRange: return "Metadata value must be within range";
    case hlsl::ValidationRule::MetaFlagsUsage: return "Flags must match usage";
    case hlsl::ValidationRule::MetaDenseResIDs: return "Resource identifiers must be zero-based and dense";
    case hlsl::ValidationRule::MetaSignatureOverlap: return "signature element %0 at location (%1,%2) size (%3,%4) overlaps another signature element.";
    case hlsl::ValidationRule::MetaSignatureOutOfRange: return "signature element %0 at location (%1,%2) size (%3,%4) is out of range.";
    case hlsl::ValidationRule::MetaSignatureIndexConflict: return "signature element %0 at location (%1,%2) size (%3,%4) has an indexing conflict with another signature element packed into the same row.";
    case hlsl::ValidationRule::MetaSignatureIllegalComponentOrder: return "signature element %0 at location (%1,%2) size (%3,%4) violates component ordering rule (arb < sv < sgv).";
    case hlsl::ValidationRule::MetaSignatureDataWidth: return "signature element %0 at location (%1, %2) size (%3, %4) has data width that differs from another element packed into the same row.";
    case hlsl::ValidationRule::MetaIntegerInterpMode: return "signature element %0 specifies invalid interpolation mode for integer component type.";
    case hlsl::ValidationRule::MetaInterpModeInOneRow: return "signature element %0 at location (%1,%2) size (%3,%4) has interpolation mode that differs from another element packed into the same row.";
    case hlsl::ValidationRule::MetaSemanticCompType: return "%0 must be %1";
    case hlsl::ValidationRule::MetaClipCullMaxRows: return "ClipDistance and CullDistance occupy more than the maximum of 2 rows combined.";
    case hlsl::ValidationRule::MetaClipCullMaxComponents: return "ClipDistance and CullDistance use more than the maximum of 8 components combined.";
    case hlsl::ValidationRule::MetaSignatureCompType: return "signature %0 specifies unrecognized or invalid component type";
    case hlsl::ValidationRule::MetaTessellatorPartition: return "Invalid Tessellator Partitioning specified. Must be integer, pow2, fractional_odd or fractional_even.";
    case hlsl::ValidationRule::MetaTessellatorOutputPrimitive: return "Invalid Tessellator Output Primitive specified. Must be point, line, triangleCW or triangleCCW.";
    case hlsl::ValidationRule::MetaMaxTessFactor: return "Hull Shader MaxTessFactor must be [%0..%1].  %2 specified";
    case hlsl::ValidationRule::MetaValidSamplerMode: return "Invalid sampler mode on sampler ";
    case hlsl::ValidationRule::MetaFunctionAnnotation: return "Cannot find function annotation for %0";
    case hlsl::ValidationRule::MetaGlcNotOnAppendConsume: return "globallycoherent cannot be used with append/consume buffers";
    case hlsl::ValidationRule::MetaStructBufAlignment: return "structured buffer element size must be a multiple of %0 bytes (actual size %1 bytes)";
    case hlsl::ValidationRule::MetaStructBufAlignmentOutOfBound: return "structured buffer elements cannot be larger than %0 bytes (actual size %1 bytes)";
    case hlsl::ValidationRule::MetaEntryFunction: return "entrypoint not found";
    case hlsl::ValidationRule::MetaInvalidControlFlowHint: return "Invalid control flow hint";
    case hlsl::ValidationRule::MetaBranchFlatten: return "Can't use branch and flatten attributes together";
    case hlsl::ValidationRule::MetaForceCaseOnSwitch: return "Attribute forcecase only works for switch";
    case hlsl::ValidationRule::MetaControlFlowHintNotOnControlFlow: return "Control flow hint only works on control flow inst";
    case hlsl::ValidationRule::MetaTextureType: return "elements of typed buffers and textures must fit in four 32-bit quantities";
    case hlsl::ValidationRule::MetaBarycentricsInterpolation: return "SV_Barycentrics cannot be used with 'nointerpolation' type";
    case hlsl::ValidationRule::MetaBarycentricsFloat3: return "only 'float3' type is allowed for SV_Barycentrics.";
    case hlsl::ValidationRule::MetaBarycentricsTwoPerspectives: return "There can only be up to two input attributes of SV_Barycentrics with different perspective interpolation mode.";
    case hlsl::ValidationRule::InstrOload: return "DXIL intrinsic overload must be valid";
    case hlsl::ValidationRule::InstrCallOload: return "Call to DXIL intrinsic '%0' does not match an allowed overload signature";
    case hlsl::ValidationRule::InstrPtrBitCast: return "Pointer type bitcast must be have same size";
    case hlsl::ValidationRule::InstrMinPrecisonBitCast: return "Bitcast on minprecison types is not allowed";
    case hlsl::ValidationRule::InstrStructBitCast: return "Bitcast on struct types is not allowed";
    case hlsl::ValidationRule::InstrStatus: return "Resource status should only used by CheckAccessFullyMapped";
    case hlsl::ValidationRule::InstrCheckAccessFullyMapped: return "CheckAccessFullyMapped should only used on resource status";
    case hlsl::ValidationRule::InstrOpConst: return "%0 of %1 must be an immediate constant";
    case hlsl::ValidationRule::InstrAllowed: return "Instructions must be of an allowed type";
    case hlsl::ValidationRule::InstrOpCodeReserved: return "Instructions must not reference reserved opcodes";
    case hlsl::ValidationRule::InstrOperandRange: return "expect %0 between %1, got %2";
    case hlsl::ValidationRule::InstrNoReadingUninitialized: return "Instructions should not read uninitialized value";
    case hlsl::ValidationRule::InstrNoGenericPtrAddrSpaceCast: return "Address space cast between pointer types must have one part to be generic address space";
    case hlsl::ValidationRule::InstrInBoundsAccess: return "Access to out-of-bounds memory is disallowed";
    case hlsl::ValidationRule::InstrOpConstRange: return "Constant values must be in-range for operation";
    case hlsl::ValidationRule::InstrImmBiasForSampleB: return "bias amount for sample_b must be in the range [%0,%1], but %2 was specified as an immediate";
    case hlsl::ValidationRule::InstrNoIndefiniteLog: return "No indefinite logarithm";
    case hlsl::ValidationRule::InstrNoIndefiniteAsin: return "No indefinite arcsine";
    case hlsl::ValidationRule::InstrNoIndefiniteAcos: return "No indefinite arccosine";
    case hlsl::ValidationRule::InstrNoIDivByZero: return "No signed integer division by zero";
    case hlsl::ValidationRule::InstrNoUDivByZero: return "No unsigned integer division by zero";
    case hlsl::ValidationRule::InstrNoIndefiniteDsxy: return "No indefinite derivative calculation";
    case hlsl::ValidationRule::InstrMinPrecisionNotPrecise: return "Instructions marked precise may not refer to minprecision values";
    case hlsl::ValidationRule::InstrOnlyOneAllocConsume: return "RWStructuredBuffers may increment or decrement their counters, but not both.";
    case hlsl::ValidationRule::InstrTextureOffset: return "offset texture instructions must take offset which can resolve to integer literal in the range -8 to 7";
    case hlsl::ValidationRule::InstrCannotPullPosition: return "%0 does not support pull-model evaluation of position";
    case hlsl::ValidationRule::InstrEvalInterpolationMode: return "Interpolation mode on %0 used with eval_* instruction must be linear, linear_centroid, linear_noperspective, linear_noperspective_centroid, linear_sample or linear_noperspective_sample";
    case hlsl::ValidationRule::InstrResourceCoordinateMiss: return "coord uninitialized";
    case hlsl::ValidationRule::InstrResourceCoordinateTooMany: return "out of bound coord must be undef";
    case hlsl::ValidationRule::InstrResourceOffsetMiss: return "offset uninitialized";
    case hlsl::ValidationRule::InstrResourceOffsetTooMany: return "out of bound offset must be undef";
    case hlsl::ValidationRule::InstrUndefResultForGetDimension: return "GetDimensions used undef dimension %0 on %1";
    case hlsl::ValidationRule::InstrSamplerModeForLOD: return "lod instruction requires sampler declared in default mode";
    case hlsl::ValidationRule::InstrSamplerModeForSample: return "sample/_l/_d/_cl_s/gather instruction requires sampler declared in default mode";
    case hlsl::ValidationRule::InstrSamplerModeForSampleC: return "sample_c_*/gather_c instructions require sampler declared in comparison mode";
    case hlsl::ValidationRule::InstrSampleCompType: return "sample_* instructions require resource to be declared to return UNORM, SNORM or FLOAT.";
    case hlsl::ValidationRule::InstrBarrierModeUselessUGroup: return "sync can't specify both _ugroup and _uglobal. If both are needed, just specify _uglobal.";
    case hlsl::ValidationRule::InstrBarrierModeNoMemory: return "sync must include some form of memory barrier - _u (UAV) and/or _g (Thread Group Shared Memory).  Only _t (thread group sync) is optional. ";
    case hlsl::ValidationRule::InstrBarrierModeForNonCS: return "sync in a non-Compute Shader must only sync UAV (sync_uglobal)";
    case hlsl::ValidationRule::InstrWriteMaskForTypedUAVStore: return "store on typed uav must write to all four components of the UAV";
    case hlsl::ValidationRule::InstrResourceKindForCalcLOD: return "lod requires resource declared as texture1D/2D/3D/Cube/CubeArray/1DArray/2DArray";
    case hlsl::ValidationRule::InstrResourceKindForSample: return "sample/_l/_d requires resource declared as texture1D/2D/3D/Cube/1DArray/2DArray/CubeArray";
    case hlsl::ValidationRule::InstrResourceKindForSampleC: return "samplec requires resource declared as texture1D/2D/Cube/1DArray/2DArray/CubeArray";
    case hlsl::ValidationRule::InstrResourceKindForGather: return "gather requires resource declared as texture/2D/Cube/2DArray/CubeArray";
    case hlsl::ValidationRule::InstrWriteMaskMatchValueForUAVStore: return "uav store write mask must match store value mask, write mask is %0 and store value mask is %1";
    case hlsl::ValidationRule::InstrResourceKindForBufferLoadStore: return "buffer load/store only works on Raw/Typed/StructuredBuffer";
    case hlsl::ValidationRule::InstrResourceKindForTextureStore: return "texture store only works on Texture1D/1DArray/2D/2DArray/3D";
    case hlsl::ValidationRule::InstrResourceKindForGetDim: return "Invalid resource kind on GetDimensions";
    case hlsl::ValidationRule::InstrResourceKindForTextureLoad: return "texture load only works on Texture1D/1DArray/2D/2DArray/3D/MS2D/MS2DArray";
    case hlsl::ValidationRule::InstrResourceClassForSamplerGather: return "sample, lod and gather should on srv resource.";
    case hlsl::ValidationRule::InstrResourceClassForUAVStore: return "store should on uav resource.";
    case hlsl::ValidationRule::InstrResourceClassForLoad: return "load can only run on UAV/SRV resource";
    case hlsl::ValidationRule::InstrOffsetOnUAVLoad: return "uav load don't support offset";
    case hlsl::ValidationRule::InstrMipOnUAVLoad: return "uav load don't support mipLevel/sampleIndex";
    case hlsl::ValidationRule::InstrSampleIndexForLoad2DMS: return "load on Texture2DMS/2DMSArray require sampleIndex";
    case hlsl::ValidationRule::InstrCoordinateCountForRawTypedBuf: return "raw/typed buffer don't need 2 coordinates";
    case hlsl::ValidationRule::InstrCoordinateCountForStructBuf: return "structured buffer require 2 coordinates";
    case hlsl::ValidationRule::InstrMipLevelForGetDimension: return "Use mip level on buffer when GetDimensions";
    case hlsl::ValidationRule::InstrDxilStructUser: return "Dxil struct types should only used by ExtractValue";
    case hlsl::ValidationRule::InstrDxilStructUserOutOfBound: return "Index out of bound when extract value from dxil struct types";
    case hlsl::ValidationRule::InstrHandleNotFromCreateHandle: return "Resource handle should returned by createHandle";
    case hlsl::ValidationRule::InstrBufferUpdateCounterOnUAV: return "BufferUpdateCounter valid only on UAV";
    case hlsl::ValidationRule::InstrCBufferOutOfBound: return "Cbuffer access out of bound";
    case hlsl::ValidationRule::InstrCBufferClassForCBufferHandle: return "Expect Cbuffer for CBufferLoad handle";
    case hlsl::ValidationRule::InstrFailToResloveTGSMPointer: return "TGSM pointers must originate from an unambiguous TGSM global variable.";
    case hlsl::ValidationRule::InstrExtractValue: return "ExtractValue should only be used on dxil struct types and cmpxchg";
    case hlsl::ValidationRule::InstrTGSMRaceCond: return "Race condition writing to shared memory detected, consider making this write conditional";
    case hlsl::ValidationRule::InstrAttributeAtVertexNoInterpolation: return "Attribute %0 must have nointerpolation mode in order to use GetAttributeAtVertex function.";
    case hlsl::ValidationRule::InstrCreateHandleImmRangeID: return "Local resource must map to global resource.";
    case hlsl::ValidationRule::TypesNoVector: return "Vector type '%0' is not allowed";
    case hlsl::ValidationRule::TypesDefined: return "Type '%0' is not defined on DXIL primitives";
    case hlsl::ValidationRule::TypesIntWidth: return "Int type '%0' has an invalid width";
    case hlsl::ValidationRule::TypesNoMultiDim: return "Only one dimension allowed for array type";
    case hlsl::ValidationRule::TypesI8: return "I8 can only used as immediate value for intrinsic";
    case hlsl::ValidationRule::SmName: return "Unknown shader model '%0'";
    case hlsl::ValidationRule::SmDxilVersion: return "Shader model requires Dxil Version %0,%1";
    case hlsl::ValidationRule::SmOpcode: return "Opcode %0 not valid in shader model %1";
    case hlsl::ValidationRule::SmOperand: return "Operand must be defined in target shader model";
    case hlsl::ValidationRule::SmSemantic: return "Semantic '%0' is invalid as %1 %2";
    case hlsl::ValidationRule::SmNoInterpMode: return "Interpolation mode for '%0' is set but should be undefined";
    case hlsl::ValidationRule::SmNoPSOutputIdx: return "Pixel shader output registers are not indexable.";
    case hlsl::ValidationRule::SmPSConsistentInterp: return "Interpolation mode for PS input position must be linear_noperspective_centroid or linear_noperspective_sample when outputting oDepthGE or oDepthLE and not running at sample frequency (which is forced by inputting SV_SampleIndex or declaring an input linear_sample or linear_noperspective_sample)";
    case hlsl::ValidationRule::SmThreadGroupChannelRange: return "Declared Thread Group %0 size %1 outside valid range [%2..%3]";
    case hlsl::ValidationRule::SmMaxTheadGroup: return "Declared Thread Group Count %0 (X*Y*Z) is beyond the valid maximum of %1";
    case hlsl::ValidationRule::SmMaxTGSMSize: return "Total Thread Group Shared Memory storage is %0, exceeded %1";
    case hlsl::ValidationRule::SmROVOnlyInPS: return "RasterizerOrdered objects are only allowed in 5.0+ pixel shaders";
    case hlsl::ValidationRule::SmTessFactorForDomain: return "Required TessFactor for domain not found declared anywhere in Patch Constant data";
    case hlsl::ValidationRule::SmTessFactorSizeMatchDomain: return "TessFactor rows, columns (%0, %1) invalid for domain %2.  Expected %3 rows and 1 column.";
    case hlsl::ValidationRule::SmInsideTessFactorSizeMatchDomain: return "InsideTessFactor rows, columns (%0, %1) invalid for domain %2.  Expected %3 rows and 1 column.";
    case hlsl::ValidationRule::SmDomainLocationIdxOOB: return "DomainLocation component index out of bounds for the domain.";
    case hlsl::ValidationRule::SmHullPassThruControlPointCountMatch: return "For pass thru hull shader, input control point count must match output control point count";
    case hlsl::ValidationRule::SmOutputControlPointsTotalScalars: return "Total number of scalars across all HS output control points must not exceed ";
    case hlsl::ValidationRule::SmIsoLineOutputPrimitiveMismatch: return "Hull Shader declared with IsoLine Domain must specify output primitive point or line. Triangle_cw or triangle_ccw output are not compatible with the IsoLine Domain.";
    case hlsl::ValidationRule::SmTriOutputPrimitiveMismatch: return "Hull Shader declared with Tri Domain must specify output primitive point, triangle_cw or triangle_ccw. Line output is not compatible with the Tri domain";
    case hlsl::ValidationRule::SmValidDomain: return "Invalid Tessellator Domain specified. Must be isoline, tri or quad";
    case hlsl::ValidationRule::SmPatchConstantOnlyForHSDS: return "patch constant signature only valid in HS and DS";
    case hlsl::ValidationRule::SmStreamIndexRange: return "Stream index (%0) must between 0 and %1";
    case hlsl::ValidationRule::SmPSOutputSemantic: return "Pixel Shader allows output semantics to be SV_Target, SV_Depth, SV_DepthGreaterEqual, SV_DepthLessEqual, SV_Coverage or SV_StencilRef, %0 found";
    case hlsl::ValidationRule::SmPSMultipleDepthSemantic: return "Pixel Shader only allows one type of depth semantic to be declared";
    case hlsl::ValidationRule::SmPSTargetIndexMatchesRow: return "SV_Target semantic index must match packed row location";
    case hlsl::ValidationRule::SmPSTargetCol0: return "SV_Target packed location must start at column 0";
    case hlsl::ValidationRule::SmPSCoverageAndInnerCoverage: return "InnerCoverage and Coverage are mutually exclusive.";
    case hlsl::ValidationRule::SmGSOutputVertexCountRange: return "GS output vertex count must be [0..%0].  %1 specified";
    case hlsl::ValidationRule::SmGSInstanceCountRange: return "GS instance count must be [1..%0].  %1 specified";
    case hlsl::ValidationRule::SmDSInputControlPointCountRange: return "DS input control point count must be [0..%0].  %1 specified";
    case hlsl::ValidationRule::SmHSInputControlPointCountRange: return "HS input control point count must be [0..%0].  %1 specified";
    case hlsl::ValidationRule::SmZeroHSInputControlPointWithInput: return "When HS input control point count is 0, no input signature should exist";
    case hlsl::ValidationRule::SmOutputControlPointCountRange: return "output control point count must be [0..%0].  %1 specified";
    case hlsl::ValidationRule::SmGSValidInputPrimitive: return "GS input primitive unrecognized";
    case hlsl::ValidationRule::SmGSValidOutputPrimitiveTopology: return "GS output primitive topology unrecognized";
    case hlsl::ValidationRule::SmAppendAndConsumeOnSameUAV: return "BufferUpdateCounter inc and dec on a given UAV (%d) cannot both be in the same shader for shader model less than 5.1.";
    case hlsl::ValidationRule::SmInvalidTextureKindOnUAV: return "Texture2DMS[Array] or TextureCube[Array] resources are not supported with UAVs";
    case hlsl::ValidationRule::SmInvalidResourceKind: return "Invalid resources kind";
    case hlsl::ValidationRule::SmInvalidResourceCompType: return "Invalid resource return type";
    case hlsl::ValidationRule::SmSampleCountOnlyOn2DMS: return "Only Texture2DMS/2DMSArray could has sample count";
    case hlsl::ValidationRule::SmCounterOnlyOnStructBuf: return "BufferUpdateCounter valid only on structured buffers";
    case hlsl::ValidationRule::SmGSTotalOutputVertexDataRange: return "Declared output vertex count (%0) multiplied by the total number of declared scalar components of output data (%1) equals %2.  This value cannot be greater than %3";
    case hlsl::ValidationRule::SmMultiStreamMustBePoint: return "Multiple GS output streams are used but '%0' is not pointlist";
    case hlsl::ValidationRule::SmCompletePosition: return "Not all elements of SV_Position were written";
    case hlsl::ValidationRule::SmUndefinedOutput: return "Not all elements of output %0 were written";
    case hlsl::ValidationRule::SmCSNoReturn: return "Compute shaders can't return values, outputs must be written in writable resources (UAVs).";
    case hlsl::ValidationRule::SmCBufferTemplateTypeMustBeStruct: return "D3D12 constant/texture buffer template element can only be a struct";
    case hlsl::ValidationRule::SmResourceRangeOverlap: return "Resource %0 with base %1 size %2 overlap with other resource with base %3 size %4 in space %5";
    case hlsl::ValidationRule::SmCBufferOffsetOverlap: return "CBuffer %0 has offset overlaps at %1";
    case hlsl::ValidationRule::SmCBufferElementOverflow: return "CBuffer %0 size insufficient for element at offset %1";
    case hlsl::ValidationRule::SmOpcodeInInvalidFunction: return "opcode '%0' should only be used in '%1'";
    case hlsl::ValidationRule::SmViewIDNeedsSlot: return "Pixel shader input signature lacks available space for ViewID";
    case hlsl::ValidationRule::UniNoWaveSensitiveGradient: return "Gradient operations are not affected by wave-sensitive data or control flow.";
    case hlsl::ValidationRule::FlowReducible: return "Execution flow must be reducible";
    case hlsl::ValidationRule::FlowNoRecusion: return "Recursion is not permitted";
    case hlsl::ValidationRule::FlowDeadLoop: return "Loop must have break";
    case hlsl::ValidationRule::FlowFunctionCall: return "Function %0 with parameter is not permitted, it should be inlined";
    case hlsl::ValidationRule::DeclDxilNsReserved: return "Declaration '%0' uses a reserved prefix";
    case hlsl::ValidationRule::DeclDxilFnExtern: return "External function '%0' is not a DXIL function";
    case hlsl::ValidationRule::DeclUsedInternal: return "Internal declaration '%0' is unused";
    case hlsl::ValidationRule::DeclNotUsedExternal: return "External declaration '%0' is unused";
    case hlsl::ValidationRule::DeclUsedExternalFunction: return "External function '%0' is unused";
    case hlsl::ValidationRule::DeclFnIsCalled: return "Function '%0' is used for something other than calling";
    case hlsl::ValidationRule::DeclFnFlattenParam: return "Type '%0' is a struct type but is used as a parameter in function '%1'";
    case hlsl::ValidationRule::DeclFnAttribute: return "Function '%0' contains invalid attribute '%1' with value '%2'";
  }
  // VALRULE-TEXT:END
  llvm_unreachable("invalid value");
  return "<unknown>";
}

namespace {

// Utility class for setting and restoring the diagnostic context so we may capture errors/warnings
struct DiagRestore {
  LLVMContext &Ctx;
  void *OrigDiagContext;
  LLVMContext::DiagnosticHandlerTy OrigHandler;

  DiagRestore(llvm::LLVMContext &Ctx, void *DiagContext) : Ctx(Ctx) {
    OrigHandler = Ctx.getDiagnosticHandler();
    OrigDiagContext = Ctx.getDiagnosticContext();
    Ctx.setDiagnosticHandler(
        hlsl::PrintDiagnosticContext::PrintDiagnosticHandler, DiagContext);
  }
  ~DiagRestore() {
    Ctx.setDiagnosticHandler(OrigHandler, OrigDiagContext);
  }
};

class DxilErrorDiagnosticInfo : public DiagnosticInfo {
private:
  const char *m_message;
public:
  DxilErrorDiagnosticInfo(const char *str)
    : DiagnosticInfo(DK_FirstPluginKind, DiagnosticSeverity::DS_Error),
    m_message(str) { }

  void print(DiagnosticPrinter &DP) const override {
    DP << m_message;
  }
};

static void emitDxilDiag(const LLVMContext &Ctx, const char *str) {
  // diagnose doesn't actually mutate anything.
  LLVMContext &diagCtx = const_cast<LLVMContext &>(Ctx);
  diagCtx.diagnose(DxilErrorDiagnosticInfo(str));
}

// Printing of types.
static inline DiagnosticPrinter &operator<<(DiagnosticPrinter &OS, Type &T) {
  std::string O;
  raw_string_ostream OSS(O);
  T.print(OSS);
  OS << OSS.str();
  return OS;
}

} // anon namespace

namespace hlsl {

// PrintDiagnosticContext methods.
PrintDiagnosticContext::PrintDiagnosticContext(DiagnosticPrinter &printer)
    : m_Printer(printer), m_errorsFound(false), m_warningsFound(false) {}

bool PrintDiagnosticContext::HasErrors() const { return m_errorsFound; }
bool PrintDiagnosticContext::HasWarnings() const { return m_warningsFound; }
void PrintDiagnosticContext::Handle(const DiagnosticInfo &DI) {
  DI.print(m_Printer);
  switch (DI.getSeverity()) {
  case llvm::DiagnosticSeverity::DS_Error:
    m_errorsFound = true;
    break;
  case llvm::DiagnosticSeverity::DS_Warning:
    m_warningsFound = true;
    break;
  default:
    break;
  }
  m_Printer << "\n";
}

void PrintDiagnosticContext::PrintDiagnosticHandler(const DiagnosticInfo &DI, void *Context) {
  reinterpret_cast<hlsl::PrintDiagnosticContext *>(Context)->Handle(DI);
}

struct PSExecutionInfo {
  bool SuperSampling = false;
  DXIL::SemanticKind OutputDepthKind = DXIL::SemanticKind::Invalid;
  const InterpolationMode *PositionInterpolationMode = nullptr;
};

struct ValidationContext {
  bool Failed = false;
  Module &M;
  Module *pDebugModule;
  DxilModule &DxilMod;
  const DataLayout &DL;
  DiagnosticPrinterRawOStream &DiagPrinter;
  PSExecutionInfo PSExec;
  DebugLoc LastDebugLocEmit;
  ValidationRule LastRuleEmit;
  std::unordered_set<Function *> entryFuncCallSet;
  std::unordered_set<Function *> patchConstFuncCallSet;
  std::unordered_map<unsigned, bool> UavCounterIncMap;
  bool hasOutputPosition[DXIL::kNumOutputStreams];
  bool hasViewID;
  unsigned OutputPositionMask[DXIL::kNumOutputStreams];
  std::vector<unsigned> outputCols;
  std::vector<unsigned> patchConstCols;
  unsigned domainLocSize;
  const unsigned kDxilControlFlowHintMDKind;
  const unsigned kDxilPreciseMDKind;
  const unsigned kLLVMLoopMDKind;
  bool m_bCoverageIn, m_bInnerCoverageIn;
  unsigned m_DxilMajor, m_DxilMinor;

  ValidationContext(Module &llvmModule, Module *DebugModule,
                    DxilModule &dxilModule,
                    DiagnosticPrinterRawOStream &DiagPrn)
      : M(llvmModule), pDebugModule(DebugModule), DxilMod(dxilModule),
        DL(llvmModule.getDataLayout()), DiagPrinter(DiagPrn),
        LastRuleEmit((ValidationRule)-1), hasViewID(false),
        kDxilControlFlowHintMDKind(llvmModule.getContext().getMDKindID(
            DxilMDHelper::kDxilControlFlowHintMDName)),
        kDxilPreciseMDKind(llvmModule.getContext().getMDKindID(
            DxilMDHelper::kDxilPreciseAttributeMDName)),
        kLLVMLoopMDKind(llvmModule.getContext().getMDKindID("llvm.loop")),
        m_bCoverageIn(false), m_bInnerCoverageIn(false) {
    DxilMod.GetDxilVersion(m_DxilMajor, m_DxilMinor);
    for (unsigned i = 0; i < DXIL::kNumOutputStreams; i++) {
      hasOutputPosition[i] = false;
      OutputPositionMask[i] = 0;
    }
    outputCols.resize(DxilMod.GetOutputSignature().GetElements().size(), 0);
    patchConstCols.resize(DxilMod.GetPatchConstantSignature().GetElements().size(), 0);
  }

  // Provide direct access to the raw_ostream in DiagPrinter.
  raw_ostream &DiagStream() {
    struct DiagnosticPrinterRawOStream_Pub : public DiagnosticPrinterRawOStream {
    public:
      raw_ostream &DiagStream() { return Stream; }
    };
    DiagnosticPrinterRawOStream_Pub* p = (DiagnosticPrinterRawOStream_Pub*)&DiagPrinter;
    return p->DiagStream();
  }

  void EmitGlobalValueError(GlobalValue *GV, ValidationRule rule) {
    EmitFormatError(rule, { GV->getName().str() });
  }

  // This is the least desirable mechanism, as it has no context.
  void EmitError(ValidationRule rule) {
    DiagPrinter << GetValidationRuleText(rule) << '\n';
    Failed = true;
  }

  void FormatRuleText(std::string &ruleText, ArrayRef<StringRef> args) {
    // Consider changing const char * to StringRef
    for (unsigned i = 0; i < args.size(); i++) {
      std::string argIdx = "%" + std::to_string(i);
      StringRef pArg = args[i];
      if (pArg == "")
        pArg = "<null>";

      std::string::size_type offset = ruleText.find(argIdx);
      if (offset == std::string::npos)
        continue;

      unsigned size = argIdx.size();
      ruleText.replace(offset, size, args[i]);
    }
  }

  void EmitFormatError(ValidationRule rule, ArrayRef<StringRef> args) {
    std::string ruleText = GetValidationRuleText(rule);
    FormatRuleText(ruleText, args);
    DiagPrinter << ruleText << '\n';
    Failed = true;
  }

  void EmitMetaError(Metadata *Meta, ValidationRule rule) {
    DiagPrinter << GetValidationRuleText(rule);
    Meta->print(DiagStream(), &M);
    DiagPrinter << '\n';
    Failed = true;
  }

  void EmitResourceError(const hlsl::DxilResourceBase *Res, ValidationRule rule) {
    DiagPrinter << GetValidationRuleText(rule);
    DiagPrinter << '\'' << Res->GetGlobalName() << '\'';
    DiagPrinter << '\n';
    Failed = true;
  }

  void EmitResourceFormatError(const hlsl::DxilResourceBase *Res,
                               ValidationRule rule,
                               ArrayRef<StringRef> args) {
    std::string ruleText = GetValidationRuleText(rule);
    FormatRuleText(ruleText, args);
    DiagPrinter << ruleText;
    DiagPrinter << '\'' << Res->GetGlobalName() << '\'';
    DiagPrinter << '\n';
    Failed = true;
  }

  bool IsDebugFunctionCall(Instruction *I) {
    CallInst *CI = dyn_cast<CallInst>(I);
    return CI && CI->getCalledFunction()->getName().startswith("llvm.dbg.");
  }

  DebugLoc GetDebugLoc(Instruction *I) {
    DXASSERT_NOMSG(I);
    if (pDebugModule) {
      // Look up the matching instruction in the debug module.
      llvm::Function *Fn = I->getParent()->getParent();
      llvm::Function *DbgFn = pDebugModule->getFunction(Fn->getName());
      if (DbgFn) {
        // Linear lookup, but then again, failing validation is rare.
        inst_iterator it = inst_begin(Fn);
        inst_iterator dbg_it = inst_begin(DbgFn);
        while (IsDebugFunctionCall(&*dbg_it)) ++dbg_it;
        while (&*it != I) {
          ++it;
          ++dbg_it;
          while (IsDebugFunctionCall(&*dbg_it)) ++dbg_it;
        }
        return dbg_it->getDebugLoc();
      }
    }
    return I->getDebugLoc();
  }

  bool EmitInstrLoc(Instruction *I, ValidationRule Rule) {
    const DebugLoc &L = GetDebugLoc(I);
    if (L) {
      // Instructions that get scalarized will likely hit
      // this case. Avoid redundant diagnostic messages.
      if (Rule == LastRuleEmit && L == LastDebugLocEmit) {
        return false;
      }
      LastRuleEmit = Rule;
      LastDebugLocEmit = L;

      L.print(DiagStream());
      DiagPrinter << ' ';
      return true;
    }
    BasicBlock *BB = I->getParent();
    Function *F = BB->getParent();

    DiagPrinter << "at " << I;
    DiagPrinter << " inside block ";
    if (!BB->getName().empty()) {
      DiagPrinter << BB->getName();
    }
    else {
      unsigned idx = 0;
      for (auto i = F->getBasicBlockList().begin(),
        e = F->getBasicBlockList().end(); i != e; ++i) {
        if (BB == &(*i)) {
          break;
        }
      }
      DiagPrinter << "#" << idx;
    }
    DiagPrinter << " of function " << *F << ' ';
    return true;
  }

  void EmitInstrError(Instruction *I, ValidationRule rule) {
    if (!EmitInstrLoc(I, rule)) return;
    DiagPrinter << GetValidationRuleText(rule);
    DiagPrinter << '\n';
    Failed = true;
  }

  void EmitInstrFormatError(Instruction *I, ValidationRule rule, ArrayRef<StringRef> args) {
    if (!EmitInstrLoc(I, rule)) return;

    std::string ruleText = GetValidationRuleText(rule);
    FormatRuleText(ruleText, args);
    DiagPrinter << ruleText;
    DiagPrinter << '\n';
    Failed = true;
  }

  void EmitOperandOutOfRange(Instruction *I, StringRef name, StringRef range, StringRef v) {
    if (!EmitInstrLoc(I, ValidationRule::InstrOperandRange)) return;

    std::string ruleText = GetValidationRuleText(ValidationRule::InstrOperandRange);
    FormatRuleText(ruleText, {name, range, v});
    DiagPrinter << ruleText;
    DiagPrinter << '\n';
    Failed = true;
  }

  void EmitSignatureError(DxilSignatureElement *SE, ValidationRule rule) {
    EmitFormatError(rule, { SE->GetName() });
  }

  void EmitTypeError(Type *Ty, ValidationRule rule) {
    std::string O;
    raw_string_ostream OSS(O);
    Ty->print(OSS);
    EmitFormatError(rule, { OSS.str() });
  }

  void EmitFnAttributeError(Function *F, StringRef Kind, StringRef Value) {
    EmitFormatError(ValidationRule::DeclFnAttribute, { F->getName(), Kind, Value });
  }
};

static bool ValidateOpcodeInProfile(DXIL::OpCode opcode,
                                    const ShaderModel *pSM) {
  unsigned op = (unsigned)opcode;
  /* <py::lines('VALOPCODESM-TEXT')>hctdb_instrhelp.get_valopcode_sm_text()</py>*/
  // VALOPCODESM-TEXT:BEGIN
  // Instructions: ThreadId=93, GroupId=94, ThreadIdInGroup=95,
  // FlattenedThreadIdInGroup=96
  if (93 <= op && op <= 96)
    return (pSM->IsCS());
  // Instructions: DomainLocation=105
  if (op == 105)
    return (pSM->IsDS());
  // Instructions: LoadOutputControlPoint=103, LoadPatchConstant=104
  if (103 <= op && op <= 104)
    return (pSM->IsDS() || pSM->IsHS());
  // Instructions: EmitStream=97, CutStream=98, EmitThenCutStream=99,
  // GSInstanceID=100
  if (97 <= op && op <= 100)
    return (pSM->IsGS());
  // Instructions: PrimitiveID=108
  if (op == 108)
    return (pSM->IsGS() || pSM->IsDS() || pSM->IsHS() || pSM->IsPS());
  // Instructions: StorePatchConstant=106, OutputControlPointID=107
  if (106 <= op && op <= 107)
    return (pSM->IsHS());
  // Instructions: Sample=60, SampleBias=61, SampleCmp=64,
  // RenderTargetGetSamplePosition=76, RenderTargetGetSampleCount=77,
  // CalculateLOD=81, Discard=82, DerivCoarseX=83, DerivCoarseY=84,
  // DerivFineX=85, DerivFineY=86, EvalSnapped=87, EvalSampleIndex=88,
  // EvalCentroid=89, SampleIndex=90, Coverage=91, InnerCoverage=92
  if (60 <= op && op <= 61 || op == 64 || 76 <= op && op <= 77 || 81 <= op && op <= 92)
    return (pSM->IsPS());
  // Instructions: AttributeAtVertex=137
  if (op == 137)
    return (pSM->GetMajor() > 6 || (pSM->GetMajor() == 6 && pSM->GetMinor() >= 1))
        && (pSM->IsPS());
  // Instructions: ViewID=138
  if (op == 138)
    return (pSM->GetMajor() > 6 || (pSM->GetMajor() == 6 && pSM->GetMinor() >= 1))
        && (pSM->IsVS() || pSM->IsHS() || pSM->IsDS() || pSM->IsGS() || pSM->IsPS());
  // Instructions: RawBufferLoad=139, RawBufferStore=140
  if (139 <= op && op <= 140)
    return (pSM->GetMajor() > 6 || (pSM->GetMajor() == 6 && pSM->GetMinor() >= 2));
  return true;
  // VALOPCODESM-TEXT:END
}

static unsigned ValidateSignatureRowCol(Instruction *I, DxilSignatureElement &SE,
                                    Value *rowVal, Value *colVal,
                                    ValidationContext &ValCtx) {
  if (ConstantInt *constRow = dyn_cast<ConstantInt>(rowVal)) {
    unsigned row = constRow->getLimitedValue();
    if (row >= SE.GetRows()) {
      ValCtx.EmitInstrError(I, ValidationRule::InstrOperandRange);
    }
  }

  if (!isa<ConstantInt>(colVal)) {
    // col must be const
    ValCtx.EmitInstrFormatError(I, ValidationRule::InstrOpConst,
                                {"Col", "LoadInput/StoreOutput"});
    return 0;
  }

  unsigned col = cast<ConstantInt>(colVal)->getLimitedValue();

  if (col > SE.GetCols()) {
    ValCtx.EmitInstrError(I, ValidationRule::InstrOperandRange);
  } else {
    if (SE.IsOutput())
      ValCtx.outputCols[SE.GetID()] |= 1 << col;
    if (SE.IsPatchConstant())
      ValCtx.patchConstCols[SE.GetID()] |= 1 << col;
  }

  return col;
}

static DxilSignatureElement *ValidateSignatureAccess(Instruction *I, DxilSignature &sig,
                                    Value *sigID, Value *rowVal, Value *colVal,
                                    ValidationContext &ValCtx) {
  if (!isa<ConstantInt>(sigID)) {
    // inputID must be const
    ValCtx.EmitInstrFormatError(I, ValidationRule::InstrOpConst,
                                {"SignatureID", "LoadInput/StoreOutput"});
    return nullptr;
  }

  unsigned SEIdx = cast<ConstantInt>(sigID)->getLimitedValue();
  if (sig.GetElements().size() <= SEIdx) {
    ValCtx.EmitInstrError(I, ValidationRule::InstrOpConstRange);
    return nullptr;
  }

  DxilSignatureElement &SE = sig.GetElement(SEIdx);
  bool isOutput = sig.IsOutput();

  unsigned col = ValidateSignatureRowCol(I, SE, rowVal, colVal, ValCtx);

  if (isOutput && SE.GetSemantic()->GetKind() == DXIL::SemanticKind::Position) {
    unsigned mask = ValCtx.OutputPositionMask[SE.GetOutputStream()];
    mask |= 1<<col;
    if (SE.GetOutputStream() < DXIL::kNumOutputStreams)
      ValCtx.OutputPositionMask[SE.GetOutputStream()] = mask;
  }
  return &SE;
}

static DXIL::SamplerKind GetSamplerKind(Value *samplerHandle, ValidationContext &ValCtx) {
  if (!isa<CallInst>(samplerHandle)) {
    ValCtx.EmitError(ValidationRule::InstrHandleNotFromCreateHandle);
    return DXIL::SamplerKind::Invalid;
  }

  DxilInst_CreateHandle createHandle(cast<CallInst>(samplerHandle));
  if (!createHandle) {
    ValCtx.EmitInstrError(cast<CallInst>(samplerHandle), ValidationRule::InstrHandleNotFromCreateHandle);
    return DXIL::SamplerKind::Invalid;
  }

  Value *resClass = createHandle.get_resourceClass();
  if (!isa<ConstantInt>(resClass)) {
    return DXIL::SamplerKind::Invalid;
  }

  if (createHandle.get_resourceClass_val() != static_cast<unsigned>(DXIL::ResourceClass::Sampler)) {
    // must be sampler.
    return DXIL::SamplerKind::Invalid;
  }

  Value *rangeIndex = createHandle.get_rangeId();
  if (!isa<ConstantInt>(rangeIndex)) {
    // must be constant
    return DXIL::SamplerKind::Invalid;
  }
  unsigned samplerIndex = cast<ConstantInt>(rangeIndex)->getLimitedValue();
  auto &samplers = ValCtx.DxilMod.GetSamplers();
  if (samplerIndex >= samplers.size()) {
    return DXIL::SamplerKind::Invalid;
  }

  DxilSampler *sampler = samplers[samplerIndex].get();

  Value *index = createHandle.get_index();
  ConstantInt *cIndex = dyn_cast<ConstantInt>(index);

  if (!sampler->GetGlobalSymbol()->getType()->getPointerElementType()->isArrayTy()) {
    if (!cIndex) {
      // index must be 0 for none array resource.
      return DXIL::SamplerKind::Invalid;
    }
  }

  if (cIndex) {
    unsigned index = cIndex->getLimitedValue();
    if (index < sampler->GetLowerBound() || index > sampler->GetUpperBound()) {
      // index out of range.
      return DXIL::SamplerKind::Invalid;
    }
  }

  return sampler->GetSamplerKind();
}

static DXIL::ResourceKind GetResourceKindAndCompTy(Value *handle, DXIL::ComponentType &CompTy, DXIL::ResourceClass &ResClass,
    unsigned &resIndex,
    ValidationContext &ValCtx) {
  CompTy = DXIL::ComponentType::Invalid;
  ResClass = DXIL::ResourceClass::Invalid;

  if (!isa<CallInst>(handle)) {
    ValCtx.EmitError(ValidationRule::InstrHandleNotFromCreateHandle);
    return DXIL::ResourceKind::Invalid;
  }

  DxilInst_CreateHandle createHandle(cast<CallInst>(handle));
  if (!createHandle) {
    ValCtx.EmitInstrError(cast<CallInst>(handle), ValidationRule::InstrHandleNotFromCreateHandle);
    return DXIL::ResourceKind::Invalid;
  }

  Value *resourceClass = createHandle.get_resourceClass();
  if (!isa<ConstantInt>(resourceClass)) {
    return DXIL::ResourceKind::Invalid;
  }

  ResClass = static_cast<DXIL::ResourceClass>(createHandle.get_resourceClass_val());

  switch (ResClass) {
  case DXIL::ResourceClass::SRV:
  case DXIL::ResourceClass::UAV:
    break;
  case DXIL::ResourceClass::CBuffer:
    return DXIL::ResourceKind::CBuffer;
  case DXIL::ResourceClass::Sampler:
    return DXIL::ResourceKind::Sampler;
  default:
    // Emit invalid res class
    return DXIL::ResourceKind::Invalid;
  }

  Value *rangeIndex = createHandle.get_rangeId();
  if (!isa<ConstantInt>(rangeIndex)) {
    ValCtx.EmitInstrError(cast<CallInst>(handle),
                          ValidationRule::InstrCreateHandleImmRangeID);
    // must be constant
    return DXIL::ResourceKind::Invalid;
  }
  resIndex = cast<ConstantInt>(rangeIndex)->getLimitedValue();

  DxilResource *res = nullptr;
  if (ResClass == DXIL::ResourceClass::UAV) {
    auto &resources = ValCtx.DxilMod.GetUAVs();
    if (resIndex >= resources.size()) {
      return DXIL::ResourceKind::Invalid;
    }
    res = resources[resIndex].get();
  } else {
    if (ResClass != DXIL::ResourceClass::SRV) {
      return DXIL::ResourceKind::Invalid;
    }
    auto &resources = ValCtx.DxilMod.GetSRVs();
    if (resIndex >= resources.size()) {
      return DXIL::ResourceKind::Invalid;
    }
    res = resources[resIndex].get();
  }

  CompTy = res->GetCompType().GetKind();

  Value *index = createHandle.get_index();
  ConstantInt *cIndex = dyn_cast<ConstantInt>(index);

  if (!res->GetGlobalSymbol()->getType()->getPointerElementType()->isArrayTy()) {
    if (!cIndex) {
      // index must be 0 for none array resource.
      return DXIL::ResourceKind::Invalid;
    }
  }
  if (cIndex) {
    unsigned index = cIndex->getLimitedValue();
    if (index < res->GetLowerBound() || index > res->GetUpperBound()) {
      // index out of range.
      return DXIL::ResourceKind::Invalid;
    }
  }

  return res->GetKind();
}

struct ResRetUsage {
  bool x;
  bool y;
  bool z;
  bool w;
  bool status;
  ResRetUsage() : x(false), y(false), z(false), w(false), status(false) {}
};

static void CollectGetDimResRetUsage(ResRetUsage &usage, Instruction *ResRet,
                                     ValidationContext &ValCtx) {
  const unsigned kMaxResRetElementIndex = 5;
  for (User *U : ResRet->users()) {
    if (ExtractValueInst *EVI = dyn_cast<ExtractValueInst>(U)) {
      for (unsigned idx : EVI->getIndices()) {
        switch (idx) {
        case 0:
          usage.x = true;
          break;
        case 1:
          usage.y = true;
          break;
        case 2:
          usage.z = true;
          break;
        case 3:
          usage.w = true;
          break;
        case DXIL::kResRetStatusIndex:
          usage.status = true;
          break;
        default:
          // Emit index out of bound.
          ValCtx.EmitInstrError(EVI,
                                ValidationRule::InstrDxilStructUserOutOfBound);
          break;
        }
      }
    } else if (PHINode *PHI = dyn_cast<PHINode>(U)) {
      CollectGetDimResRetUsage(usage, PHI, ValCtx);
    } else {
      Instruction *User = cast<Instruction>(U);
      ValCtx.EmitInstrError(User, ValidationRule::InstrDxilStructUser);
    }
  }
}

static void ValidateStatus(Instruction *ResRet, ValidationContext &ValCtx) {
  ResRetUsage usage;
  CollectGetDimResRetUsage(usage, ResRet, ValCtx);
  if (usage.status) {
    for (User *U : ResRet->users()) {
      if (ExtractValueInst *EVI = dyn_cast<ExtractValueInst>(U)) {
        for (unsigned idx : EVI->getIndices()) {
          switch (idx) {
          case DXIL::kResRetStatusIndex:
            for (User *SU : EVI->users()) {
              Instruction *I = cast<Instruction>(SU);
              // Make sure all use is CheckAccess.
              if (!isa<CallInst>(I)) {
                ValCtx.EmitInstrError(I, ValidationRule::InstrStatus);
                return;
              }
              if (!ValCtx.DxilMod.GetOP()->IsDxilOpFuncCallInst(
                      I, DXIL::OpCode::CheckAccessFullyMapped)) {
                ValCtx.EmitInstrError(I, ValidationRule::InstrStatus);
                return;
              }
            }
            break;
          }
        }
      }
    }
  }
}

static void ValidateResourceCoord(CallInst *CI, DXIL::ResourceKind resKind,
                                  ArrayRef<Value *> coords,
                                  ValidationContext &ValCtx) {
  const unsigned kMaxNumCoords = 4;
  unsigned numCoords = DxilResource::GetNumCoords(resKind);
  for (unsigned i = 0; i < kMaxNumCoords; i++) {
    if (i < numCoords) {
      if (isa<UndefValue>(coords[i])) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceCoordinateMiss);
      }
    } else {
      if (!isa<UndefValue>(coords[i])) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceCoordinateTooMany);
      }
    }
  }
}

static void ValidateCalcLODResourceDimensionCoord(CallInst *CI, DXIL::ResourceKind resKind,
                                  ArrayRef<Value *> coords,
                                  ValidationContext &ValCtx) {
  const unsigned kMaxNumDimCoords = 3;
  unsigned numCoords = DxilResource::GetNumDimensionsForCalcLOD(resKind);
  for (unsigned i = 0; i < kMaxNumDimCoords; i++) {
    if (i < numCoords) {
      if (isa<UndefValue>(coords[i])) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceCoordinateMiss);
      }
    } else {
      if (!isa<UndefValue>(coords[i])) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceCoordinateTooMany);
      }
    }
  }
}

static void ValidateResourceOffset(CallInst *CI, DXIL::ResourceKind resKind,
                                   ArrayRef<Value *> offsets,
                                   ValidationContext &ValCtx) {
  const unsigned kMaxNumOffsets = 3;
  unsigned numOffsets = DxilResource::GetNumOffsets(resKind);
  bool hasOffset = !isa<UndefValue>(offsets[0]);

  auto validateOffset = [&](Value *offset) {
    if (ConstantInt *cOffset = dyn_cast<ConstantInt>(offset)) {
      int offset = cOffset->getValue().getSExtValue();
      if (offset > 7 || offset < -8) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrTextureOffset);
      }
    } else {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrTextureOffset);
    }
  };

  if (hasOffset) {
    validateOffset(offsets[0]);
  }

  for (unsigned i = 1; i < kMaxNumOffsets; i++) {
    if (i < numOffsets) {
      if (hasOffset) {
        if (isa<UndefValue>(offsets[i]))
          ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceOffsetMiss);
        else
          validateOffset(offsets[i]);
      }
    } else {
      if (!isa<UndefValue>(offsets[i])) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceOffsetTooMany);
      }
    }
  }
}

static void ValidateSampleInst(CallInst *CI, Value *srvHandle, Value *samplerHandle,
                               ArrayRef<Value *> coords,
                               ArrayRef<Value *> offsets,
                               bool IsSampleC,
                               ValidationContext &ValCtx) {
  if (!IsSampleC) {
    if (GetSamplerKind(samplerHandle, ValCtx) != DXIL::SamplerKind::Default) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrSamplerModeForSample);
    }
  } else {
    if (GetSamplerKind(samplerHandle, ValCtx) !=
        DXIL::SamplerKind::Comparison) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrSamplerModeForSampleC);
    }
  }

  DXIL::ComponentType compTy;
  DXIL::ResourceClass resClass;
  unsigned resIndex;
  DXIL::ResourceKind resKind =
      GetResourceKindAndCompTy(srvHandle, compTy, resClass, resIndex, ValCtx);
  bool isSampleCompTy = compTy == DXIL::ComponentType::F32;
  isSampleCompTy |= compTy == DXIL::ComponentType::SNormF32;
  isSampleCompTy |= compTy == DXIL::ComponentType::UNormF32;
  if (!isSampleCompTy) {
    ValCtx.EmitInstrError(CI, ValidationRule::InstrSampleCompType);
  }

  if (resClass != DXIL::ResourceClass::SRV) {
    ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceClassForSamplerGather);
  }

  ValidationRule rule = ValidationRule::InstrResourceKindForSample;
  if (IsSampleC) {
    rule =  ValidationRule::InstrResourceKindForSampleC;
  }

  switch (resKind) {
  case DXIL::ResourceKind::Texture1D:
  case DXIL::ResourceKind::Texture1DArray:
  case DXIL::ResourceKind::Texture2D:
  case DXIL::ResourceKind::Texture2DArray:
  case DXIL::ResourceKind::TextureCube:
  case DXIL::ResourceKind::TextureCubeArray:
    break;
  case DXIL::ResourceKind::Texture3D:
    if (IsSampleC) {
      ValCtx.EmitInstrError(CI, rule);
    }
    break;
  default:
    ValCtx.EmitInstrError(CI, rule);
    return;
  }

  // Coord match resource kind.
  ValidateResourceCoord(CI, resKind, coords, ValCtx);
  // Offset match resource kind.
  ValidateResourceOffset(CI, resKind, offsets, ValCtx);
}

static void ValidateGather(CallInst *CI, Value *srvHandle, Value *samplerHandle,
                               ArrayRef<Value *> coords,
                               ArrayRef<Value *> offsets,
                               bool IsSampleC,
                               ValidationContext &ValCtx) {
  if (!IsSampleC) {
    if (GetSamplerKind(samplerHandle, ValCtx) != DXIL::SamplerKind::Default) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrSamplerModeForSample);
    }
  } else {
    if (GetSamplerKind(samplerHandle, ValCtx) !=
        DXIL::SamplerKind::Comparison) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrSamplerModeForSampleC);
    }
  }

  DXIL::ComponentType compTy;
  DXIL::ResourceClass resClass;
  unsigned resIndex;
  DXIL::ResourceKind resKind =
      GetResourceKindAndCompTy(srvHandle, compTy, resClass, resIndex, ValCtx);

  if (resClass != DXIL::ResourceClass::SRV) {
    ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceClassForSamplerGather);
    return;
  }

  // Coord match resource kind.
  ValidateResourceCoord(CI, resKind, coords, ValCtx);
  // Offset match resource kind.
  switch (resKind) {
  case DXIL::ResourceKind::Texture2D:
  case DXIL::ResourceKind::Texture2DArray: {
    bool hasOffset = !isa<UndefValue>(offsets[0]);
    if (hasOffset) {
      if (isa<UndefValue>(offsets[1])) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceOffsetMiss);
      }
    }
  } break;
  case DXIL::ResourceKind::TextureCube:
  case DXIL::ResourceKind::TextureCubeArray: {
    if (!isa<UndefValue>(offsets[0])) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceOffsetTooMany);
    }
    if (!isa<UndefValue>(offsets[1])) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceOffsetTooMany);
    }
  } break;
  default:
    // Invalid resource type for gather.
    ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceKindForGather);
    break;
  }
}

static unsigned StoreValueToMask(ArrayRef<Value *> vals) {
  unsigned mask = 0;
  for (unsigned i = 0; i < 4; i++) {
    if (!isa<UndefValue>(vals[i])) {
      mask |= 1<<i;
    }
  }
  return mask;
}

static int GetCBufSize(Value *cbHandle, ValidationContext &ValCtx) {
  DxilInst_CreateHandle createHandle(cast<CallInst>(cbHandle));
  if (!createHandle) {
    ValCtx.EmitInstrError(cast<CallInst>(cbHandle),
                          ValidationRule::InstrHandleNotFromCreateHandle);
    return -1;
  }

  Value *resourceClass = createHandle.get_resourceClass();
  if (!isa<ConstantInt>(resourceClass)) {
    ValCtx.EmitInstrError(cast<CallInst>(cbHandle),
                          ValidationRule::InstrOpConstRange);
    return -1;
  }

  if (static_cast<DXIL::ResourceClass>(createHandle.get_resourceClass_val()) !=
      DXIL::ResourceClass::CBuffer) {
    ValCtx.EmitInstrError(cast<CallInst>(cbHandle), ValidationRule::InstrCBufferClassForCBufferHandle);
    return -1;
  }

  Value *rangeIndex = createHandle.get_rangeId();
  if (!isa<ConstantInt>(rangeIndex)) {
    ValCtx.EmitInstrError(cast<CallInst>(cbHandle),
                          ValidationRule::InstrOpConstRange);
    return -1;
  }

  DxilCBuffer &CB = ValCtx.DxilMod.GetCBuffer(
      cast<ConstantInt>(rangeIndex)->getLimitedValue());
  return CB.GetSize();
}

static unsigned GetNumVertices(DXIL::InputPrimitive inputPrimitive) {
  const unsigned InputPrimitiveVertexTab[] = {
    0, // Undefined = 0,
    1, // Point = 1,
    2, // Line = 2,
    3, // Triangle = 3,
    0, // Reserved4 = 4,
    0, // Reserved5 = 5,
    4, // LineWithAdjacency = 6,
    6, // TriangleWithAdjacency = 7,
    1, // ControlPointPatch1 = 8,
    2, // ControlPointPatch2 = 9,
    3, // ControlPointPatch3 = 10,
    4, // ControlPointPatch4 = 11,
    5, // ControlPointPatch5 = 12,
    6, // ControlPointPatch6 = 13,
    7, // ControlPointPatch7 = 14,
    8, // ControlPointPatch8 = 15,
    9, // ControlPointPatch9 = 16,
    10, // ControlPointPatch10 = 17,
    11, // ControlPointPatch11 = 18,
    12, // ControlPointPatch12 = 19,
    13, // ControlPointPatch13 = 20,
    14, // ControlPointPatch14 = 21,
    15, // ControlPointPatch15 = 22,
    16, // ControlPointPatch16 = 23,
    17, // ControlPointPatch17 = 24,
    18, // ControlPointPatch18 = 25,
    19, // ControlPointPatch19 = 26,
    20, // ControlPointPatch20 = 27,
    21, // ControlPointPatch21 = 28,
    22, // ControlPointPatch22 = 29,
    23, // ControlPointPatch23 = 30,
    24, // ControlPointPatch24 = 31,
    25, // ControlPointPatch25 = 32,
    26, // ControlPointPatch26 = 33,
    27, // ControlPointPatch27 = 34,
    28, // ControlPointPatch28 = 35,
    29, // ControlPointPatch29 = 36,
    30, // ControlPointPatch30 = 37,
    31, // ControlPointPatch31 = 38,
    32, // ControlPointPatch32 = 39,
    0, // LastEntry,
  };

  unsigned primitiveIdx = static_cast<unsigned>(inputPrimitive);
  return InputPrimitiveVertexTab[primitiveIdx];
}

static void ValidateDxilOperationCallInProfile(CallInst *CI,
                                               DXIL::OpCode opcode,
                                               const ShaderModel *pSM,
                                               ValidationContext &ValCtx) {
  switch (opcode) {
  case DXIL::OpCode::LoadInput: {
    Value *inputID = CI->getArgOperand(DXIL::OperandIndex::kLoadInputIDOpIdx);
    DxilSignature &inputSig = ValCtx.DxilMod.GetInputSignature();
    Value *row = CI->getArgOperand(DXIL::OperandIndex::kLoadInputRowOpIdx);
    Value *col = CI->getArgOperand(DXIL::OperandIndex::kLoadInputColOpIdx);
    ValidateSignatureAccess(CI, inputSig, inputID, row, col, ValCtx);

    // Check vertexID in ps/vs. and none array input.
    Value *vertexID =
        CI->getArgOperand(DXIL::OperandIndex::kLoadInputVertexIDOpIdx);
    bool usedVertexID = vertexID && !isa<UndefValue>(vertexID);
    if (pSM->IsVS() || pSM->IsPS()) {
      if (usedVertexID) {
        // use vertexID in VS/PS input.
        ValCtx.EmitInstrError(CI, ValidationRule::SmOperand);
        return;
      }
    } else {
      if (ConstantInt *cVertexID = dyn_cast<ConstantInt>(vertexID)) {
        int immVertexID = cVertexID->getValue().getLimitedValue();
        if (cVertexID->getValue().isNegative()) {
          immVertexID = cVertexID->getValue().getSExtValue();
        }
        const int low = 0;
        int high = 0;
        if (pSM->IsGS()) {
          DXIL::InputPrimitive inputPrimitive =
              ValCtx.DxilMod.GetInputPrimitive();
          high = GetNumVertices(inputPrimitive);
        } else if (pSM->IsDS()) {
          high = ValCtx.DxilMod.GetInputControlPointCount();
        } else if (pSM->IsHS()) {
          high = ValCtx.DxilMod.GetInputControlPointCount();
        } else {
          ValCtx.EmitFormatError(ValidationRule::SmOpcodeInInvalidFunction,
                                 {"LoadInput", "VS/HS/DS/GS/PS"});
        }
        if (immVertexID < low || immVertexID >= high) {
          std::string range = std::to_string(low)+"~"+
                                       std::to_string(high);
          ValCtx.EmitOperandOutOfRange(CI, "VertexID", range,
                                       std::to_string(immVertexID));
        }
      }
    }
  } break;
  case DXIL::OpCode::DomainLocation: {
    Value *colValue = CI->getArgOperand(DXIL::OperandIndex::kDomainLocationColOpIdx);
    if (!isa<ConstantInt>(colValue)) {
      // col must be const
      ValCtx.EmitInstrFormatError(CI, ValidationRule::InstrOpConst,
                                  {"Col", "DomainLocation"});
    } else {
      unsigned col = cast<ConstantInt>(colValue)->getLimitedValue();
      if (col >= ValCtx.domainLocSize) {
        ValCtx.EmitError(ValidationRule::SmDomainLocationIdxOOB);
      }
    }
  } break;
  case DXIL::OpCode::CBufferLoad: {
    DxilInst_CBufferLoad CBLoad(CI);
    Value *regIndex = CBLoad.get_byteOffset();
    if (ConstantInt *cIndex = dyn_cast<ConstantInt>(regIndex)) {
      int offset = cIndex->getLimitedValue();
      int size = GetCBufSize(CBLoad.get_handle(), ValCtx);
      if (size > 0 &&  offset >= size) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrCBufferOutOfBound);
      }
    }
  } break;
  case DXIL::OpCode::CBufferLoadLegacy: {
    DxilInst_CBufferLoadLegacy CBLoad(CI);
    Value *regIndex = CBLoad.get_regIndex();
    if (ConstantInt *cIndex = dyn_cast<ConstantInt>(regIndex)) {
      int offset = cIndex->getLimitedValue() * 16; // 16 bytes align
      int size = GetCBufSize(CBLoad.get_handle(), ValCtx);
      if (size > 0 &&  offset >= size) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrCBufferOutOfBound);
      }
    }
  } break;
  case DXIL::OpCode::StoreOutput: {
    Value *outputID =
        CI->getArgOperand(DXIL::OperandIndex::kStoreOutputIDOpIdx);
    DxilSignature &outputSig = ValCtx.DxilMod.GetOutputSignature();
    Value *row = CI->getArgOperand(DXIL::OperandIndex::kStoreOutputRowOpIdx);
    Value *col = CI->getArgOperand(DXIL::OperandIndex::kStoreOutputColOpIdx);
    ValidateSignatureAccess(CI, outputSig, outputID, row, col, ValCtx);
  } break;
  case DXIL::OpCode::OutputControlPointID: {
    // Only used in hull shader.
    Function *func = CI->getParent()->getParent();
    if (ValCtx.patchConstFuncCallSet.count(func) > 0 || !ValCtx.DxilMod.GetShaderModel()->IsHS()) {
      ValCtx.EmitFormatError(ValidationRule::SmOpcodeInInvalidFunction,
                             {"OutputControlPointID", "hull function"});
    }
  } break;
  case DXIL::OpCode::LoadOutputControlPoint: {
    // Only used in patch constant function.
    Function *func = CI->getParent()->getParent();
    if (ValCtx.entryFuncCallSet.count(func) > 0) {
      ValCtx.EmitFormatError(
          ValidationRule::SmOpcodeInInvalidFunction,
          {"LoadOutputControlPoint", "PatchConstant function"});
    }
    Value *outputID =
        CI->getArgOperand(DXIL::OperandIndex::kStoreOutputIDOpIdx);
    DxilSignature &outputSig = ValCtx.DxilMod.GetOutputSignature();
    Value *row = CI->getArgOperand(DXIL::OperandIndex::kStoreOutputRowOpIdx);
    Value *col = CI->getArgOperand(DXIL::OperandIndex::kStoreOutputColOpIdx);
    ValidateSignatureAccess(CI, outputSig, outputID, row, col, ValCtx);
  } break;
  case DXIL::OpCode::StorePatchConstant: {
    // Only used in patch constant function.
    Function *func = CI->getParent()->getParent();
    if (ValCtx.entryFuncCallSet.count(func) > 0) {
      ValCtx.EmitFormatError(ValidationRule::SmOpcodeInInvalidFunction,
                             {"StorePatchConstant", "PatchConstant function"});
    }
    Value *outputID =
        CI->getArgOperand(DXIL::OperandIndex::kStoreOutputIDOpIdx);
    DxilSignature &outputSig = ValCtx.DxilMod.GetPatchConstantSignature();
    Value *row = CI->getArgOperand(DXIL::OperandIndex::kStoreOutputRowOpIdx);
    Value *col = CI->getArgOperand(DXIL::OperandIndex::kStoreOutputColOpIdx);
    ValidateSignatureAccess(CI, outputSig, outputID, row, col, ValCtx);
  } break;
  case DXIL::OpCode::EvalCentroid:
  case DXIL::OpCode::EvalSampleIndex:
  case DXIL::OpCode::EvalSnapped: {
    // Eval* share same operand index with load input.
    Value *inputID = CI->getArgOperand(DXIL::OperandIndex::kLoadInputIDOpIdx);
    DxilSignature &inputSig = ValCtx.DxilMod.GetInputSignature();
    Value *row = CI->getArgOperand(DXIL::OperandIndex::kLoadInputRowOpIdx);
    Value *col = CI->getArgOperand(DXIL::OperandIndex::kLoadInputColOpIdx);
    DxilSignatureElement *pSE =
        ValidateSignatureAccess(CI, inputSig, inputID, row, col, ValCtx);
    if (pSE) {
      switch (pSE->GetInterpolationMode()->GetKind()) {
      case DXIL::InterpolationMode::Linear:
      case DXIL::InterpolationMode::LinearNoperspective:
      case DXIL::InterpolationMode::LinearCentroid:
      case DXIL::InterpolationMode::LinearNoperspectiveCentroid:
      case DXIL::InterpolationMode::LinearSample:
      case DXIL::InterpolationMode::LinearNoperspectiveSample:
        break;
      default:
        ValCtx.EmitInstrFormatError(
            CI, ValidationRule::InstrEvalInterpolationMode, {pSE->GetName()});
        break;
      }
      if (pSE->GetSemantic()->GetKind() == DXIL::SemanticKind::Position) {
        ValCtx.EmitInstrFormatError(
            CI, ValidationRule::InstrCannotPullPosition,
            {ValCtx.DxilMod.GetShaderModel()->GetName()});
      }
    }
  } break;
  case DXIL::OpCode::AttributeAtVertex: {
    Value *Attribute = CI->getArgOperand(DXIL::OperandIndex::kBinarySrc0OpIdx);
    DxilSignature &inputSig = ValCtx.DxilMod.GetInputSignature();
    Value *row = CI->getArgOperand(DXIL::OperandIndex::kLoadInputRowOpIdx);
    Value *col = CI->getArgOperand(DXIL::OperandIndex::kLoadInputColOpIdx);
    DxilSignatureElement *pSE =
        ValidateSignatureAccess(CI, inputSig, Attribute, row, col, ValCtx);
    if (pSE && pSE->GetInterpolationMode()->GetKind() !=
                   hlsl::InterpolationMode::Kind::Constant) {
      ValCtx.EmitInstrFormatError(
          CI, ValidationRule::InstrAttributeAtVertexNoInterpolation,
          {pSE->GetName()});
    }
  } break;
  case DXIL::OpCode::GetDimensions: {
    DxilInst_GetDimensions getDim(CI);
    Value *handle = getDim.get_handle();
    DXIL::ComponentType compTy;
    DXIL::ResourceClass resClass;
    unsigned resIndex;
    DXIL::ResourceKind resKind =
        GetResourceKindAndCompTy(handle, compTy, resClass, resIndex, ValCtx);

    // Check the result component use.
    ResRetUsage usage;
    CollectGetDimResRetUsage(usage, CI, ValCtx);

    // Mip level only for texture.
    switch (resKind) {
    case DXIL::ResourceKind::Texture1D:
      if (usage.y) {
        ValCtx.EmitInstrFormatError(
            CI, ValidationRule::InstrUndefResultForGetDimension,
            {"y", "Texture1D"});
      }
      if (usage.z) {
        ValCtx.EmitInstrFormatError(
            CI, ValidationRule::InstrUndefResultForGetDimension,
            {"z", "Texture1D"});
      }
      break;
    case DXIL::ResourceKind::Texture1DArray:
      if (usage.z) {
        ValCtx.EmitInstrFormatError(
            CI, ValidationRule::InstrUndefResultForGetDimension,
            {"z", "Texture1DArray"});
      }
      break;
    case DXIL::ResourceKind::Texture2D:
      if (usage.z) {
        ValCtx.EmitInstrFormatError(
            CI, ValidationRule::InstrUndefResultForGetDimension,
            {"z", "Texture2D"});
      }
      break;
    case DXIL::ResourceKind::Texture2DArray:
      break;
    case DXIL::ResourceKind::Texture2DMS:
      if (usage.z) {
        ValCtx.EmitInstrFormatError(
            CI, ValidationRule::InstrUndefResultForGetDimension,
            {"z", "Texture2DMS"});
      }
      break;
    case DXIL::ResourceKind::Texture2DMSArray:
      break;
    case DXIL::ResourceKind::Texture3D:
      break;
    case DXIL::ResourceKind::TextureCube:
      if (usage.z) {
        ValCtx.EmitInstrFormatError(
            CI, ValidationRule::InstrUndefResultForGetDimension,
            {"z", "TextureCube"});
      }
      break;
    case DXIL::ResourceKind::TextureCubeArray:
      break;
    case DXIL::ResourceKind::StructuredBuffer:
    case DXIL::ResourceKind::RawBuffer:
    case DXIL::ResourceKind::TypedBuffer:
    case DXIL::ResourceKind::TBuffer: {
      Value *mip = getDim.get_mipLevel();
      if (!isa<UndefValue>(mip)) {
        ValCtx.EmitInstrError(
            CI, ValidationRule::InstrMipLevelForGetDimension);
      }
      if (resKind != DXIL::ResourceKind::Invalid) {
        if (usage.y || usage.z || usage.w) {
          ValCtx.EmitInstrFormatError(
              CI, ValidationRule::InstrUndefResultForGetDimension,
              {"invalid", "resource"});
        }
      }
    } break;
    default: {
      ValCtx.EmitInstrError(
          CI, ValidationRule::InstrResourceKindForGetDim);
    } break;
    }

    if (usage.status) {
      ValCtx.EmitInstrFormatError(
          CI, ValidationRule::InstrUndefResultForGetDimension,
          {"invalid", "resource"});
    }
  } break;
  case DXIL::OpCode::CalculateLOD: {
    DxilInst_CalculateLOD lod(CI);
    Value *samplerHandle = lod.get_sampler();
    if (GetSamplerKind(samplerHandle, ValCtx) != DXIL::SamplerKind::Default) {
      ValCtx.EmitInstrError(CI,
                            ValidationRule::InstrSamplerModeForLOD);
    }
    Value *handle = lod.get_handle();
    DXIL::ComponentType compTy;
    DXIL::ResourceClass resClass;
    unsigned resIndex;
    DXIL::ResourceKind resKind =
        GetResourceKindAndCompTy(handle, compTy, resClass, resIndex, ValCtx);
    if (resClass != DXIL::ResourceClass::SRV) {
      ValCtx.EmitInstrError(CI,
                            ValidationRule::InstrResourceClassForSamplerGather);
      return;
    }
    // Coord match resource.
    ValidateCalcLODResourceDimensionCoord(
        CI, resKind, {lod.get_coord0(), lod.get_coord1(), lod.get_coord2()},
        ValCtx);

    switch (resKind) {
    case DXIL::ResourceKind::Texture1D:
    case DXIL::ResourceKind::Texture1DArray:
    case DXIL::ResourceKind::Texture2D:
    case DXIL::ResourceKind::Texture2DArray:
    case DXIL::ResourceKind::Texture3D:
    case DXIL::ResourceKind::TextureCube:
    case DXIL::ResourceKind::TextureCubeArray:
      break;
    default:
      ValCtx.EmitInstrError(
          CI, ValidationRule::InstrResourceKindForCalcLOD);
      break;
    }

  } break;
  case DXIL::OpCode::TextureGather: {
    DxilInst_TextureGather gather(CI);
    ValidateGather(CI, gather.get_srv(), gather.get_sampler(),
                   {gather.get_coord0(), gather.get_coord1(),
                    gather.get_coord2(), gather.get_coord3()},
                   {gather.get_offset0(), gather.get_offset1()},
                   /*IsSampleC*/ false, ValCtx);
  } break;
  case DXIL::OpCode::TextureGatherCmp: {
    DxilInst_TextureGatherCmp gather(CI);
    ValidateGather(CI, gather.get_srv(), gather.get_sampler(),
                   {gather.get_coord0(), gather.get_coord1(),
                    gather.get_coord2(), gather.get_coord3()},
                   {gather.get_offset0(), gather.get_offset1()},
                   /*IsSampleC*/ true, ValCtx);
  } break;
  case DXIL::OpCode::Sample: {
    DxilInst_Sample sample(CI);
    ValidateSampleInst(
        CI, sample.get_srv(), sample.get_sampler(),
        {sample.get_coord0(), sample.get_coord1(), sample.get_coord2(),
         sample.get_coord3()},
        {sample.get_offset0(), sample.get_offset1(), sample.get_offset2()},
        /*IsSampleC*/ false, ValCtx);
  } break;
  case DXIL::OpCode::SampleCmp: {
    DxilInst_SampleCmp sample(CI);
    ValidateSampleInst(
        CI, sample.get_srv(), sample.get_sampler(),
        {sample.get_coord0(), sample.get_coord1(), sample.get_coord2(),
         sample.get_coord3()},
        {sample.get_offset0(), sample.get_offset1(), sample.get_offset2()},
        /*IsSampleC*/ true, ValCtx);
  } break;
  case DXIL::OpCode::SampleCmpLevelZero: {
    // sampler must be comparison mode.
    DxilInst_SampleCmpLevelZero sample(CI);
    ValidateSampleInst(
        CI, sample.get_srv(), sample.get_sampler(),
        {sample.get_coord0(), sample.get_coord1(), sample.get_coord2(),
         sample.get_coord3()},
        {sample.get_offset0(), sample.get_offset1(), sample.get_offset2()},
        /*IsSampleC*/ true, ValCtx);
  } break;
  case DXIL::OpCode::SampleBias: {
    DxilInst_SampleBias sample(CI);
    Value *bias = sample.get_bias();
    if (ConstantFP *cBias = dyn_cast<ConstantFP>(bias)) {
      float fBias = cBias->getValueAPF().convertToFloat();
      if (fBias < DXIL::kMinMipLodBias || fBias > DXIL::kMaxMipLodBias) {
        ValCtx.EmitInstrFormatError(
            CI, ValidationRule::InstrImmBiasForSampleB,
            {std::to_string(DXIL::kMinMipLodBias),
             std::to_string(DXIL::kMaxMipLodBias),
             std::to_string(cBias->getValueAPF().convertToFloat())});
      }
    }

    ValidateSampleInst(
        CI, sample.get_srv(), sample.get_sampler(),
        {sample.get_coord0(), sample.get_coord1(), sample.get_coord2(),
         sample.get_coord3()},
        {sample.get_offset0(), sample.get_offset1(), sample.get_offset2()},
        /*IsSampleC*/ false, ValCtx);
  } break;
  case DXIL::OpCode::SampleGrad: {
    DxilInst_SampleGrad sample(CI);
    ValidateSampleInst(
        CI, sample.get_srv(), sample.get_sampler(),
        {sample.get_coord0(), sample.get_coord1(), sample.get_coord2(),
         sample.get_coord3()},
        {sample.get_offset0(), sample.get_offset1(), sample.get_offset2()},
        /*IsSampleC*/ false, ValCtx);
  } break;
  case DXIL::OpCode::SampleLevel: {
    DxilInst_SampleLevel sample(CI);
    ValidateSampleInst(
        CI, sample.get_srv(), sample.get_sampler(),
        {sample.get_coord0(), sample.get_coord1(), sample.get_coord2(),
         sample.get_coord3()},
        {sample.get_offset0(), sample.get_offset1(), sample.get_offset2()},
        /*IsSampleC*/ false, ValCtx);
  } break;
  case DXIL::OpCode::CheckAccessFullyMapped: {
    Value *Src = CI->getArgOperand(DXIL::OperandIndex::kUnarySrc0OpIdx);
    ExtractValueInst *EVI = dyn_cast<ExtractValueInst>(Src);
    if (!EVI) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrCheckAccessFullyMapped);
    } else {
      Value *V = EVI->getOperand(0);
      bool isLegal = EVI->getNumIndices() == 1 &&
                     EVI->getIndices()[0] == DXIL::kResRetStatusIndex &&
                     ValCtx.DxilMod.GetOP()->IsResRetType(V->getType());
      if (!isLegal) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrCheckAccessFullyMapped);
      }
    }
  } break;
  case DXIL::OpCode::Barrier: {
    DxilInst_Barrier barrier(CI);
    Value *mode = barrier.get_barrierMode();
    ConstantInt *cMode = dyn_cast<ConstantInt>(mode);
    if (!cMode) {
      ValCtx.EmitInstrFormatError(CI, ValidationRule::InstrOpConst,
                                  {"Mode", "Barrier"});
      return;
    }

    const unsigned uglobal =
        static_cast<unsigned>(DXIL::BarrierMode::UAVFenceGlobal);
    const unsigned g = static_cast<unsigned>(DXIL::BarrierMode::TGSMFence);
    const unsigned t =
        static_cast<unsigned>(DXIL::BarrierMode::SyncThreadGroup);
    const unsigned ut =
        static_cast<unsigned>(DXIL::BarrierMode::UAVFenceThreadGroup);
    unsigned barrierMode = cMode->getLimitedValue();

    if (ValCtx.DxilMod.GetShaderModel()->IsCS()) {
      bool bHasUGlobal = barrierMode & uglobal;
      bool bHasGroup = barrierMode & g;
      bool bHasUGroup = barrierMode & ut;
      if (bHasUGlobal && bHasUGroup) {
        ValCtx.EmitInstrError(CI,
                              ValidationRule::InstrBarrierModeUselessUGroup);
      }

      if (!bHasUGlobal && !bHasGroup && !bHasUGroup) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrBarrierModeNoMemory);
      }
    } else {
      if (uglobal != barrierMode) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrBarrierModeForNonCS);
      }
    }
  } break;
  case DXIL::OpCode::BufferStore: {
    DxilInst_BufferStore bufSt(CI);
    DXIL::ComponentType compTy;
    DXIL::ResourceClass resClass;
    unsigned resIndex;
    DXIL::ResourceKind resKind =
        GetResourceKindAndCompTy(bufSt.get_uav(), compTy, resClass, resIndex, ValCtx);

    if (resClass != DXIL::ResourceClass::UAV) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceClassForUAVStore);
    }

    ConstantInt *mask = dyn_cast<ConstantInt>(bufSt.get_mask());
    if (!mask) {
      // Mask for buffer store should be immediate.
      ValCtx.EmitInstrFormatError(CI, ValidationRule::InstrOpConst, {"Mask", "BufferStore"});
      return;
    }
    unsigned uMask = mask->getLimitedValue();
    unsigned stValMask =
        StoreValueToMask({bufSt.get_value0(), bufSt.get_value1(),
                          bufSt.get_value2(), bufSt.get_value3()});

    if (stValMask != uMask) {
      ValCtx.EmitInstrFormatError(
          CI, ValidationRule::InstrWriteMaskMatchValueForUAVStore,
          {std::to_string(uMask), std::to_string(stValMask)});
    }

    Value *offset = bufSt.get_coord1();

    switch (resKind) {
    case DXIL::ResourceKind::RawBuffer:
      if (!isa<UndefValue>(offset)) {
        ValCtx.EmitInstrError(
            CI, ValidationRule::InstrCoordinateCountForRawTypedBuf);
      }
      break;
    case DXIL::ResourceKind::TypedBuffer:
    case DXIL::ResourceKind::TBuffer:
      if (!isa<UndefValue>(offset)) {
        ValCtx.EmitInstrError(
            CI, ValidationRule::InstrCoordinateCountForRawTypedBuf);
      }

      if (uMask != 0xf) {
        ValCtx.EmitInstrError(
            CI, ValidationRule::InstrWriteMaskForTypedUAVStore);
      }
      break;
    case DXIL::ResourceKind::StructuredBuffer:
      if (isa<UndefValue>(offset)) {
        ValCtx.EmitInstrError(
            CI, ValidationRule::InstrCoordinateCountForStructBuf);
      }
      break;
    default:
      ValCtx.EmitInstrError(
          CI, ValidationRule::InstrResourceKindForBufferLoadStore);
      break;
    }

  } break;
  case DXIL::OpCode::TextureStore: {
    DxilInst_TextureStore texSt(CI);
    DXIL::ComponentType compTy;
    DXIL::ResourceClass resClass;
    unsigned resIndex;
    DXIL::ResourceKind resKind =
        GetResourceKindAndCompTy(texSt.get_srv(), compTy, resClass, resIndex, ValCtx);

    if (resClass != DXIL::ResourceClass::UAV) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceClassForUAVStore);
    }

    ConstantInt *mask = dyn_cast<ConstantInt>(texSt.get_mask());
    if (!mask) {
      // Mask for buffer store should be immediate.
      ValCtx.EmitInstrFormatError(CI, ValidationRule::InstrOpConst, {"Mask", "TextureStore"});
      return;
    }
    unsigned uMask = mask->getLimitedValue();
    if (uMask != 0xf) {
      ValCtx.EmitInstrError(
          CI, ValidationRule::InstrWriteMaskForTypedUAVStore);
    }

    unsigned stValMask =
        StoreValueToMask({texSt.get_value0(), texSt.get_value1(),
                          texSt.get_value2(), texSt.get_value3()});

    if (stValMask != uMask) {
      ValCtx.EmitInstrFormatError(
          CI, ValidationRule::InstrWriteMaskMatchValueForUAVStore,
          {std::to_string(uMask), std::to_string(stValMask)});
    }

    switch (resKind) {
    case DXIL::ResourceKind::Texture1D:
    case DXIL::ResourceKind::Texture1DArray:
    case DXIL::ResourceKind::Texture2D:
    case DXIL::ResourceKind::Texture2DArray:
    case DXIL::ResourceKind::Texture3D:
      break;
    default:
      ValCtx.EmitInstrError(
          CI, ValidationRule::InstrResourceKindForTextureStore);
      break;
    }
  } break;
  case DXIL::OpCode::BufferLoad: {
    DxilInst_BufferLoad bufLd(CI);
    DXIL::ComponentType compTy;
    DXIL::ResourceClass resClass;
    unsigned resIndex;
    DXIL::ResourceKind resKind =
        GetResourceKindAndCompTy(bufLd.get_srv(), compTy, resClass, resIndex, ValCtx);

    if (resClass != DXIL::ResourceClass::SRV &&
        resClass != DXIL::ResourceClass::UAV) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceClassForLoad);
    }

    Value *offset = bufLd.get_wot();

    switch (resKind) {
    case DXIL::ResourceKind::RawBuffer:
    case DXIL::ResourceKind::TypedBuffer:
    case DXIL::ResourceKind::TBuffer:
      if (!isa<UndefValue>(offset)) {
        ValCtx.EmitInstrError(
            CI, ValidationRule::InstrCoordinateCountForRawTypedBuf);
      }
      break;
    case DXIL::ResourceKind::StructuredBuffer:
      if (isa<UndefValue>(offset)) {
        ValCtx.EmitInstrError(
            CI, ValidationRule::InstrCoordinateCountForStructBuf);
      }
      break;
    default:
      ValCtx.EmitInstrError(
          CI, ValidationRule::InstrResourceKindForBufferLoadStore);
      break;
    }

  } break;
  case DXIL::OpCode::TextureLoad: {
    DxilInst_TextureLoad texLd(CI);
    DXIL::ComponentType compTy;
    DXIL::ResourceClass resClass;
    unsigned resIndex;
    DXIL::ResourceKind resKind =
        GetResourceKindAndCompTy(texLd.get_srv(), compTy, resClass, resIndex, ValCtx);

    Value *mipLevel = texLd.get_mipLevelOrSampleCount();

    if (resClass == DXIL::ResourceClass::UAV) {
      bool noOffset = isa<UndefValue>(texLd.get_offset0());
      noOffset &= isa<UndefValue>(texLd.get_offset1());
      noOffset &= isa<UndefValue>(texLd.get_offset2());
      if (!noOffset) {
        ValCtx.EmitInstrError(CI,
                              ValidationRule::InstrOffsetOnUAVLoad);
      }
      if (!isa<UndefValue>(mipLevel)) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrMipOnUAVLoad);
      }
    } else {
      if (resClass != DXIL::ResourceClass::SRV) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceClassForLoad);
      }
    }

    switch (resKind) {
    case DXIL::ResourceKind::Texture1D:
    case DXIL::ResourceKind::Texture1DArray:
    case DXIL::ResourceKind::Texture2D:
    case DXIL::ResourceKind::Texture2DArray:
    case DXIL::ResourceKind::Texture3D:
      break;
    case DXIL::ResourceKind::Texture2DMS:
    case DXIL::ResourceKind::Texture2DMSArray: {
      if (isa<UndefValue>(mipLevel)) {
        ValCtx.EmitInstrError(
            CI, ValidationRule::InstrSampleIndexForLoad2DMS);
      }
    } break;
    default:
      ValCtx.EmitInstrError(
          CI, ValidationRule::InstrResourceKindForTextureLoad);
      break;
    }
  } break;
  case DXIL::OpCode::CutStream:
  case DXIL::OpCode::EmitThenCutStream:
  case DXIL::OpCode::EmitStream: {
    if (pSM->IsGS()) {
      unsigned streamMask = ValCtx.DxilMod.GetActiveStreamMask();

      Value *streamID =
          CI->getArgOperand(DXIL::OperandIndex::kStreamEmitCutIDOpIdx);
      if (ConstantInt *cStreamID = dyn_cast<ConstantInt>(streamID)) {
        int immStreamID = cStreamID->getValue().getLimitedValue();
        if (cStreamID->getValue().isNegative() || immStreamID >= 4) {
          ValCtx.EmitOperandOutOfRange(CI, "StreamID","0~4",
                                       std::to_string(immStreamID));
        } else {
          unsigned immMask = 1 << immStreamID;
          if ((streamMask & immMask) == 0) {
            std::string range;
            for (unsigned i = 0; i < 4; i++) {
              if (streamMask & (1 << i)) {
                range += std::to_string(i) + " ";
              }
            }
            ValCtx.EmitOperandOutOfRange(CI, "StreamID", range,
                                         std::to_string(immStreamID));
          }
        }

      } else {
        ValCtx.EmitInstrFormatError(CI, ValidationRule::InstrOpConst,
                                    {"StreamID", "Emit/CutStream"});
      }
    } else {
      ValCtx.EmitInstrFormatError(CI, ValidationRule::SmOpcodeInInvalidFunction,
                                  {"Emit/CutStream", "Geometry shader"});
    }
  } break;
  case DXIL::OpCode::BufferUpdateCounter: {
    DxilInst_BufferUpdateCounter updateCounter(CI);
    DXIL::ComponentType compTy;
    DXIL::ResourceClass resClass;
    unsigned resIndex;
    DXIL::ResourceKind resKind =
        GetResourceKindAndCompTy(updateCounter.get_uav(), compTy, resClass, resIndex, ValCtx);

    if (resClass != DXIL::ResourceClass::UAV) {
      ValCtx.EmitInstrError(CI,
                               ValidationRule::InstrBufferUpdateCounterOnUAV);
    }

    if (resKind != DXIL::ResourceKind::StructuredBuffer) {
      ValCtx.EmitInstrError(CI,
                               ValidationRule::SmCounterOnlyOnStructBuf);
    }

    Value *inc = updateCounter.get_inc();
    if (ConstantInt *cInc = dyn_cast<ConstantInt>(inc)) {
      bool isInc = cInc->getLimitedValue() == 1;
      if (ValCtx.UavCounterIncMap.count(resIndex)) {
        if (isInc != ValCtx.UavCounterIncMap[resIndex]) {
          ValCtx.EmitInstrError(CI, ValidationRule::InstrOnlyOneAllocConsume);
        }
      }
      else {
        ValCtx.UavCounterIncMap[resIndex] = isInc;
      }
    } else {
        ValCtx.EmitInstrFormatError(CI, ValidationRule::InstrOpConst, {"inc", "BufferUpdateCounter"});
    }

  } break;
  case DXIL::OpCode::Asin: {
    DxilInst_Asin I(CI);
    if (ConstantFP *imm = dyn_cast<ConstantFP>(I.get_value())) {
      if (imm->getValueAPF().isInfinity()) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrNoIndefiniteAsin);
      }
    }
  } break;
  case DXIL::OpCode::Acos: {
    DxilInst_Acos I(CI);
    if (ConstantFP *imm = dyn_cast<ConstantFP>(I.get_value())) {
      if (imm->getValueAPF().isInfinity()) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrNoIndefiniteAcos);
      }
    }
  } break;
  case DXIL::OpCode::Log: {
    DxilInst_Log I(CI);
    if (ConstantFP *imm = dyn_cast<ConstantFP>(I.get_value())) {
      if (imm->getValueAPF().isInfinity()) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrNoIndefiniteLog);
      }
    }
  } break;
  case DXIL::OpCode::DerivFineX:
  case DXIL::OpCode::DerivFineY:
  case DXIL::OpCode::DerivCoarseX:
  case DXIL::OpCode::DerivCoarseY: {
    Value *V = CI->getArgOperand(DXIL::OperandIndex::kUnarySrc0OpIdx);
    if (ConstantFP *imm = dyn_cast<ConstantFP>(V)) {
      if (imm->getValueAPF().isInfinity()) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrNoIndefiniteDsxy);
      }
    }
  } break;
  case DXIL::OpCode::Coverage:
    ValCtx.m_bCoverageIn = true;
    break;
  case DXIL::OpCode::InnerCoverage:
    ValCtx.m_bInnerCoverageIn = true;
    break;
  case DXIL::OpCode::ViewID:
    ValCtx.hasViewID = true;
    break;
  case DXIL::OpCode::QuadOp:
    if (!pSM->IsPS())
      ValCtx.EmitFormatError(ValidationRule::SmOpcodeInInvalidFunction,
                             {"QuadReadAcross", "Pixel Shader"});
    break;
  case DXIL::OpCode::QuadReadLaneAt:
    if (!pSM->IsPS())
      ValCtx.EmitFormatError(ValidationRule::SmOpcodeInInvalidFunction,
                             {"QuadReadLaneAt", "Pixel Shader"});
    break;
  default:
    // Skip opcodes don't need special check.
    break;
  }

  if (ValCtx.m_bCoverageIn && ValCtx.m_bInnerCoverageIn) {
    ValCtx.EmitError(ValidationRule::SmPSCoverageAndInnerCoverage);
  }

}

static bool IsDxilFunction(llvm::Function *F) {
  unsigned argSize = F->arg_size();
  if (argSize < 1) {
    // Cannot be a DXIL operation.
    return false;
  }

  return OP::IsDxilOpFunc(F);
}

static void ValidateExternalFunction(Function *F, ValidationContext &ValCtx) {
  if (!IsDxilFunction(F)) {
    ValCtx.EmitGlobalValueError(F, ValidationRule::DeclDxilFnExtern);
    return;
  }

  if (F->use_empty()) {
    ValCtx.EmitGlobalValueError(F, ValidationRule::DeclUsedExternalFunction);
    return;
  }

  const ShaderModel *pSM = ValCtx.DxilMod.GetShaderModel();
  OP *hlslOP = ValCtx.DxilMod.GetOP();
  Type *voidTy = Type::getVoidTy(F->getContext());
  for (User *user : F->users()) {
    CallInst *CI = dyn_cast<CallInst>(user);
    if (!CI) {
      ValCtx.EmitGlobalValueError(F, ValidationRule::DeclFnIsCalled);
      continue;
    }

    Value *argOpcode = CI->getArgOperand(0);
    ConstantInt *constOpcode = dyn_cast<ConstantInt>(argOpcode);
    if (!constOpcode) {
      // opcode not immediate; function body will validate this error.
      continue;
    }

    unsigned opcode = constOpcode->getLimitedValue();
    if (opcode >= (unsigned)DXIL::OpCode::NumOpCodes) {
      // invalid opcode; function body will validate this error.
      continue;
    }

    DXIL::OpCode dxilOpcode = (DXIL::OpCode)opcode;

    // In some cases, no overloads are provided (void is exclusive to others)
    Function *dxilFunc;
    if (hlslOP->IsOverloadLegal(dxilOpcode, voidTy)) {
      dxilFunc = hlslOP->GetOpFunc(dxilOpcode, voidTy);
    }
    else {
      Type *Ty = hlslOP->GetOverloadType(dxilOpcode, CI->getCalledFunction());
      try {
        if (!hlslOP->IsOverloadLegal(dxilOpcode, Ty)) {
          ValCtx.EmitInstrError(CI, ValidationRule::InstrOload);
          continue;
        }
      }
      catch (...) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrOload);
        continue;
      }
      dxilFunc = hlslOP->GetOpFunc(dxilOpcode, Ty->getScalarType());
    }

    if (!dxilFunc) {
      // Cannot find dxilFunction based on opcode and type.
      ValCtx.EmitInstrError(CI, ValidationRule::InstrOload);
      continue;
    }

    if (dxilFunc->getFunctionType() != F->getFunctionType()) {
      ValCtx.EmitGlobalValueError(dxilFunc, ValidationRule::InstrCallOload);
      continue;
    }

    if (!ValidateOpcodeInProfile(dxilOpcode, pSM)) {
      // Opcode not available in profile.
      ValCtx.EmitInstrFormatError(CI, ValidationRule::SmOpcode,
                                  {hlslOP->GetOpCodeName(dxilOpcode),
                                   pSM->GetName()});
      continue;
    }

    // Check more detail.
    ValidateDxilOperationCallInProfile(CI, dxilOpcode, pSM, ValCtx);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Instruction validation functions.                                         //
static bool IsLLVMInstructionAllowed(llvm::Instruction &I) {
  unsigned op = I.getOpcode();
  /* <py::lines('OPCODE-ALLOWED')>hctdb_instrhelp.get_instrs_pred("op", lambda i: not i.is_dxil_op and i.is_allowed, "llvm_id")</py>*/
  // OPCODE-ALLOWED:BEGIN
  // Instructions: Ret=1, Br=2, Switch=3, Add=8, FAdd=9, Sub=10, FSub=11, Mul=12,
  // FMul=13, UDiv=14, SDiv=15, FDiv=16, URem=17, SRem=18, FRem=19, Shl=20,
  // LShr=21, AShr=22, And=23, Or=24, Xor=25, Alloca=26, Load=27, Store=28,
  // GetElementPtr=29, AtomicCmpXchg=31, AtomicRMW=32, Trunc=33, ZExt=34,
  // SExt=35, FPToUI=36, FPToSI=37, UIToFP=38, SIToFP=39, FPTrunc=40, FPExt=41,
  // BitCast=44, AddrSpaceCast=45, ICmp=46, FCmp=47, PHI=48, Call=49, Select=50,
  // ExtractValue=57
  return 1 <= op && op <= 3 || 8 <= op && op <= 29 || 31 <= op && op <= 41 || 44 <= op && op <= 50 || op == 57;
  // OPCODE-ALLOWED:END
}

static bool IsDxilBuiltinStructType(StructType *ST, hlsl::OP *hlslOP) {
  if (ST == hlslOP->GetBinaryWithCarryType())
    return true;
  if (ST == hlslOP->GetBinaryWithTwoOutputsType())
    return true;
  if (ST == hlslOP->GetInt4Type())
    return true;
  if (ST == hlslOP->GetDimensionsType())
    return true;
  if (ST == hlslOP->GetHandleType())
    return true;
  if (ST == hlslOP->GetSamplePosType())
    return true;
  if (ST == hlslOP->GetSplitDoubleType())
    return true;

  unsigned EltNum = ST->getNumElements();
  switch (EltNum) {
  case 2:
  case 4:
  case 8: { // 2 for doubles, 8 for halfs.
    Type *EltTy = ST->getElementType(0);
    return ST == hlslOP->GetCBufferRetType(EltTy);
  } break;
  case 5: {
    Type *EltTy = ST->getElementType(0);
    return ST == hlslOP->GetResRetType(EltTy);
  } break;
  default:
    return false;
  }
}

static bool ValidateType(Type *Ty, ValidationContext &ValCtx) {
  DXASSERT_NOMSG(Ty != nullptr);
  if (Ty->isPointerTy()) {
    return ValidateType(Ty->getPointerElementType(), ValCtx);
  }
  if (Ty->isArrayTy()) {
    Type *EltTy = Ty->getArrayElementType();
    if (isa<ArrayType>(EltTy)) {
      ValCtx.EmitTypeError(Ty, ValidationRule::TypesNoMultiDim);
      return false;
    }
    return ValidateType(EltTy, ValCtx);
  }
  if (Ty->isStructTy()) {
    bool result = true;
    StructType *ST = cast<StructType>(Ty);

    StringRef Name = ST->getName();
    if (Name.startswith("dx.")) {
      hlsl::OP *hlslOP = ValCtx.DxilMod.GetOP();
      if (IsDxilBuiltinStructType(ST, hlslOP)) {
        ValCtx.EmitTypeError(Ty, ValidationRule::InstrDxilStructUser);
        result = false;
      }

      ValCtx.EmitTypeError(Ty, ValidationRule::DeclDxilNsReserved);
      result = false;
    }
    for (auto e : ST->elements()) {
      if (!ValidateType(e, ValCtx)) {
        result = false;
      }
    }
    return result;
  }
  if (Ty->isFloatTy() || Ty->isHalfTy() || Ty->isDoubleTy()) {
    return true;
  }
  if (Ty->isIntegerTy()) {
    unsigned width = Ty->getIntegerBitWidth();
    if (width != 1 && width != 8 && width != 16 && width != 32 && width != 64) {
      ValCtx.EmitTypeError(Ty, ValidationRule::TypesIntWidth);
      return false;
    }
    return true;
  }

  if (Ty->isVectorTy()) {
    ValCtx.EmitTypeError(Ty, ValidationRule::TypesNoVector);
    return false;
  }
  ValCtx.EmitTypeError(Ty, ValidationRule::TypesDefined);
  return false;
}

static bool GetNodeOperandAsInt(ValidationContext &ValCtx, MDNode *pMD, unsigned index, uint64_t *pValue) {
  *pValue = 0;
  if (pMD->getNumOperands() < index) {
    ValCtx.EmitMetaError(pMD, ValidationRule::MetaWellFormed);
    return false;
  }
  ConstantAsMetadata *C = dyn_cast<ConstantAsMetadata>(pMD->getOperand(index));
  if (C == nullptr) {
    ValCtx.EmitMetaError(pMD, ValidationRule::MetaWellFormed);
    return false;
  }
  ConstantInt *CI = dyn_cast<ConstantInt>(C->getValue());
  if (CI == nullptr) {
    ValCtx.EmitMetaError(pMD, ValidationRule::MetaWellFormed);
    return false;
  }
  *pValue = CI->getValue().getZExtValue();
  return true;
}

static bool IsPrecise(Instruction &I, ValidationContext &ValCtx) {
  MDNode *pMD = I.getMetadata(DxilMDHelper::kDxilPreciseAttributeMDName);
  if (pMD == nullptr) {
    return false;
  }
  if (pMD->getNumOperands() != 1) {
    ValCtx.EmitMetaError(pMD, ValidationRule::MetaWellFormed);
    return false;
  }

  uint64_t val;
  if (!GetNodeOperandAsInt(ValCtx, pMD, 0, &val)) {
    return false;
  }
  if (val == 1) {
    return true;
  }
  if (val != 0) {
    ValCtx.EmitMetaError(pMD, ValidationRule::MetaValueRange);
  }
  return false;
}

static bool IsValueMinPrec(DxilModule &DxilMod, Value *V) {
  DXASSERT(DxilMod.GetGlobalFlags() & DXIL::kEnableMinPrecision,
           "else caller didn't check - currently this path should never be hit "
           "otherwise");
  (DxilMod);
  Type *Ty = V->getType();
  if (Ty->isIntegerTy()) {
    return 16 == Ty->getIntegerBitWidth();
  }
  return Ty->isHalfTy();
}

static void ValidateGradientOps(Function *F, ArrayRef<CallInst *> ops, ArrayRef<CallInst *> barriers, ValidationContext &ValCtx) {
  // In the absence of wave operations, the wave validation effect need not happen.
  // We haven't verified this is true at this point, but validation will fail
  // later if the flags don't match in any case. Given that most shaders will
  // not be using these wave operations, it's a reasonable cost saving.
  if (!ValCtx.DxilMod.m_ShaderFlags.GetWaveOps()) {
    return;
  }

  std::unique_ptr<WaveSensitivityAnalysis> WaveVal(WaveSensitivityAnalysis::create());
  WaveVal->Analyze(F);
  for (CallInst *op : ops) {
    if (WaveVal->IsWaveSensitive(op)) {
      ValCtx.EmitInstrError(op, ValidationRule::UniNoWaveSensitiveGradient);
    }
  }
}

static void ValidateControlFlowHint(BasicBlock &bb, ValidationContext &ValCtx) {
  // Validate controlflow hint.
  TerminatorInst *TI = bb.getTerminator();
  if (!TI)
    return;

  MDNode *pNode = TI->getMetadata(DxilMDHelper::kDxilControlFlowHintMDName);
  if (!pNode)
    return;

  if (pNode->getNumOperands() < 3)
    return;

  bool bHasBranch = false;
  bool bHasFlatten = false;
  bool bForceCase = false;

  for (unsigned i = 2; i < pNode->getNumOperands(); i++) {
    uint64_t value = 0;
    if (GetNodeOperandAsInt(ValCtx, pNode, i, &value)) {
      DXIL::ControlFlowHint hint = static_cast<DXIL::ControlFlowHint>(value);
      switch (hint) {
      case DXIL::ControlFlowHint::Flatten:
        bHasFlatten = true;
        break;
      case DXIL::ControlFlowHint::Branch:
        bHasBranch = true;
        break;
      case DXIL::ControlFlowHint::ForceCase:
        bForceCase = true;
        break;
      default:
        ValCtx.EmitMetaError(pNode,
                               ValidationRule::MetaInvalidControlFlowHint);
      }
    }
  }
  if (bHasBranch && bHasFlatten) {
    ValCtx.EmitMetaError(pNode, ValidationRule::MetaBranchFlatten);
  }
  if (bForceCase && !isa<SwitchInst>(TI)) {
    ValCtx.EmitMetaError(pNode, ValidationRule::MetaForceCaseOnSwitch);
  }
}

static void ValidateTBAAMetadata(MDNode *Node, ValidationContext &ValCtx) {
  switch (Node->getNumOperands()) {
  case 1: {
    if (Node->getOperand(0)->getMetadataID() != Metadata::MDStringKind) {
      ValCtx.EmitMetaError(Node, ValidationRule::MetaWellFormed);
    }
  } break;
  case 2: {
    MDNode *rootNode = dyn_cast<MDNode>(Node->getOperand(1));
    if (!rootNode) {
      ValCtx.EmitMetaError(Node, ValidationRule::MetaWellFormed);
    } else {
      ValidateTBAAMetadata(rootNode, ValCtx);
    }
  } break;
  case 3: {
    MDNode *rootNode = dyn_cast<MDNode>(Node->getOperand(1));
    if (!rootNode) {
      ValCtx.EmitMetaError(Node, ValidationRule::MetaWellFormed);
    } else {
      ValidateTBAAMetadata(rootNode, ValCtx);
    }
    ConstantAsMetadata *pointsToConstMem = dyn_cast<ConstantAsMetadata>(Node->getOperand(2));
    if (!pointsToConstMem) {
      ValCtx.EmitMetaError(Node, ValidationRule::MetaWellFormed);
    } else {
      ConstantInt *isConst = dyn_cast<ConstantInt>(pointsToConstMem->getValue());
      if (!isConst) {
        ValCtx.EmitMetaError(Node, ValidationRule::MetaWellFormed);
      } else if (isConst->getValue().getLimitedValue() > 1) {
        ValCtx.EmitMetaError(Node, ValidationRule::MetaWellFormed);
      }
    }
  } break;
  default:
    ValCtx.EmitMetaError(Node, ValidationRule::MetaWellFormed);
  }
}

static void ValidateLoopMetadata(MDNode *Node, ValidationContext &ValCtx) {
  if (Node->getNumOperands() == 0 || Node->getNumOperands() > 2) {
    ValCtx.EmitMetaError(Node, ValidationRule::MetaWellFormed);
    return;
  }
  if (Node != Node->getOperand(0).get()) {
    ValCtx.EmitMetaError(Node, ValidationRule::MetaWellFormed);
    return;
  }
  if (Node->getNumOperands() == 1) {
    return;
  }

  MDNode *LoopNode = dyn_cast<MDNode>(Node->getOperand(1).get());
  if (!LoopNode) {
    ValCtx.EmitMetaError(Node, ValidationRule::MetaWellFormed);
    return;
  }

  if (LoopNode->getNumOperands() < 1 || LoopNode->getNumOperands() > 2) {
    ValCtx.EmitMetaError(LoopNode, ValidationRule::MetaWellFormed);
    return;
  }

  if (LoopNode->getOperand(0) == LoopNode) {
    ValidateLoopMetadata(LoopNode, ValCtx);
    return;
  }

  MDString *LoopStr = dyn_cast<MDString>(LoopNode->getOperand(0));
  if (!LoopStr) {
    ValCtx.EmitMetaError(LoopNode, ValidationRule::MetaWellFormed);
    return;
  }

  StringRef Name = LoopStr->getString();
  if (Name != "llvm.loop.unroll.full" && Name != "llvm.loop.unroll.disable" &&
      Name != "llvm.loop.unroll.count") {
    ValCtx.EmitMetaError(LoopNode, ValidationRule::MetaWellFormed);
    return;
  }

  if (Name == "llvm.loop.unroll.count") {
    if (LoopNode->getNumOperands() != 2) {
      ValCtx.EmitMetaError(LoopNode, ValidationRule::MetaWellFormed);
      return;
    }
    ConstantAsMetadata *CountNode =
        dyn_cast<ConstantAsMetadata>(LoopNode->getOperand(1));
    if (!CountNode) {
      ValCtx.EmitMetaError(LoopNode, ValidationRule::MetaWellFormed);
    } else {
      ConstantInt *Count = dyn_cast<ConstantInt>(CountNode->getValue());
      if (!Count) {
        ValCtx.EmitMetaError(CountNode, ValidationRule::MetaWellFormed);
      }
    }
  }
}

static void ValidateInstructionMetadata(Instruction *I,
                                        ValidationContext &ValCtx) {
  SmallVector<std::pair<unsigned, MDNode *>, 2> MDNodes;
  I->getAllMetadataOtherThanDebugLoc(MDNodes);
  for (auto &MD : MDNodes) {
    if (MD.first == ValCtx.kDxilControlFlowHintMDKind) {
      if (!isa<TerminatorInst>(I)) {
        ValCtx.EmitInstrError(
            I, ValidationRule::MetaControlFlowHintNotOnControlFlow);
      }
    } else if (MD.first == ValCtx.kDxilPreciseMDKind) {
      // Validated in IsPrecise.
    } else if (MD.first == ValCtx.kLLVMLoopMDKind) {
      ValidateLoopMetadata(MD.second, ValCtx);
    } else if (MD.first == LLVMContext::MD_tbaa) {
      ValidateTBAAMetadata(MD.second, ValCtx);
    } else if (MD.first == LLVMContext::MD_range) {
      // Validated in Verifier.cpp.
    } else if (MD.first == LLVMContext::MD_noalias ||
               MD.first == LLVMContext::MD_alias_scope) {
      // noalias for DXIL validator >= 1.2
    } else {
      ValCtx.EmitMetaError(MD.second, ValidationRule::MetaUsed);
    }
  }
}

static void ValidateFunctionAttribute(Function *F, ValidationContext &ValCtx) {
  AttributeSet attrSet = F->getAttributes().getFnAttributes();
  // fp32-denorm-mode
  if (attrSet.hasAttribute(AttributeSet::FunctionIndex,
                           DXIL::kFP32DenormKindString)) {
    Attribute attr = attrSet.getAttribute(AttributeSet::FunctionIndex,
                                          DXIL::kFP32DenormKindString);
    StringRef value = attr.getValueAsString();
    if (!value.equals(DXIL::kFP32DenormValueAnyString) &&
        !value.equals(DXIL::kFP32DenormValueFtzString) &&
        !value.equals(DXIL::kFP32DenormValuePreserveString)) {
      ValCtx.EmitFnAttributeError(F, attr.getKindAsString(),
                                  attr.getValueAsString());
    }
  }
  // TODO: If validating libraries, we should remove all unknown function attributes.
  // For each attribute, check if it is a known attribute
  for (unsigned I = 0, E = attrSet.getNumSlots(); I != E; ++I) {
    for (auto AttrIter = attrSet.begin(I), AttrEnd = attrSet.end(I);
         AttrIter != AttrEnd; ++AttrIter) {
      if (!AttrIter->isStringAttribute()) {
        continue;
      }
      StringRef kind = AttrIter->getKindAsString();
      if (!kind.equals(DXIL::kFP32DenormKindString)) {
        ValCtx.EmitFnAttributeError(F, AttrIter->getKindAsString(),
                                    AttrIter->getValueAsString());
      }
    }
  }
}

static void ValidateFunctionMetadata(Function *F, ValidationContext &ValCtx) {
  SmallVector<std::pair<unsigned, MDNode *>, 2> MDNodes;
  F->getAllMetadata(MDNodes);
  for (auto &MD : MDNodes) {
    ValCtx.EmitMetaError(MD.second, ValidationRule::MetaUsed);
  }
}

static void ValidateFunctionBody(Function *F, ValidationContext &ValCtx) {
  bool SupportsMinPrecision =
      ValCtx.DxilMod.GetGlobalFlags() & DXIL::kEnableMinPrecision;
  SmallVector<CallInst *, 16> gradientOps;
  SmallVector<CallInst *, 16> barriers;
  for (auto b = F->begin(), bend = F->end(); b != bend; ++b) {
    for (auto i = b->begin(), iend = b->end(); i != iend; ++i) {
      llvm::Instruction &I = *i;

      if (I.hasMetadata()) {

        ValidateInstructionMetadata(&I, ValCtx);
      }

      // Instructions must be allowed.
      if (!IsLLVMInstructionAllowed(I)) {
        ValCtx.EmitInstrError(&I, ValidationRule::InstrAllowed);
        continue;
      }

      // Instructions marked precise may not have minprecision arguments.
      if (SupportsMinPrecision) {
        if (IsPrecise(I, ValCtx)) {
          for (auto &O : I.operands()) {
            if (IsValueMinPrec(ValCtx.DxilMod, O)) {
              ValCtx.EmitInstrError(
                  &I, ValidationRule::InstrMinPrecisionNotPrecise);
              break;
            }
          }
        }
      }

      // Calls to external functions.
      CallInst *CI = dyn_cast<CallInst>(&I);
      if (CI) {
        Function *FCalled = CI->getCalledFunction();
        if (FCalled->isDeclaration()) {
          // External function validation will diagnose.
          if (!IsDxilFunction(FCalled)) {
            continue;
          }

          Value *opcodeVal = CI->getOperand(0);
          ConstantInt *OpcodeConst = dyn_cast<ConstantInt>(opcodeVal);
          if (OpcodeConst == nullptr) {
            ValCtx.EmitInstrFormatError(&I, ValidationRule::InstrOpConst,
                                        {"Opcode", "DXIL operation"});
            continue;
          }

          unsigned opcode = OpcodeConst->getLimitedValue();
          DXIL::OpCode dxilOpcode = (DXIL::OpCode)opcode;

          if (OP::IsDxilOpGradient(dxilOpcode)) {
            gradientOps.push_back(CI);
          }

          if (dxilOpcode == DXIL::OpCode::Barrier) {
            barriers.push_back(CI);
          }
          // External function validation will check the parameter
          // list. This function will check that the call does not
          // violate any rules.
        }
        continue;
      }

      for (Value *op : I.operands()) {
        if (!isa<PHINode>(&I) && isa<UndefValue>(op)) {
          ValCtx.EmitInstrError(&I,
                                ValidationRule::InstrNoReadingUninitialized);
        } else if (ConstantExpr *CE = dyn_cast<ConstantExpr>(op)) {
          for (Value *opCE : CE->operands()) {
            if (isa<UndefValue>(opCE)) {
              ValCtx.EmitInstrError(
                  &I, ValidationRule::InstrNoReadingUninitialized);
            }
          }
        }
        if (IntegerType *IT = dyn_cast<IntegerType>(op->getType())) {
          if (IT->getBitWidth() == 8) {
            ValCtx.EmitInstrError(&I, ValidationRule::TypesI8);
          }
        }
      }

      Type *Ty = I.getType();
      if (isa<PointerType>(Ty))
        Ty = Ty->getPointerElementType();
      while (isa<ArrayType>(Ty))
        Ty = Ty->getArrayElementType();
      if (IntegerType *IT = dyn_cast<IntegerType>(Ty)) {
        if (IT->getBitWidth() == 8) {
          ValCtx.EmitInstrError(&I, ValidationRule::TypesI8);
        }
      }

      unsigned opcode = I.getOpcode();
      switch (opcode) {
      case Instruction::Alloca: {
        AllocaInst *AI = cast<AllocaInst>(&I);
        // TODO: validate address space and alignment
        Type *Ty = AI->getAllocatedType();
        if (!ValidateType(Ty, ValCtx)) {
          continue;
        }
      } break;
      case Instruction::ExtractValue: {
        ExtractValueInst *EV = cast<ExtractValueInst>(&I);
        Type *Ty = EV->getAggregateOperand()->getType();
        if (StructType *ST = dyn_cast<StructType>(Ty)) {
          Value *Agg = EV->getAggregateOperand();
          if (!isa<AtomicCmpXchgInst>(Agg) &&
              !IsDxilBuiltinStructType(ST, ValCtx.DxilMod.GetOP())) {
            ValCtx.EmitInstrError(EV, ValidationRule::InstrExtractValue);
          }
        } else {
          ValCtx.EmitInstrError(EV, ValidationRule::InstrExtractValue);
        }
      } break;
      case Instruction::Load: {
        Type *Ty = I.getType();
        if (!ValidateType(Ty, ValCtx)) {
          continue;
        }
      } break;
      case Instruction::Store: {
        StoreInst *SI = cast<StoreInst>(&I);
        Type *Ty = SI->getValueOperand()->getType();
        if (!ValidateType(Ty, ValCtx)) {
          continue;
        }
      } break;
      case Instruction::GetElementPtr: {
        Type *Ty = I.getType()->getPointerElementType();
        if (!ValidateType(Ty, ValCtx)) {
          continue;
        }
        GetElementPtrInst *GEP = cast<GetElementPtrInst>(&I);
        bool allImmIndex = true;
        for (auto Idx = GEP->idx_begin(), E = GEP->idx_end(); Idx != E; Idx++) {
          if (!isa<ConstantInt>(Idx)) {
            allImmIndex = false;
            break;
          }
        }
        if (allImmIndex) {
          const DataLayout &DL = ValCtx.DL;

          Value *Ptr = GEP->getPointerOperand();
          unsigned size =
              DL.getTypeAllocSize(Ptr->getType()->getPointerElementType());
          unsigned valSize = DL.getTypeAllocSize(GEP->getType()->getPointerElementType());

          SmallVector<Value *, 8> Indices(GEP->idx_begin(), GEP->idx_end());
          unsigned offset =
              DL.getIndexedOffset(GEP->getPointerOperandType(), Indices);
          if ((offset + valSize) > size) {
            ValCtx.EmitInstrError(GEP, ValidationRule::InstrInBoundsAccess);
          }
        }
      } break;
      case Instruction::SDiv: {
        BinaryOperator *BO = cast<BinaryOperator>(&I);
        Value *V = BO->getOperand(1);
        if (ConstantInt *imm = dyn_cast<ConstantInt>(V)) {
          if (imm->getValue().getLimitedValue() == 0) {
            ValCtx.EmitInstrError(BO, ValidationRule::InstrNoIDivByZero);
          }
        }
      } break;
      case Instruction::UDiv: {
        BinaryOperator *BO = cast<BinaryOperator>(&I);
        Value *V = BO->getOperand(1);
        if (ConstantInt *imm = dyn_cast<ConstantInt>(V)) {
          if (imm->getValue().getLimitedValue() == 0) {
            ValCtx.EmitInstrError(BO, ValidationRule::InstrNoUDivByZero);
          }
        }
      } break;
      case Instruction::AddrSpaceCast: {
        AddrSpaceCastInst *Cast = cast<AddrSpaceCastInst>(&I);
        unsigned ToAddrSpace = Cast->getType()->getPointerAddressSpace();
        unsigned FromAddrSpace = Cast->getOperand(0)->getType()->getPointerAddressSpace();
        if (ToAddrSpace != DXIL::kGenericPointerAddrSpace &&
            FromAddrSpace != DXIL::kGenericPointerAddrSpace) {
          ValCtx.EmitInstrError(Cast, ValidationRule::InstrNoGenericPtrAddrSpaceCast);
        }
      } break;
      case Instruction::BitCast: {
        BitCastInst *Cast = cast<BitCastInst>(&I);
        Type *FromTy = Cast->getOperand(0)->getType();
        Type *ToTy = Cast->getType();
        if (isa<PointerType>(FromTy)) {
          FromTy = FromTy->getPointerElementType();
          ToTy = ToTy->getPointerElementType();
          unsigned FromSize = ValCtx.DL.getTypeAllocSize(FromTy);
          unsigned ToSize = ValCtx.DL.getTypeAllocSize(ToTy);
          if (FromSize != ToSize) {
            ValCtx.EmitInstrError(Cast, ValidationRule::InstrPtrBitCast);
            continue;
          }
          while (isa<ArrayType>(FromTy)) {
            FromTy = FromTy->getArrayElementType();
          }
          while (isa<ArrayType>(ToTy)) {
            ToTy = ToTy->getArrayElementType();
          }
        }
        if (isa<StructType>(FromTy) || isa<StructType>(ToTy)) {
          ValCtx.EmitInstrError(Cast, ValidationRule::InstrStructBitCast);
          continue;
        }

        bool IsMinPrecisionTy =
            (ValCtx.DL.getTypeStoreSize(FromTy) < 4 ||
             ValCtx.DL.getTypeStoreSize(ToTy) < 4) &&
            !ValCtx.DxilMod.m_ShaderFlags.GetUseNativeLowPrecision();
        if (IsMinPrecisionTy) {
          ValCtx.EmitInstrError(Cast, ValidationRule::InstrMinPrecisonBitCast);
        }
      } break;
      }

      if (PointerType *PT = dyn_cast<PointerType>(I.getType())) {
        if (PT->getAddressSpace() == DXIL::kTGSMAddrSpace) {
          if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I)) {
            Value *Ptr = GEP->getPointerOperand();
            if (!isa<GlobalVariable>(Ptr)) {
              ValCtx.EmitInstrError(
                  &I, ValidationRule::InstrFailToResloveTGSMPointer);
            }
          } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(&I)) {
            Value *Ptr = BCI->getOperand(0);
            if (!isa<GetElementPtrInst>(Ptr) && !isa<GlobalVariable>(Ptr)) {
              ValCtx.EmitInstrError(
                  &I, ValidationRule::InstrFailToResloveTGSMPointer);
            }
          } else {
            ValCtx.EmitInstrError(
                &I, ValidationRule::InstrFailToResloveTGSMPointer);
          }
        }
      }
    }
    ValidateControlFlowHint(*b, ValCtx);
  }

  if (!gradientOps.empty()) {
    ValidateGradientOps(F, gradientOps, barriers, ValCtx);
  }
}

static void ValidateFunction(Function &F, ValidationContext &ValCtx) {
  if (F.isDeclaration()) {
    ValidateExternalFunction(&F, ValCtx);
  } else {
    if (!F.arg_empty())
      ValCtx.EmitFormatError(ValidationRule::FlowFunctionCall,
                             {F.getName().str()});

    DxilFunctionAnnotation *funcAnnotation =
        ValCtx.DxilMod.GetTypeSystem().GetFunctionAnnotation(&F);
    if (!funcAnnotation) {
      ValCtx.EmitFormatError(ValidationRule::MetaFunctionAnnotation,
                             {F.getName().str()});
      return;
    }

    // Validate parameter type.
    for (auto &arg : F.args()) {
      Type *argTy = arg.getType();
      if (argTy->isPointerTy())
        argTy = argTy->getPointerElementType();
      while (argTy->isArrayTy()) {
        argTy = argTy->getArrayElementType();
      }

      if (argTy->isStructTy()) {
        if (arg.hasName())
          ValCtx.EmitFormatError(
              ValidationRule::DeclFnFlattenParam,
              {arg.getName().str(), F.getName().str()});
        else
          ValCtx.EmitFormatError(ValidationRule::DeclFnFlattenParam,
                                 {std::to_string(arg.getArgNo()),
                                  F.getName().str()});
        break;
      }
    }

    ValidateFunctionBody(&F, ValCtx);
  }

  ValidateFunctionAttribute(&F, ValCtx);

  if (F.hasMetadata()) {
    ValidateFunctionMetadata(&F, ValCtx);
  }
}

static void ValidateGlobalVariable(GlobalVariable &GV,
                                   ValidationContext &ValCtx) {
  bool isInternalGV =
      dxilutil::IsStaticGlobal(&GV) || dxilutil::IsSharedMemoryGlobal(&GV);

  if (!isInternalGV) {
    if (!GV.user_empty()) {
      bool hasInstructionUser = false;
      for (User *U : GV.users()) {
        if (isa<Instruction>(U)) {
          hasInstructionUser = true;
          break;
        }
      }
      // External GV should not have instruction user.
      if (hasInstructionUser) {
        ValCtx.EmitGlobalValueError(&GV, ValidationRule::DeclNotUsedExternal);
      }
    }
    // Must have metadata description for each variable.

  } else {
    // Internal GV must have user.
    if (GV.user_empty()) {
      ValCtx.EmitGlobalValueError(&GV, ValidationRule::DeclUsedInternal);
    }

    // Validate type for internal globals.
    if (dxilutil::IsStaticGlobal(&GV) || dxilutil::IsSharedMemoryGlobal(&GV)) {
      Type *Ty = GV.getType()->getPointerElementType();
      ValidateType(Ty, ValCtx);
    }
  }
}

static void CollectFixAddressAccess(Value *V,
                                    std::vector<StoreInst *> &fixAddrTGSMList) {
  for (User *U : V->users()) {
    if (GEPOperator *GEP = dyn_cast<GEPOperator>(U)) {
      if (isa<ConstantExpr>(GEP) || GEP->hasAllConstantIndices()) {
        CollectFixAddressAccess(GEP, fixAddrTGSMList);
      }
    } else if (StoreInst *SI = dyn_cast<StoreInst>(U)) {
      fixAddrTGSMList.emplace_back(SI);
    }
  }
}

static bool IsDivergent(Value *V) {
  // TODO: return correct result.
  return false;
}

static void ValidateTGSMRaceCondition(std::vector<StoreInst *> &fixAddrTGSMList,
                                      ValidationContext &ValCtx) {
  std::unordered_set<Function *> fixAddrTGSMFuncSet;
  for (StoreInst *I : fixAddrTGSMList) {
    BasicBlock *BB = I->getParent();
    fixAddrTGSMFuncSet.insert(BB->getParent());
  }

  for (auto &F : ValCtx.DxilMod.GetModule()->functions()) {
    if (F.isDeclaration() || !fixAddrTGSMFuncSet.count(&F))
      continue;

    PostDominatorTree PDT;
    PDT.runOnFunction(F);

    BasicBlock *Entry = &F.getEntryBlock();

    for (StoreInst *SI : fixAddrTGSMList) {
      BasicBlock *BB = SI->getParent();
      if (BB->getParent() == &F) {
        if (PDT.dominates(BB, Entry)) {
          if (IsDivergent(SI->getValueOperand()))
            ValCtx.EmitInstrError(SI, ValidationRule::InstrTGSMRaceCond);
        }
      }
    }
  }
}

static void ValidateGlobalVariables(ValidationContext &ValCtx) {
  DxilModule &M = ValCtx.DxilMod;

  unsigned TGSMSize = 0;
  std::vector<StoreInst*> fixAddrTGSMList;
  const DataLayout &DL = M.GetModule()->getDataLayout();
  for (GlobalVariable &GV : M.GetModule()->globals()) {
    ValidateGlobalVariable(GV, ValCtx);
    if (GV.getType()->getAddressSpace() == DXIL::kTGSMAddrSpace) {
      TGSMSize += DL.getTypeAllocSize(GV.getType()->getElementType());
      CollectFixAddressAccess(&GV, fixAddrTGSMList);
    }
  }

  if (TGSMSize > DXIL::kMaxTGSMSize) {
    ValCtx.EmitFormatError(ValidationRule::SmMaxTGSMSize,
                           {std::to_string(TGSMSize),
                            std::to_string(DXIL::kMaxTGSMSize)});
  }
  if (!fixAddrTGSMList.empty()) {
    ValidateTGSMRaceCondition(fixAddrTGSMList, ValCtx);
  }
}

static void ValidateValidatorVersion(ValidationContext &ValCtx) {
  Module *pModule = &ValCtx.M;
  NamedMDNode *pNode = pModule->getNamedMetadata("dx.valver");
  if (pNode == nullptr) {
    return;
  }
  if (pNode->getNumOperands() == 1) {
    MDTuple *pVerValues = dyn_cast<MDTuple>(pNode->getOperand(0));
    if (pVerValues != nullptr && pVerValues->getNumOperands() == 2) {
      uint64_t majorVer, minorVer;
      if (GetNodeOperandAsInt(ValCtx, pVerValues, 0, &majorVer) &&
          GetNodeOperandAsInt(ValCtx, pVerValues, 1, &minorVer)) {
        unsigned curMajor, curMinor;
        GetValidationVersion(&curMajor, &curMinor);
        // This will need to be updated as major/minor versions evolve,
        // depending on the degree of compat across versions.
        if (majorVer == curMajor && minorVer <= curMinor) {
          return;
        }
      }
    }
  }
  ValCtx.EmitError(ValidationRule::MetaWellFormed);
}

static void ValidateDxilVersion(ValidationContext &ValCtx) {
  Module *pModule = &ValCtx.M;
  NamedMDNode *pNode = pModule->getNamedMetadata("dx.version");
  if (pNode && pNode->getNumOperands() == 1) {
    MDTuple *pVerValues = dyn_cast<MDTuple>(pNode->getOperand(0));
    if (pVerValues != nullptr && pVerValues->getNumOperands() == 2) {
      uint64_t majorVer, minorVer;
      if (GetNodeOperandAsInt(ValCtx, pVerValues, 0, &majorVer) &&
          GetNodeOperandAsInt(ValCtx, pVerValues, 1, &minorVer)) {
        // This will need to be updated as dxil major/minor versions evolve,
        // depending on the degree of compat across versions.
        if ((majorVer == 1 && minorVer < 4) &&
            (majorVer == ValCtx.m_DxilMajor && minorVer == ValCtx.m_DxilMinor)) {
          return;
        }
      }
    }
  }
  ValCtx.EmitError(ValidationRule::MetaWellFormed);
}

static void ValidateTypeAnnotation(ValidationContext &ValCtx) {
  if (ValCtx.m_DxilMajor == 1 && ValCtx.m_DxilMinor >= 2) {
    Module *pModule = &ValCtx.M;
    NamedMDNode *TA = pModule->getNamedMetadata("dx.typeAnnotations");
    if (TA == nullptr)
      return;
    for (unsigned i = 0, end = TA->getNumOperands(); i < end; ++i) {
      MDTuple *TANode = dyn_cast<MDTuple>(TA->getOperand(i));
      if (TANode->getNumOperands() < 3) {
        ValCtx.EmitMetaError(TANode, ValidationRule::MetaWellFormed);
        return;
      }
      ConstantInt *tag = mdconst::extract<ConstantInt>(TANode->getOperand(0));
      uint64_t tagValue = tag->getZExtValue();
      if (tagValue != DxilMDHelper::kDxilTypeSystemStructTag &&
          tagValue != DxilMDHelper::kDxilTypeSystemFunctionTag) {
          ValCtx.EmitMetaError(TANode, ValidationRule::MetaWellFormed);
          return;
      }
    }
  }
}

static void ValidateMetadata(ValidationContext &ValCtx) {
  Module *pModule = &ValCtx.M;
  const std::string &target = pModule->getTargetTriple();
  if (target != "dxil-ms-dx") {
    ValCtx.EmitFormatError(ValidationRule::MetaTarget, {target});
  }

  // The llvm.dbg.(cu/contents/defines/mainFileName/arg) named metadata nodes
  // are only available in debug modules, not in the validated ones.
  // llvm.bitsets is also disallowed.
  //
  // These are verified in lib/IR/Verifier.cpp.
  StringMap<bool> llvmNamedMeta;
  llvmNamedMeta["llvm.ident"];
  llvmNamedMeta["llvm.module.flags"];

  for (auto &NamedMetaNode : pModule->named_metadata()) {
    if (!DxilModule::IsKnownNamedMetaData(NamedMetaNode)) {
      StringRef name = NamedMetaNode.getName();
      if (!name.startswith_lower("llvm."))
        ValCtx.EmitFormatError(ValidationRule::MetaKnown, {name.str()});
      else {
        if (llvmNamedMeta.count(name) == 0) {
          ValCtx.EmitFormatError(ValidationRule::MetaKnown,
                                 {name.str()});
        }
      }
    }
  }

  const hlsl::ShaderModel *SM = ValCtx.DxilMod.GetShaderModel();
  if (!SM->IsValidForDxil()) {
    ValCtx.EmitFormatError(ValidationRule::SmName,
                           {ValCtx.DxilMod.GetShaderModel()->GetName()});
  }

  if (SM->GetMajor() == 6) {
    // Make sure DxilVersion matches the shader model.
    unsigned SMDxilMajor, SMDxilMinor;
    SM->GetDxilVersion(SMDxilMajor, SMDxilMinor);
    if (ValCtx.m_DxilMajor != SMDxilMajor || ValCtx.m_DxilMinor != SMDxilMinor) {
      ValCtx.EmitFormatError(ValidationRule::SmDxilVersion,
                             {std::to_string(SMDxilMajor),
                              std::to_string(SMDxilMinor)});
    }
  }

  ValidateDxilVersion(ValCtx);
  ValidateValidatorVersion(ValCtx);
  ValidateTypeAnnotation(ValCtx);
}

static void ValidateResourceOverlap(
    hlsl::DxilResourceBase &res,
    SpacesAllocator<unsigned, DxilResourceBase> &spaceAllocator,
    ValidationContext &ValCtx) {
  unsigned base = res.GetLowerBound();
  unsigned size = res.GetRangeSize();
  unsigned space = res.GetSpaceID();

  auto &allocator = spaceAllocator.Get(space);
  unsigned end = base + size - 1;
  // unbounded
  if (end < base)
    end = size;
  const DxilResourceBase *conflictRes = allocator.Insert(&res, base, end);
  if (conflictRes) {
    ValCtx.EmitFormatError(
        ValidationRule::SmResourceRangeOverlap,
        {res.GetGlobalName(), std::to_string(base),
         std::to_string(size),
         std::to_string(conflictRes->GetLowerBound()),
         std::to_string(conflictRes->GetRangeSize()),
         std::to_string(space)});
  }
}

static void ValidateResource(hlsl::DxilResource &res,
                             ValidationContext &ValCtx) {
  switch (res.GetKind()) {
  case DXIL::ResourceKind::RawBuffer:
  case DXIL::ResourceKind::TypedBuffer:
  case DXIL::ResourceKind::TBuffer:
  case DXIL::ResourceKind::StructuredBuffer:
  case DXIL::ResourceKind::Texture1D:
  case DXIL::ResourceKind::Texture1DArray:
  case DXIL::ResourceKind::Texture2D:
  case DXIL::ResourceKind::Texture2DArray:
  case DXIL::ResourceKind::Texture3D:
  case DXIL::ResourceKind::TextureCube:
  case DXIL::ResourceKind::TextureCubeArray:
    if (res.GetSampleCount() > 0) {
      ValCtx.EmitResourceError(&res, ValidationRule::SmSampleCountOnlyOn2DMS);
    }
    break;
  case DXIL::ResourceKind::Texture2DMS:
  case DXIL::ResourceKind::Texture2DMSArray:
    break;
  default:
    ValCtx.EmitResourceError(&res, ValidationRule::SmInvalidResourceKind);
    break;
  }

  switch (res.GetCompType().GetKind()) {
  case DXIL::ComponentType::F32:
  case DXIL::ComponentType::SNormF32:
  case DXIL::ComponentType::UNormF32:
  case DXIL::ComponentType::F64:
  case DXIL::ComponentType::I32:
  case DXIL::ComponentType::I64:
  case DXIL::ComponentType::U32:
  case DXIL::ComponentType::U64:
  case DXIL::ComponentType::F16:
  case DXIL::ComponentType::I16:
  case DXIL::ComponentType::U16:
    break;
  default:
    if (!res.IsStructuredBuffer() && !res.IsRawBuffer())
      ValCtx.EmitResourceError(&res, ValidationRule::SmInvalidResourceCompType);
    break;
  }

  if (res.IsStructuredBuffer()) {
    unsigned stride = res.GetElementStride();
    bool alignedTo4Bytes = (stride & 3) == 0;
    if (!alignedTo4Bytes && !ValCtx.M.GetDxilModule().m_ShaderFlags.GetUseNativeLowPrecision()) {
      ValCtx.EmitResourceFormatError(
          &res, ValidationRule::MetaStructBufAlignment,
          {std::to_string(4), std::to_string(stride)});
    }
    if (stride > DXIL::kMaxStructBufferStride) {
      ValCtx.EmitResourceFormatError(
          &res, ValidationRule::MetaStructBufAlignmentOutOfBound,
          {std::to_string(DXIL::kMaxStructBufferStride),
           std::to_string(stride)});
    }
  }

  if (res.IsAnyTexture() || res.IsTypedBuffer()) {
    Type *RetTy = res.GetRetType();
    unsigned size = ValCtx.DxilMod.GetModule()->getDataLayout().getTypeAllocSize(RetTy);
    if (size > 4*4) {
      ValCtx.EmitResourceError(&res, ValidationRule::MetaTextureType);
    }
  }
}

static void
CollectCBufferRanges(DxilStructAnnotation *annotation,
                     SpanAllocator<unsigned, DxilFieldAnnotation> &constAllocator,
                     unsigned base, DxilTypeSystem &typeSys, StringRef cbName,
                     ValidationContext &ValCtx) {
  unsigned cbSize = annotation->GetCBufferSize();

  const StructType *ST = annotation->GetStructType();

  for (int i = annotation->GetNumFields() - 1; i >= 0; i--) {
    DxilFieldAnnotation &fieldAnnotation = annotation->GetFieldAnnotation(i);
    Type *EltTy = ST->getElementType(i);

    unsigned offset = fieldAnnotation.GetCBufferOffset();

    unsigned EltSize = dxilutil::GetLegacyCBufferFieldElementSize(
        fieldAnnotation, EltTy, typeSys);

    bool bOutOfBound = false;
    if (!EltTy->isAggregateType()) {
      bOutOfBound = (offset + EltSize) > cbSize;
      if (!bOutOfBound) {
        if (constAllocator.Insert(&fieldAnnotation, base + offset,
                                  base + offset + EltSize - 1)) {
          ValCtx.EmitFormatError(
              ValidationRule::SmCBufferOffsetOverlap,
              {cbName, std::to_string(base + offset)});
        }
      }
    } else if (isa<ArrayType>(EltTy)) {
      unsigned arrayCount = 1;
      while (isa<ArrayType>(EltTy)) {
        arrayCount *= EltTy->getArrayNumElements();
        EltTy = EltTy->getArrayElementType();
      }
      unsigned arrayBase = base + offset;

      DxilStructAnnotation *EltAnnotation = nullptr;
      if (StructType *EltST = dyn_cast<StructType>(EltTy))
        EltAnnotation = typeSys.GetStructAnnotation(EltST);

      for (unsigned idx = 0; idx < arrayCount; idx++) {
        // 16 bytes align except last component.
        if (idx < (arrayCount - 1)) {
          arrayBase = (arrayBase + 15) & ~(0xf);
        }

        if (arrayBase > (base + cbSize)) {
          bOutOfBound = true;
          break;
        }

        if (!EltAnnotation) {
          if (constAllocator.Insert(&fieldAnnotation, arrayBase,
                                    arrayBase + EltSize - 1)) {
            ValCtx.EmitFormatError(
                ValidationRule::SmCBufferOffsetOverlap,
                {cbName, std::to_string(base + offset)});
          }

        } else {
          CollectCBufferRanges(EltAnnotation,
              constAllocator, arrayBase, typeSys,
                               cbName, ValCtx);
        }
        arrayBase += EltSize;
      }
    } else {
      cast<StructType>(EltTy);
      bOutOfBound = (offset + EltSize) > cbSize;
    }

    if (bOutOfBound) {
      ValCtx.EmitFormatError(ValidationRule::SmCBufferElementOverflow,
                             {cbName, std::to_string(base + offset)});
    }
  }
}

static void ValidateCBuffer(DxilCBuffer &cb, ValidationContext &ValCtx) {
  Type *Ty = cb.GetGlobalSymbol()->getType()->getPointerElementType();
  if (cb.GetRangeSize() != 1) {
    Ty = Ty->getArrayElementType();
  }
  if (!isa<StructType>(Ty)) {
    ValCtx.EmitResourceError(&cb,
                             ValidationRule::SmCBufferTemplateTypeMustBeStruct);
    return;
  }
  StructType *ST = cast<StructType>(Ty);
  DxilTypeSystem &typeSys = ValCtx.DxilMod.GetTypeSystem();
  DxilStructAnnotation *annotation = typeSys.GetStructAnnotation(ST);
  if (!annotation)
    return;

  // Collect constant ranges.
  std::vector<std::pair<unsigned, unsigned>> constRanges;
  SpanAllocator<unsigned, DxilFieldAnnotation> constAllocator(0,
      // 4096 * 16 bytes.
      DXIL::kMaxCBufferSize << 4);
  CollectCBufferRanges(annotation, constAllocator,
                       0, typeSys,
                       cb.GetGlobalName(), ValCtx);
}

static void ValidateResources(ValidationContext &ValCtx) {
  const vector<unique_ptr<DxilResource>> &uavs = ValCtx.DxilMod.GetUAVs();
  bool hasROV = false;
  SpacesAllocator<unsigned, DxilResourceBase> uavAllocator;

  for (auto &uav : uavs) {
    if (uav->IsROV()) {
      hasROV = true;
      if (!ValCtx.DxilMod.GetShaderModel()->IsPS()) {
        ValCtx.EmitResourceError(uav.get(), ValidationRule::SmROVOnlyInPS);
      }
    }
    switch (uav->GetKind()) {
    case DXIL::ResourceKind::Texture2DMS:
    case DXIL::ResourceKind::Texture2DMSArray:
    case DXIL::ResourceKind::TextureCube:
    case DXIL::ResourceKind::TextureCubeArray:
      ValCtx.EmitResourceError(uav.get(),
                               ValidationRule::SmInvalidTextureKindOnUAV);
      break;
    default:
      break;
    }

    if (uav->HasCounter() && !uav->IsStructuredBuffer()) {
      ValCtx.EmitResourceError(uav.get(),
                               ValidationRule::SmCounterOnlyOnStructBuf);
    }
    if (uav->HasCounter() && uav->IsGloballyCoherent())
      ValCtx.EmitResourceError(uav.get(),
                               ValidationRule::MetaGlcNotOnAppendConsume);

    ValidateResource(*uav, ValCtx);
    ValidateResourceOverlap(*uav, uavAllocator, ValCtx);
  }

  SpacesAllocator<unsigned, DxilResourceBase> srvAllocator;
  const vector<unique_ptr<DxilResource>> &srvs = ValCtx.DxilMod.GetSRVs();
  for (auto &srv : srvs) {
    ValidateResource(*srv, ValCtx);
    ValidateResourceOverlap(*srv, srvAllocator, ValCtx);
  }

  hlsl::DxilResourceBase *pNonDense;
  if (!AreDxilResourcesDense(&ValCtx.M, &pNonDense)) {
    ValCtx.EmitResourceError(pNonDense, ValidationRule::MetaDenseResIDs);
  }

  SpacesAllocator<unsigned, DxilResourceBase> samplerAllocator;
  for (auto &sampler : ValCtx.DxilMod.GetSamplers()) {
    if (sampler->GetSamplerKind() == DXIL::SamplerKind::Invalid) {
      ValCtx.EmitResourceError(sampler.get(),
                               ValidationRule::MetaValidSamplerMode);
    }
    ValidateResourceOverlap(*sampler, samplerAllocator, ValCtx);
  }

  SpacesAllocator<unsigned, DxilResourceBase> cbufferAllocator;
  for (auto &cbuffer : ValCtx.DxilMod.GetCBuffers()) {
    ValidateCBuffer(*cbuffer, ValCtx);
    ValidateResourceOverlap(*cbuffer, cbufferAllocator, ValCtx);
  }
}

static void ValidateShaderFlags(ValidationContext &ValCtx) {
  DxilModule::ShaderFlags calcFlags;
  ValCtx.DxilMod.CollectShaderFlags(calcFlags);
  const uint64_t mask = DxilModule::ShaderFlags::GetShaderFlagsRawForCollection();
  uint64_t declaredFlagsRaw = ValCtx.DxilMod.m_ShaderFlags.GetShaderFlagsRaw();
  uint64_t calcFlagsRaw = calcFlags.GetShaderFlagsRaw();

  declaredFlagsRaw &= mask;
  calcFlagsRaw &= mask;

  if (declaredFlagsRaw == calcFlagsRaw) {
    return;
  }

  ValCtx.EmitError(ValidationRule::MetaFlagsUsage);
  ValCtx.DiagStream() << "Flags declared=" << declaredFlagsRaw
                      << ", actual=" << calcFlagsRaw << "\n";
}

static void ValidateSignatureElement(DxilSignatureElement &SE,
                                     ValidationContext &ValCtx) {
  DXIL::SemanticKind semanticKind = SE.GetSemantic()->GetKind();
  CompType::Kind compKind = SE.GetCompType().GetKind();
  DXIL::InterpolationMode Mode = SE.GetInterpolationMode()->GetKind();

  StringRef Name = SE.GetName();
  if (Name.size() < 1 || Name.size() > 64) {
    ValCtx.EmitSignatureError(&SE, ValidationRule::MetaSemanticLen);
  }

  if (semanticKind > DXIL::SemanticKind::Arbitrary && semanticKind < DXIL::SemanticKind::Invalid) {
    if (semanticKind != Semantic::GetByName(SE.GetName())->GetKind()) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemaKindMatchesName,
                             {SE.GetName(), SE.GetSemantic()->GetName()});
    }
  }

  unsigned compWidth = 0;
  bool compFloat = false;
  bool compInt = false;
  bool compUnsigned = false;
  bool compBool = false;
  bool compSNorm = false;
  bool compUNorm = false;

  switch (compKind) {
  case CompType::Kind::U64: compWidth = 64; compInt = true; compUnsigned = true; break;
  case CompType::Kind::I64: compWidth = 64; compInt = true; break;
  case CompType::Kind::U32: compWidth = 32; compInt = true; compUnsigned = true; break;
  case CompType::Kind::I32: compWidth = 32; compInt = true; break;
  case CompType::Kind::U16: compWidth = 16; compInt = true; compUnsigned = true; break;
  case CompType::Kind::I16: compWidth = 16; compInt = true; break;
  case CompType::Kind::I1: compWidth = 1; compBool = true; break;
  case CompType::Kind::F64: compWidth = 64; compFloat = true; break;
  case CompType::Kind::F32: compWidth = 32; compFloat = true; break;
  case CompType::Kind::F16: compWidth = 16; compFloat = true; break;
  case CompType::Kind::SNormF64: compWidth = 64; compFloat = true; compSNorm = true; break;
  case CompType::Kind::SNormF32: compWidth = 32; compFloat = true; compSNorm = true; break;
  case CompType::Kind::SNormF16: compWidth = 16; compFloat = true; compSNorm = true; break;
  case CompType::Kind::UNormF64: compWidth = 64; compFloat = true; compUNorm = true; break;
  case CompType::Kind::UNormF32: compWidth = 32; compFloat = true; compUNorm = true; break;
  case CompType::Kind::UNormF16: compWidth = 16; compFloat = true; compUNorm = true; break;
  case CompType::Kind::Invalid:
  default:
    ValCtx.EmitFormatError(ValidationRule::MetaSignatureCompType, { SE.GetName() });
    break;
  }

  if (compInt || compBool) {
    switch (Mode) {
    case DXIL::InterpolationMode::Linear:
    case DXIL::InterpolationMode::LinearCentroid:
    case DXIL::InterpolationMode::LinearNoperspective:
    case DXIL::InterpolationMode::LinearNoperspectiveCentroid:
    case DXIL::InterpolationMode::LinearSample:
    case DXIL::InterpolationMode::LinearNoperspectiveSample: {
      ValCtx.EmitFormatError(ValidationRule::MetaIntegerInterpMode, {SE.GetName()});
    } break;
    default:
      break;
    }
  }

  // Elements that should not appear in the Dxil signature:
  bool bAllowedInSig = true;
  bool bShouldBeAllocated = true;
  switch (SE.GetInterpretation()) {
  case DXIL::SemanticInterpretationKind::NA:
  case DXIL::SemanticInterpretationKind::NotInSig:
  case DXIL::SemanticInterpretationKind::Invalid:
    bAllowedInSig = false;
    __fallthrough;
  case DXIL::SemanticInterpretationKind::NotPacked:
  case DXIL::SemanticInterpretationKind::Shadow:
    bShouldBeAllocated = false;
    break;
  default:
    break;
  }

  const char *inputOutput = nullptr;
  if (SE.IsInput())
    inputOutput = "Input";
  else if (SE.IsOutput())
    inputOutput = "Output";
  else
    inputOutput = "PatchConstant";

  if (!bAllowedInSig) {
    ValCtx.EmitFormatError(
        ValidationRule::SmSemantic,
        {SE.GetName(), ValCtx.DxilMod.GetShaderModel()->GetKindName(), inputOutput});
  } else if (bShouldBeAllocated && !SE.IsAllocated()) {
    ValCtx.EmitFormatError(ValidationRule::MetaSemanticShouldBeAllocated,
      {inputOutput, SE.GetName()});
  } else if (!bShouldBeAllocated && SE.IsAllocated()) {
    ValCtx.EmitFormatError(ValidationRule::MetaSemanticShouldNotBeAllocated,
      {inputOutput, SE.GetName()});
  }

  bool bIsClipCull = false;
  bool bIsTessfactor = false;
  bool bIsBarycentric = false;

  switch (semanticKind) {
  case DXIL::SemanticKind::Depth:
  case DXIL::SemanticKind::DepthGreaterEqual:
  case DXIL::SemanticKind::DepthLessEqual:
    if (!compFloat || compWidth > 32 || SE.GetCols() != 1) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticCompType,
                             {SE.GetSemantic()->GetName(), "float"});
    }
    break;
  case DXIL::SemanticKind::Coverage:
    DXASSERT(!SE.IsInput() || !bAllowedInSig, "else internal inconsistency between semantic interpretation table and validation code");
    __fallthrough;
  case DXIL::SemanticKind::InnerCoverage:
  case DXIL::SemanticKind::OutputControlPointID:
    if (compKind != CompType::Kind::U32 || SE.GetCols() != 1) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticCompType,
                             {SE.GetSemantic()->GetName(), "uint"});
    }
    break;
  case DXIL::SemanticKind::Position:
    if (!compFloat || compWidth > 32 || SE.GetCols() != 4) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticCompType,
                             {SE.GetSemantic()->GetName(), "float4"});
    }
    break;
  case DXIL::SemanticKind::Target:
    if (compWidth > 32) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticCompType,
                             {SE.GetSemantic()->GetName(), "float/int/uint"});
    }
    break;
  case DXIL::SemanticKind::ClipDistance:
  case DXIL::SemanticKind::CullDistance:
    bIsClipCull = true;
    if (!compFloat || compWidth > 32) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticCompType,
                             {SE.GetSemantic()->GetName(), "float"});
    }
    // NOTE: clip cull distance size is checked at ValidateSignature.
    break;
  case DXIL::SemanticKind::IsFrontFace: {
    if (!(compInt && compWidth == 32) || SE.GetCols() != 1) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticCompType,
                             {SE.GetSemantic()->GetName(), "uint"});
    }
  } break;
  case DXIL::SemanticKind::RenderTargetArrayIndex:
  case DXIL::SemanticKind::ViewPortArrayIndex:
  case DXIL::SemanticKind::VertexID:
  case DXIL::SemanticKind::PrimitiveID:
  case DXIL::SemanticKind::InstanceID:
  case DXIL::SemanticKind::GSInstanceID:
  case DXIL::SemanticKind::SampleIndex:
  case DXIL::SemanticKind::StencilRef:
    if ((compKind != CompType::Kind::U32 && compKind != CompType::Kind::U16) || SE.GetCols() != 1) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticCompType,
                             {SE.GetSemantic()->GetName(), "uint"});
    }
    break;
  case DXIL::SemanticKind::TessFactor:
  case DXIL::SemanticKind::InsideTessFactor:
    // NOTE: the size check is at CheckPatchConstantSemantic.
    bIsTessfactor = true;
    if (!compFloat || compWidth > 32) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticCompType,
                             {SE.GetSemantic()->GetName(), "float"});
    }
    break;
  case DXIL::SemanticKind::Arbitrary:
    break;
  case DXIL::SemanticKind::DomainLocation:
  case DXIL::SemanticKind::Invalid:
    DXASSERT(!bAllowedInSig, "else internal inconsistency between semantic interpretation table and validation code");
    break;
  case DXIL::SemanticKind::Barycentrics:
    bIsBarycentric = true;
    if (!compFloat || compWidth > 32) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticCompType, {SE.GetSemantic()->GetName(), "float"});
    }
    if (Mode != InterpolationMode::Kind::Linear &&
        Mode != InterpolationMode::Kind::LinearCentroid &&
        Mode != InterpolationMode::Kind::LinearNoperspective &&
        Mode != InterpolationMode::Kind::LinearNoperspectiveCentroid &&
        Mode != InterpolationMode::Kind::LinearNoperspectiveSample &&
        Mode != InterpolationMode::Kind::LinearSample) {
      ValCtx.EmitSignatureError(&SE, ValidationRule::MetaBarycentricsInterpolation);
    }
    if (SE.GetCols() != 3) {
      ValCtx.EmitSignatureError(&SE, ValidationRule::MetaBarycentricsFloat3);
    }
    break;
  default:
    ValCtx.EmitSignatureError(&SE, ValidationRule::MetaSemaKindValid);
    break;
  }

  if (ValCtx.DxilMod.GetShaderModel()->IsGS() && SE.IsOutput()) {
    if (SE.GetOutputStream() >= DXIL::kNumOutputStreams) {
      ValCtx.EmitFormatError(ValidationRule::SmStreamIndexRange,
                             {std::to_string(SE.GetOutputStream()),
                              std::to_string(DXIL::kNumOutputStreams - 1)});
    }
  } else {
    if (SE.GetOutputStream() > 0) {
      ValCtx.EmitFormatError(ValidationRule::SmStreamIndexRange,
                             {std::to_string(SE.GetOutputStream()),
                              "0"});
    }
  }

  if (ValCtx.DxilMod.GetShaderModel()->IsGS()) {
    if (SE.GetOutputStream() != 0) {
      if (ValCtx.DxilMod.GetStreamPrimitiveTopology() !=
          DXIL::PrimitiveTopology::PointList) {
        ValCtx.EmitSignatureError(&SE,
                                  ValidationRule::SmMultiStreamMustBePoint);
      }
    }
  }

  if (semanticKind == DXIL::SemanticKind::Target) {
    // Verify packed row == semantic index
    unsigned row = SE.GetStartRow();
    for (unsigned i : SE.GetSemanticIndexVec()) {
      if (row != i) {
        ValCtx.EmitSignatureError(&SE, ValidationRule::SmPSTargetIndexMatchesRow);
      }
      ++row;
    }
    // Verify packed col is 0
    if (SE.GetStartCol() != 0) {
      ValCtx.EmitSignatureError(&SE, ValidationRule::SmPSTargetCol0);
    }
    // Verify max row used < 8
    if (SE.GetStartRow() + SE.GetRows() > 8) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticIndexMax, {"SV_Target", "7"});
    }
  } else if (bAllowedInSig && semanticKind != DXIL::SemanticKind::Arbitrary) {
    if (bIsBarycentric) {
      if (SE.GetSemanticStartIndex() > 1) {
        ValCtx.EmitFormatError(ValidationRule::MetaSemanticIndexMax, { SE.GetSemantic()->GetName(), "1" });
      }
    }
    else if (!bIsClipCull && SE.GetSemanticStartIndex() > 0) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticIndexMax, {SE.GetSemantic()->GetName(), "0"});
    }
    // Maximum rows is 1 for system values other than Target
    // with the exception of tessfactors, which are validated in CheckPatchConstantSemantic
    if (!bIsTessfactor && SE.GetRows() > 1) {
      ValCtx.EmitSignatureError(&SE, ValidationRule::MetaSystemValueRows);
    }
  }

  if (SE.GetCols() + (SE.IsAllocated() ? SE.GetStartCol() : 0) > 4) {
    unsigned size = (SE.GetRows() - 1) * 4 + SE.GetCols();
    ValCtx.EmitFormatError(ValidationRule::MetaSignatureOutOfRange,
                            {SE.GetName(),
                            std::to_string(SE.GetStartRow()),
                            std::to_string(SE.GetStartCol()),
                            std::to_string(size)});
  }

  if (!SE.GetInterpolationMode()->IsValid()) {
    ValCtx.EmitSignatureError(&SE, ValidationRule::MetaInterpModeValid);
  }
}

static void ValidateSignatureOverlap(
    DxilSignatureElement &E, unsigned maxScalars,
    DxilSignatureAllocator &allocator,
    ValidationContext &ValCtx) {

  // Skip entries that are not or should not be allocated.  Validation occurs in ValidateSignatureElement.
  if (!E.IsAllocated())
    return;
  switch (E.GetInterpretation()) {
  case DXIL::SemanticInterpretationKind::NA:
  case DXIL::SemanticInterpretationKind::NotInSig:
  case DXIL::SemanticInterpretationKind::Invalid:
  case DXIL::SemanticInterpretationKind::NotPacked:
  case DXIL::SemanticInterpretationKind::Shadow:
    return;
  default:
    break;
  }

  DxilPackElement PE(&E, allocator.UseMinPrecision());
  DxilSignatureAllocator::ConflictType conflict = allocator.DetectRowConflict(&PE, E.GetStartRow());
  if (conflict == DxilSignatureAllocator::kNoConflict || conflict == DxilSignatureAllocator::kInsufficientFreeComponents)
    conflict = allocator.DetectColConflict(&PE, E.GetStartRow(), E.GetStartCol());
  switch (conflict) {
  case DxilSignatureAllocator::kNoConflict:
    allocator.PlaceElement(&PE, E.GetStartRow(), E.GetStartCol());
    break;
  case DxilSignatureAllocator::kConflictsWithIndexed:
    ValCtx.EmitFormatError(ValidationRule::MetaSignatureIndexConflict,
                            {E.GetName(),
                            std::to_string(E.GetStartRow()),
                            std::to_string(E.GetStartCol()),
                            std::to_string(E.GetRows()),
                            std::to_string(E.GetCols())});
    break;
  case DxilSignatureAllocator::kConflictsWithIndexedTessFactor:
    ValCtx.EmitFormatError(ValidationRule::MetaSignatureIndexConflict,
                            {E.GetName(),
                            std::to_string(E.GetStartRow()),
                            std::to_string(E.GetStartCol()),
                            std::to_string(E.GetRows()),
                            std::to_string(E.GetCols())});
    break;
  case DxilSignatureAllocator::kConflictsWithInterpolationMode:
    ValCtx.EmitFormatError(ValidationRule::MetaInterpModeInOneRow,
                            {E.GetName(),
                            std::to_string(E.GetStartRow()),
                            std::to_string(E.GetStartCol()),
                            std::to_string(E.GetRows()),
                            std::to_string(E.GetCols())});
    break;
  case DxilSignatureAllocator::kInsufficientFreeComponents:
    DXASSERT(false, "otherwise, conflict not translated");
    break;
  case DxilSignatureAllocator::kOverlapElement:
    ValCtx.EmitFormatError(ValidationRule::MetaSignatureOverlap,
                            {E.GetName(),
                            std::to_string(E.GetStartRow()),
                            std::to_string(E.GetStartCol()),
                            std::to_string(E.GetRows()),
                            std::to_string(E.GetCols())});
    break;
  case DxilSignatureAllocator::kIllegalComponentOrder:
    ValCtx.EmitFormatError(ValidationRule::MetaSignatureIllegalComponentOrder,
                            {E.GetName(),
                            std::to_string(E.GetStartRow()),
                            std::to_string(E.GetStartCol()),
                            std::to_string(E.GetRows()),
                            std::to_string(E.GetCols())});
    break;
  case DxilSignatureAllocator::kConflictFit:
    ValCtx.EmitFormatError(ValidationRule::MetaSignatureOutOfRange,
                            {E.GetName(),
                            std::to_string(E.GetStartRow()),
                            std::to_string(E.GetStartCol()),
                            std::to_string(E.GetRows()),
                            std::to_string(E.GetCols())});
    break;
  case DxilSignatureAllocator::kConflictDataWidth:
    ValCtx.EmitFormatError(ValidationRule::MetaSignatureDataWidth,
                            {E.GetName(),
                            std::to_string(E.GetStartRow()),
                            std::to_string(E.GetStartCol()),
                            std::to_string(E.GetRows()),
                            std::to_string(E.GetCols())});
    break;
  default:
    DXASSERT(false, "otherwise, unrecognized conflict type from DxilSignatureAllocator");
  }
}

static void ValidateSignature(ValidationContext &ValCtx, const DxilSignature &S,
                              unsigned maxScalars) {
  DxilSignatureAllocator allocator[DXIL::kNumOutputStreams] = {
      {32, !ValCtx.DxilMod.m_ShaderFlags.GetUseNativeLowPrecision()},
      {32, !ValCtx.DxilMod.m_ShaderFlags.GetUseNativeLowPrecision()},
      {32, !ValCtx.DxilMod.m_ShaderFlags.GetUseNativeLowPrecision()},
      {32, !ValCtx.DxilMod.m_ShaderFlags.GetUseNativeLowPrecision()}};
  unordered_set<unsigned> semanticUsageSet[DXIL::kNumOutputStreams];
  StringMap<unordered_set<unsigned>> semanticIndexMap[DXIL::kNumOutputStreams];
  unordered_set<unsigned> clipcullRowSet[DXIL::kNumOutputStreams];
  unsigned clipcullComponents[DXIL::kNumOutputStreams] = {0, 0, 0, 0};

  bool isOutput = S.IsOutput();
  unsigned TargetMask = 0;
  DXIL::SemanticKind DepthKind = DXIL::SemanticKind::Invalid;

  const InterpolationMode *prevBaryInterpMode = nullptr;
  unsigned numBarycentrics = 0;


  for (auto &E : S.GetElements()) {
    DXIL::SemanticKind semanticKind = E->GetSemantic()->GetKind();
    ValidateSignatureElement(*E, ValCtx);
    // Avoid OOB indexing on streamId.
    unsigned streamId = E->GetOutputStream();
    if (streamId >= DXIL::kNumOutputStreams ||
        !isOutput ||
        !ValCtx.DxilMod.GetShaderModel()->IsGS()) {
      streamId = 0;
    }

    // Semantic index overlap check, keyed by name.
    std::string nameUpper(E->GetName());
    std::transform(nameUpper.begin(), nameUpper.end(), nameUpper.begin(), ::toupper);
    unordered_set<unsigned> &semIdxSet = semanticIndexMap[streamId][nameUpper];
    for (unsigned semIdx : E->GetSemanticIndexVec()) {
      if (semIdxSet.count(semIdx) > 0) {
        ValCtx.EmitFormatError(ValidationRule::MetaNoSemanticOverlap,
                               {E->GetName(), std::to_string(semIdx)});
        return;
      } else
        semIdxSet.insert(semIdx);
    }

    // SV_Target has special rules
    if (semanticKind == DXIL::SemanticKind::Target) {
      // Validate target overlap
      if (E->GetStartRow() + E->GetRows() <= 8) {
        unsigned mask = ((1 << E->GetRows()) - 1) << E->GetStartRow();
        if (TargetMask & mask) {
          ValCtx.EmitFormatError(ValidationRule::MetaNoSemanticOverlap,
                                 {"SV_Target", std::to_string(E->GetStartRow())});
        }
        TargetMask = TargetMask | mask;
      }
      if (E->GetRows() > 1) {
        ValCtx.EmitError(ValidationRule::SmNoPSOutputIdx);
      }
      continue;
    }

    if (E->GetSemantic()->IsInvalid())
      continue;

    // validate system value semantic rules
    switch (semanticKind) {
    case DXIL::SemanticKind::Arbitrary:
      break;
    case DXIL::SemanticKind::ClipDistance:
    case DXIL::SemanticKind::CullDistance:
      // Validate max 8 components across 2 rows (registers)
      clipcullRowSet[streamId].insert(E->GetStartRow());
      if (clipcullRowSet[streamId].size() > 2) {
        ValCtx.EmitError(ValidationRule::MetaClipCullMaxRows);
      }
      clipcullComponents[streamId] += E->GetCols();
      if (clipcullComponents[streamId] > 8) {
        ValCtx.EmitError(ValidationRule::MetaClipCullMaxComponents);
      }
      break;
    case DXIL::SemanticKind::Depth:
    case DXIL::SemanticKind::DepthGreaterEqual:
    case DXIL::SemanticKind::DepthLessEqual:
      if (DepthKind != DXIL::SemanticKind::Invalid) {
        ValCtx.EmitError(ValidationRule::SmPSMultipleDepthSemantic);
      }
      DepthKind = semanticKind;
      break;
    case DXIL::SemanticKind::Barycentrics: {
      // There can only be up to two SV_Barycentrics
      // with differeent perspective interpolation modes.
      if (numBarycentrics++ > 1) {
        ValCtx.EmitError(ValidationRule::MetaBarycentricsTwoPerspectives);
        break;
      }
      const InterpolationMode *mode = E->GetInterpolationMode();
      if (prevBaryInterpMode) {
        if ((mode->IsAnyNoPerspective() && prevBaryInterpMode->IsAnyNoPerspective())
          || (!mode->IsAnyNoPerspective() && !prevBaryInterpMode->IsAnyNoPerspective())) {
          ValCtx.EmitError(ValidationRule::MetaBarycentricsTwoPerspectives);
        }
      }
      prevBaryInterpMode = mode;
      break;
    }
    default:
      if (semanticUsageSet[streamId].count(static_cast<unsigned>(semanticKind)) > 0) {
        ValCtx.EmitFormatError(ValidationRule::MetaDuplicateSysValue,
                               {E->GetSemantic()->GetName()});
      }
      semanticUsageSet[streamId].insert(static_cast<unsigned>(semanticKind));
      break;
    }

    // Packed element overlap check.
    ValidateSignatureOverlap(*E.get(), maxScalars, allocator[streamId], ValCtx);

    if (isOutput && semanticKind == DXIL::SemanticKind::Position) {
      ValCtx.hasOutputPosition[E->GetOutputStream()] = true;
    }
  }

  if (ValCtx.hasViewID && S.IsInput() && ValCtx.DxilMod.GetShaderModel()->GetKind() == DXIL::ShaderKind::Pixel) {
    // Ensure sufficient space for ViewID:
    DxilSignatureAllocator::DummyElement viewID;
    viewID.rows = 1;
    viewID.cols = 1;
    viewID.kind = DXIL::SemanticKind::Arbitrary;
    viewID.interpolation = DXIL::InterpolationMode::Constant;
    viewID.interpretation = DXIL::SemanticInterpretationKind::SGV;
    allocator[0].PackNext(&viewID, 0, 32);
    if (!viewID.IsAllocated()) {
      ValCtx.EmitError(ValidationRule::SmViewIDNeedsSlot);
    }
  }
}

static void ValidateNoInterpModeSignature(ValidationContext &ValCtx, const DxilSignature &S) {
  for (auto &E : S.GetElements()) {
    if (!E->GetInterpolationMode()->IsUndefined()) {
      ValCtx.EmitSignatureError(E.get(), ValidationRule::SmNoInterpMode);
    }
  }
}

static void ValidateSignatures(ValidationContext &ValCtx) {
  DxilModule &M = ValCtx.DxilMod;
  bool isPS = M.GetShaderModel()->IsPS();
  bool isVS = M.GetShaderModel()->IsVS();
  bool isGS = M.GetShaderModel()->IsGS();
  bool isCS = M.GetShaderModel()->IsCS();

  if (isPS) {
    // PS output no interp mode.
    ValidateNoInterpModeSignature(ValCtx, ValCtx.DxilMod.GetOutputSignature());
  } else if (isVS) {
    // VS input no interp mode.
    ValidateNoInterpModeSignature(ValCtx, ValCtx.DxilMod.GetInputSignature());
  }
  // patch constant no interp mode.
  ValidateNoInterpModeSignature(ValCtx, ValCtx.DxilMod.GetPatchConstantSignature());

  unsigned maxInputScalars = DXIL::kMaxInputTotalScalars;
  unsigned maxOutputScalars = 0;
  unsigned maxPatchConstantScalars = 0;

  switch (M.GetShaderModel()->GetKind()) {
  case DXIL::ShaderKind::Compute:
    break;
  case DXIL::ShaderKind::Vertex:
  case DXIL::ShaderKind::Geometry:
  case DXIL::ShaderKind::Pixel:
      maxOutputScalars = DXIL::kMaxOutputTotalScalars;
    break;
  case DXIL::ShaderKind::Hull:
  case DXIL::ShaderKind::Domain:
      maxOutputScalars = DXIL::kMaxOutputTotalScalars;
      maxPatchConstantScalars = DXIL::kMaxHSOutputPatchConstantTotalScalars;
    break;
  default:
    break;
  }

  ValidateSignature(ValCtx, ValCtx.DxilMod.GetInputSignature(), maxInputScalars);
  ValidateSignature(ValCtx, ValCtx.DxilMod.GetOutputSignature(), maxOutputScalars);
  ValidateSignature(ValCtx, ValCtx.DxilMod.GetPatchConstantSignature(), maxPatchConstantScalars);

  if (isPS) {
    // Gather execution information.
    hlsl::PSExecutionInfo &PSExec = ValCtx.PSExec;
    for (auto &E : ValCtx.DxilMod.GetInputSignature().GetElements()) {
      if (E->GetKind() == DXIL::SemanticKind::SampleIndex) {
        PSExec.SuperSampling = true;
        continue;
      }

      const InterpolationMode *IM = E->GetInterpolationMode();
      if (IM->IsLinearSample() || IM->IsLinearNoperspectiveSample()) {
        PSExec.SuperSampling = true;
      }
      if (E->GetKind() == DXIL::SemanticKind::Position) {
        PSExec.PositionInterpolationMode = IM;
      }
    }

    for (auto &E : ValCtx.DxilMod.GetOutputSignature().GetElements()) {
      if (E->IsAnyDepth()) {
        PSExec.OutputDepthKind = E->GetKind();
        break;
      }
    }

    if (!PSExec.SuperSampling &&
        PSExec.OutputDepthKind != DXIL::SemanticKind::Invalid &&
        PSExec.OutputDepthKind != DXIL::SemanticKind::Depth) {
      if (PSExec.PositionInterpolationMode != nullptr) {
        if (!PSExec.PositionInterpolationMode->IsUndefined() &&
            !PSExec.PositionInterpolationMode->IsLinearNoperspectiveCentroid() &&
            !PSExec.PositionInterpolationMode->IsLinearNoperspectiveSample()) {
          ValCtx.EmitError(ValidationRule::SmPSConsistentInterp);
        }
      }
    }

    // Validate PS output semantic.
    DxilSignature &outputSig = M.GetOutputSignature();
    for (auto &SE : outputSig.GetElements()) {
      Semantic::Kind semanticKind = SE->GetSemantic()->GetKind();
      switch (semanticKind) {
      case Semantic::Kind::Target:
      case Semantic::Kind::Coverage:
      case Semantic::Kind::Depth:
      case Semantic::Kind::DepthGreaterEqual:
      case Semantic::Kind::DepthLessEqual:
      case Semantic::Kind::StencilRef:
        break;
      default: {
        ValCtx.EmitFormatError(ValidationRule::SmPSOutputSemantic, {SE->GetName()});
      } break;
      }
    }
  }

  if (isGS) {
    unsigned maxVertexCount = M.GetMaxVertexCount();
    unsigned outputScalarCount = 0;
    DxilSignature &outSig = ValCtx.DxilMod.GetOutputSignature();
    for (auto &SE : outSig.GetElements()) {
      outputScalarCount += SE->GetRows() * SE->GetCols();
    }
    unsigned totalOutputScalars = maxVertexCount * outputScalarCount;
    if (totalOutputScalars > DXIL::kMaxGSOutputTotalScalars) {
      ValCtx.EmitFormatError(
          ValidationRule::SmGSTotalOutputVertexDataRange,
          {std::to_string(maxVertexCount),
           std::to_string(outputScalarCount),
           std::to_string(totalOutputScalars),
           std::to_string(DXIL::kMaxGSOutputTotalScalars)});
    }
  }

  if (isCS) {
      if (!ValCtx.DxilMod.GetOutputSignature().GetElements().empty() ||
          !ValCtx.DxilMod.GetPatchConstantSignature().GetElements().empty()) {
        ValCtx.EmitError(ValidationRule::SmCSNoReturn);
      }
  }
}

static void CheckPatchConstantSemantic(ValidationContext &ValCtx)
{
  bool isHS = ValCtx.DxilMod.GetShaderModel()->IsHS();

  DXIL::TessellatorDomain domain = ValCtx.DxilMod.GetTessellatorDomain();
  DxilSignature &patchConstantSig = ValCtx.DxilMod.GetPatchConstantSignature();

  const unsigned kQuadEdgeSize = 4;
  const unsigned kQuadInsideSize = 2;
  const unsigned kQuadDomainLocSize = 2;

  const unsigned kTriEdgeSize = 3;
  const unsigned kTriInsideSize = 1;
  const unsigned kTriDomainLocSize = 3;

  const unsigned kIsolineEdgeSize = 2;
  const unsigned kIsolineInsideSize = 0;
  const unsigned kIsolineDomainLocSize = 3;

  const char *domainName = "";

  DXIL::SemanticKind kEdgeSemantic = DXIL::SemanticKind::TessFactor;
  unsigned edgeSize = 0;

  DXIL::SemanticKind kInsideSemantic = DXIL::SemanticKind::InsideTessFactor;
  unsigned insideSize = 0;

  ValCtx.domainLocSize = 0;

  switch (domain) {
  case DXIL::TessellatorDomain::IsoLine:
    domainName = "IsoLine";
    edgeSize = kIsolineEdgeSize;
    insideSize = kIsolineInsideSize;
    ValCtx.domainLocSize = kIsolineDomainLocSize;
    break;
  case DXIL::TessellatorDomain::Tri:
    domainName = "Tri";
    edgeSize = kTriEdgeSize;
    insideSize = kTriInsideSize;
    ValCtx.domainLocSize = kTriDomainLocSize;
    break;
  case DXIL::TessellatorDomain::Quad:
    domainName = "Quad";
    edgeSize = kQuadEdgeSize;
    insideSize = kQuadInsideSize;
    ValCtx.domainLocSize = kQuadDomainLocSize;
    break;
  default:
    // Don't bother with other tests if domain is invalid
    return;
  }

  bool bFoundEdgeSemantic = false;
  bool bFoundInsideSemantic = false;
  for (auto &SE : patchConstantSig.GetElements()) {
    Semantic::Kind kind = SE->GetSemantic()->GetKind();
    if (kind == kEdgeSemantic) {
      bFoundEdgeSemantic = true;
      if (SE->GetRows() != edgeSize || SE->GetCols() > 1) {
        ValCtx.EmitFormatError(ValidationRule::SmTessFactorSizeMatchDomain,
                               {std::to_string(SE->GetRows()),
                                std::to_string(SE->GetCols()),
                                domainName,
                                std::to_string(edgeSize)});
      }
    } else if (kind == kInsideSemantic) {
      bFoundInsideSemantic = true;
      if (SE->GetRows() != insideSize || SE->GetCols() > 1) {
        ValCtx.EmitFormatError(ValidationRule::SmInsideTessFactorSizeMatchDomain,
                               {std::to_string(SE->GetRows()),
                                std::to_string(SE->GetCols()),
                                domainName,
                                std::to_string(insideSize)});
      }
    }
  }

  if (isHS) {
    if (!bFoundEdgeSemantic) {
      ValCtx.EmitError(ValidationRule::SmTessFactorForDomain);
    }
    if (!bFoundInsideSemantic && domain != DXIL::TessellatorDomain::IsoLine) {
      ValCtx.EmitError(ValidationRule::SmTessFactorForDomain);
    }
  }
}

static void ValidateShaderState(ValidationContext &ValCtx) {
  DxilModule &M = ValCtx.DxilMod;
  DXIL::ShaderKind ShaderType = M.GetShaderModel()->GetKind();

  if (ShaderType == DXIL::ShaderKind::Compute) {
    unsigned x = M.m_NumThreads[0];
    unsigned y = M.m_NumThreads[1];
    unsigned z = M.m_NumThreads[2];

    unsigned threadsInGroup = x * y * z;

    if ((x < DXIL::kMinCSThreadGroupX) || (x > DXIL::kMaxCSThreadGroupX)) {
      ValCtx.EmitFormatError(
          ValidationRule::SmThreadGroupChannelRange,
          {"X", std::to_string(x),
           std::to_string(DXIL::kMinCSThreadGroupX),
           std::to_string(DXIL::kMaxCSThreadGroupX)});
    }
    if ((y < DXIL::kMinCSThreadGroupY) || (y > DXIL::kMaxCSThreadGroupY)) {
      ValCtx.EmitFormatError(
          ValidationRule::SmThreadGroupChannelRange,
          {"Y", std::to_string(y),
           std::to_string(DXIL::kMinCSThreadGroupY),
           std::to_string(DXIL::kMaxCSThreadGroupY)});
    }
    if ((z < DXIL::kMinCSThreadGroupZ) || (z > DXIL::kMaxCSThreadGroupZ)) {
      ValCtx.EmitFormatError(
          ValidationRule::SmThreadGroupChannelRange,
          {"Z", std::to_string(z),
           std::to_string(DXIL::kMinCSThreadGroupZ),
           std::to_string(DXIL::kMaxCSThreadGroupZ)});
    }

    if (threadsInGroup > DXIL::kMaxCSThreadsPerGroup) {
      ValCtx.EmitFormatError(
          ValidationRule::SmMaxTheadGroup,
          {std::to_string(threadsInGroup),
           std::to_string(DXIL::kMaxCSThreadsPerGroup)});
    }

    // type of threadID, thread group ID take care by DXIL operation overload
    // check.
  } else if (ShaderType == DXIL::ShaderKind::Domain) {
    DXIL::TessellatorDomain domain = M.GetTessellatorDomain();
    if (domain >= DXIL::TessellatorDomain::LastEntry)
      domain = DXIL::TessellatorDomain::Undefined;
    unsigned inputControlPointCount = M.GetInputControlPointCount();

    if (inputControlPointCount > DXIL::kMaxIAPatchControlPointCount) {
      ValCtx.EmitFormatError(
          ValidationRule::SmDSInputControlPointCountRange,
          {std::to_string(DXIL::kMaxIAPatchControlPointCount),
           std::to_string(inputControlPointCount)});
    }
    if (domain == DXIL::TessellatorDomain::Undefined) {
      ValCtx.EmitError(ValidationRule::SmValidDomain);
    }
    CheckPatchConstantSemantic(ValCtx);
  } else if (ShaderType == DXIL::ShaderKind::Hull) {
    DXIL::TessellatorDomain domain = M.GetTessellatorDomain();
    if (domain >= DXIL::TessellatorDomain::LastEntry)
      domain = DXIL::TessellatorDomain::Undefined;
    unsigned inputControlPointCount = M.GetInputControlPointCount();
    if (inputControlPointCount == 0) {
      if (!M.GetInputSignature().GetElements().empty()) {
        ValCtx.EmitError(
            ValidationRule::SmZeroHSInputControlPointWithInput);
      }
    } else if (inputControlPointCount > DXIL::kMaxIAPatchControlPointCount) {
      ValCtx.EmitFormatError(
          ValidationRule::SmHSInputControlPointCountRange,
          {std::to_string(DXIL::kMaxIAPatchControlPointCount),
           std::to_string(inputControlPointCount)});
    }
    if (domain == DXIL::TessellatorDomain::Undefined) {
      ValCtx.EmitError(ValidationRule::SmValidDomain);
    }
    DXIL::TessellatorPartitioning partition = M.GetTessellatorPartitioning();
    if (partition == DXIL::TessellatorPartitioning::Undefined) {
      ValCtx.EmitError(ValidationRule::MetaTessellatorPartition);
    }

    DXIL::TessellatorOutputPrimitive tessOutputPrimitive =
        M.GetTessellatorOutputPrimitive();
    if (tessOutputPrimitive == DXIL::TessellatorOutputPrimitive::Undefined ||
        tessOutputPrimitive == DXIL::TessellatorOutputPrimitive::LastEntry) {
      ValCtx.EmitError(ValidationRule::MetaTessellatorOutputPrimitive);
    }

    float maxTessFactor = M.GetMaxTessellationFactor();
    if (maxTessFactor < DXIL::kHSMaxTessFactorLowerBound ||
        maxTessFactor > DXIL::kHSMaxTessFactorUpperBound) {
      ValCtx.EmitFormatError(
          ValidationRule::MetaMaxTessFactor,
          {std::to_string(DXIL::kHSMaxTessFactorLowerBound),
           std::to_string(DXIL::kHSMaxTessFactorUpperBound),
           std::to_string(maxTessFactor)});
    }
    // Domain and OutPrimivtive match.
    switch (domain) {
    case DXIL::TessellatorDomain::IsoLine:
      switch (tessOutputPrimitive) {
      case DXIL::TessellatorOutputPrimitive::TriangleCW:
      case DXIL::TessellatorOutputPrimitive::TriangleCCW:
        ValCtx.EmitError(ValidationRule::SmIsoLineOutputPrimitiveMismatch);
        break;
      default:
        break;
      }
      break;
    case DXIL::TessellatorDomain::Tri:
      switch (tessOutputPrimitive) {
      case DXIL::TessellatorOutputPrimitive::Line:
        ValCtx.EmitError(ValidationRule::SmTriOutputPrimitiveMismatch);
        break;
      default:
        break;
      }
      break;
    case DXIL::TessellatorDomain::Quad:
      switch (tessOutputPrimitive) {
      case DXIL::TessellatorOutputPrimitive::Line:
        ValCtx.EmitError(ValidationRule::SmTriOutputPrimitiveMismatch);
        break;
      default:
        break;
      }
      break;
    default:
      ValCtx.EmitError(ValidationRule::SmValidDomain);
      break;
    }

    // Check pass thru HS.
    if (M.GetEntryFunction() == nullptr) {
      if (M.GetShaderModel()->IsHS()) {
        if (M.GetInputControlPointCount() < M.GetOutputControlPointCount()) {
          ValCtx.EmitError(
              ValidationRule::SmHullPassThruControlPointCountMatch);
        }

        // Check declared control point outputs storage amounts are ok to pass
        // through (less output storage than input for control points).
        DxilSignature &outSig = M.GetOutputSignature();
        unsigned totalOutputCPScalars = 0;
        for (auto &SE : outSig.GetElements()) {
          totalOutputCPScalars += SE->GetRows() * SE->GetCols();
        }
        if (totalOutputCPScalars * M.GetOutputControlPointCount() >
            DXIL::kMaxHSOutputControlPointsTotalScalars) {
          ValCtx.EmitError(ValidationRule::SmOutputControlPointsTotalScalars);
        }
      } else {
        ValCtx.EmitError(ValidationRule::MetaEntryFunction);
      }
    }

    CheckPatchConstantSemantic(ValCtx);
  } else if (ShaderType == DXIL::ShaderKind::Geometry) {
    unsigned maxVertexCount = M.GetMaxVertexCount();
    if (maxVertexCount > DXIL::kMaxGSOutputVertexCount) {
      ValCtx.EmitFormatError(
          ValidationRule::SmGSOutputVertexCountRange,
          {std::to_string(DXIL::kMaxGSOutputVertexCount),
           std::to_string(maxVertexCount)});
    }

    unsigned instanceCount = M.GetGSInstanceCount();
    if (instanceCount > DXIL::kMaxGSInstanceCount || instanceCount < 1) {
      ValCtx.EmitFormatError(ValidationRule::SmGSInstanceCountRange,
                             {std::to_string(DXIL::kMaxGSInstanceCount),
                              std::to_string(instanceCount)});
    }

    DXIL::PrimitiveTopology topo = M.GetStreamPrimitiveTopology();
    switch (topo) {
    case DXIL::PrimitiveTopology::PointList:
    case DXIL::PrimitiveTopology::LineStrip:
    case DXIL::PrimitiveTopology::TriangleStrip:
      break;
    default: {
      ValCtx.EmitError(ValidationRule::SmGSValidOutputPrimitiveTopology);
    } break;
    }

    DXIL::InputPrimitive inputPrimitive = M.GetInputPrimitive();
    unsigned VertexCount = GetNumVertices(inputPrimitive);
    if (VertexCount == 0 && inputPrimitive != DXIL::InputPrimitive::Undefined) {
      ValCtx.EmitError(ValidationRule::SmGSValidInputPrimitive);
    }
  }

  unsigned outputControlPointCount = M.GetOutputControlPointCount();
  if (outputControlPointCount > DXIL::kMaxIAPatchControlPointCount) {
    ValCtx.EmitFormatError(
        ValidationRule::SmOutputControlPointCountRange,
        {std::to_string(DXIL::kMaxIAPatchControlPointCount),
         std::to_string(outputControlPointCount)});
  }
}

static bool
CalculateCallDepth(CallGraphNode *node,
                   std::unordered_map<CallGraphNode *, unsigned> &depthMap,
                   std::unordered_set<CallGraphNode *> &callStack,
                   std::unordered_set<Function *> &funcSet) {
  unsigned depth = callStack.size();
  funcSet.insert(node->getFunction());
  for (auto it = node->begin(), ei = node->end(); it != ei; it++) {
    CallGraphNode *toNode = it->second;
    if (callStack.insert(toNode).second == false) {
      // Recursive.
      return true;
    }
    if (depthMap[toNode] < depth)
      depthMap[toNode] = depth;
    if (CalculateCallDepth(toNode, depthMap, callStack, funcSet)) {
      // Recursive
      return true;
    }
    callStack.erase(toNode);
  }

  return false;
}

static void ValidateCallGraph(ValidationContext &ValCtx) {
  // Build CallGraph.
  CallGraph CG(*ValCtx.DxilMod.GetModule());

  std::unordered_map<CallGraphNode*, unsigned> depthMap;
  std::unordered_set<CallGraphNode*> callStack;
  CallGraphNode *entryNode = CG[ValCtx.DxilMod.GetEntryFunction()];
  depthMap[entryNode] = 0;
  bool bRecursive = CalculateCallDepth(entryNode, depthMap, callStack, ValCtx.entryFuncCallSet);
  if (ValCtx.DxilMod.GetShaderModel()->IsHS()) {
    CallGraphNode *patchConstantNode = CG[ValCtx.DxilMod.GetPatchConstantFunction()];
    depthMap[patchConstantNode] = 0;
    callStack.clear();
    bRecursive |= CalculateCallDepth(patchConstantNode, depthMap, callStack, ValCtx.patchConstFuncCallSet);
  }

  if (bRecursive) {
    ValCtx.EmitError(ValidationRule::FlowNoRecusion);
  }
}

static void ValidateFlowControl(ValidationContext &ValCtx) {
  bool reducible =
      IsReducible(*ValCtx.DxilMod.GetModule(), IrreducibilityAction::Ignore);
  if (!reducible) {
    ValCtx.EmitError(ValidationRule::FlowReducible);
    return;
  }

  ValidateCallGraph(ValCtx);

  for (auto &F : ValCtx.DxilMod.GetModule()->functions()) {
    if (F.isDeclaration())
      continue;

    DominatorTreeAnalysis DTA;
    DominatorTree DT = DTA.run(F);
    LoopInfo LI;
    LI.Analyze(DT);
    for (auto loopIt = LI.begin(); loopIt != LI.end(); loopIt++) {
      Loop *loop = *loopIt;
      SmallVector<BasicBlock *, 4> exitBlocks;
      loop->getExitBlocks(exitBlocks);
      if (exitBlocks.empty())
        ValCtx.EmitError(ValidationRule::FlowDeadLoop);
    }
  }
  // fxc has ERR_CONTINUE_INSIDE_SWITCH to disallow continue in switch.
  // Not do it for now.
}

static void ValidateUninitializedOutput(ValidationContext &ValCtx) {
  // For HS only need to check Tessfactor which is in patch constant sig.
  if (ValCtx.DxilMod.GetShaderModel()->IsHS()) {
    std::vector<unsigned> &patchConstCols = ValCtx.patchConstCols;
    for (auto &E : ValCtx.DxilMod.GetPatchConstantSignature().GetElements()) {
      unsigned mask = patchConstCols[E->GetID()];
      unsigned requireMask = (1 << E->GetCols()) - 1;
      // TODO: check other case uninitialized output is allowed.
      if (mask != requireMask && !E->GetSemantic()->IsArbitrary()) {
        ValCtx.EmitFormatError(ValidationRule::SmUndefinedOutput,
                               {E->GetName()});
      }
    }
    return;
  }
  std::vector<unsigned> &outputCols = ValCtx.outputCols;
  for (auto &E : ValCtx.DxilMod.GetOutputSignature().GetElements()) {
    unsigned mask = outputCols[E->GetID()];
    unsigned requireMask = (1 << E->GetCols()) - 1;
    // TODO: check other case uninitialized output is allowed.
    if (mask != requireMask && !E->GetSemantic()->IsArbitrary() &&
        E->GetSemantic()->GetKind() != Semantic::Kind::Target) {
      ValCtx.EmitFormatError(ValidationRule::SmUndefinedOutput, {E->GetName()});
    }
  }
}

void GetValidationVersion(_Out_ unsigned *pMajor, _Out_ unsigned *pMinor) {
  // 1.0 is the first validator.
  // 1.1 adds:
  // - ILDN container part support
  // 1.2 adds:
  // - Metadata for floating point denorm mode
  // 1.3 adds:
  // TODO: add comment
  *pMajor = 1;
  *pMinor = 3;
}

_Use_decl_annotations_ HRESULT
ValidateDxilModule(llvm::Module *pModule, llvm::Module *pDebugModule) {
  std::string diagStr;
  raw_string_ostream diagStream(diagStr);
  DiagnosticPrinterRawOStream DiagPrinter(diagStream);

  DxilModule *pDxilModule = DxilModule::TryGetDxilModule(pModule);
  if (!pDxilModule) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  ValidationContext ValCtx(*pModule, pDebugModule, *pDxilModule, DiagPrinter);

  ValidateMetadata(ValCtx);

  ValidateShaderState(ValCtx);

  ValidateGlobalVariables(ValCtx);

  ValidateResources(ValCtx);

  // Validate control flow and collect function call info.
  // If has recursive call, call info collection will not finish.
  ValidateFlowControl(ValCtx);

  // Validate functions.
  for (Function &F : pModule->functions()) {
    ValidateFunction(F, ValCtx);
  }

  ValidateUninitializedOutput(ValCtx);

  ValidateShaderFlags(ValCtx);

  ValidateSignatures(ValCtx);

  if (!pDxilModule->GetShaderModel()->IsGS()) {
    unsigned posMask = ValCtx.OutputPositionMask[0];
    if (posMask != 0xf && ValCtx.hasOutputPosition[0]) {
      ValCtx.EmitError(ValidationRule::SmCompletePosition);
    }
  } else {
    unsigned streamMask = ValCtx.DxilMod.GetActiveStreamMask();
    for (unsigned i = 0; i < DXIL::kNumOutputStreams; i++) {
      if (streamMask & (1 << i)) {
        unsigned posMask = ValCtx.OutputPositionMask[i];
        if (posMask != 0xf && ValCtx.hasOutputPosition[i]) {
          ValCtx.EmitError(ValidationRule::SmCompletePosition);
        }
      }
    }
  }

  // Ensure error messages are flushed out on error.
  if (ValCtx.Failed) {
    emitDxilDiag(pModule->getContext(), diagStream.str().c_str());
    return DXC_E_IR_VERIFICATION_FAILED;
  }
  return S_OK;
}

// DXIL Container Verification Functions

static void VerifyBlobPartMatches(_In_ ValidationContext &ValCtx,
                                  _In_ LPCSTR pName,
                                  DxilPartWriter *pWriter,
                                  _In_reads_bytes_opt_(Size) const void *pData,
                                  _In_ uint32_t Size) {
  if (!pData && pWriter->size()) {
    // No blob part, but writer says non-zero size is expected.
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing, {pName});
    return;
  }

  // Compare sizes
  if (pWriter->size() != Size) {
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches, {pName});
    return;
  }

  if (Size == 0) {
    return;
  }

  CComPtr<AbstractMemoryStream> pOutputStream;
  IFT(CreateMemoryStream(DxcGetThreadMallocNoRef(), &pOutputStream));
  pOutputStream->Reserve(Size);

  pWriter->write(pOutputStream);
  DXASSERT(pOutputStream->GetPtrSize() == Size, "otherwise, DxilPartWriter misreported size");

  if (memcmp(pData, pOutputStream->GetPtr(), Size)) {
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches, {pName});
    return;
  }

  return;
}

static void VerifySignatureMatches(_In_ ValidationContext &ValCtx,
                                   DXIL::SignatureKind SigKind,
                                   _In_reads_bytes_opt_(SigSize) const void *pSigData,
                                   _In_ uint32_t SigSize) {
  // Generate corresponding signature from module and memcmp

  const char *pName = nullptr;
  switch (SigKind)
  {
  case hlsl::DXIL::SignatureKind::Input:
    pName = "Program Input Signature";
    break;
  case hlsl::DXIL::SignatureKind::Output:
    pName = "Program Output Signature";
    break;
  case hlsl::DXIL::SignatureKind::PatchConstant:
    pName = "Program Patch Constant Signature";
    break;
  default:
    break;
  }

  unique_ptr<DxilPartWriter> pWriter(NewProgramSignatureWriter(ValCtx.DxilMod, SigKind));
  VerifyBlobPartMatches(ValCtx, pName, pWriter.get(), pSigData, SigSize);
}

_Use_decl_annotations_
bool VerifySignatureMatches(llvm::Module *pModule,
                            DXIL::SignatureKind SigKind,
                            const void *pSigData,
                            uint32_t SigSize) {
  std::string diagStr;
  raw_string_ostream diagStream(diagStr);
  DiagnosticPrinterRawOStream DiagPrinter(diagStream);
  ValidationContext ValCtx(*pModule, nullptr, pModule->GetOrCreateDxilModule(), DiagPrinter);
  VerifySignatureMatches(ValCtx, SigKind, pSigData, SigSize);
  if (ValCtx.Failed) {
    emitDxilDiag(pModule->getContext(), diagStream.str().c_str());
  }
  return !ValCtx.Failed;
}

static void VerifyPSVMatches(_In_ ValidationContext &ValCtx,
                             _In_reads_bytes_(PSVSize) const void *pPSVData,
                             _In_ uint32_t PSVSize) {
  uint32_t PSVVersion = 1;  // This should be set to the newest version
  unique_ptr<DxilPartWriter> pWriter(NewPSVWriter(ValCtx.DxilMod, PSVVersion));
  // Try each version in case an earlier version matches module
  while (PSVVersion && pWriter->size() != PSVSize) {
    PSVVersion --;
    pWriter.reset(NewPSVWriter(ValCtx.DxilMod, PSVVersion));
  }
  // generate PSV data from module and memcmp
  VerifyBlobPartMatches(ValCtx, "Pipeline State Validation", pWriter.get(), pPSVData, PSVSize);
}

_Use_decl_annotations_
bool VerifyPSVMatches(llvm::Module *pModule,
                      const void *pPSVData,
                      uint32_t PSVSize) {
  std::string diagStr;
  raw_string_ostream diagStream(diagStr);
  DiagnosticPrinterRawOStream DiagPrinter(diagStream);
  ValidationContext ValCtx(*pModule, nullptr, pModule->GetOrCreateDxilModule(), DiagPrinter);
  VerifyPSVMatches(ValCtx, pPSVData, PSVSize);
  if (ValCtx.Failed) {
    emitDxilDiag(pModule->getContext(), diagStream.str().c_str());
  }
  return !ValCtx.Failed;
}

static void VerifyFeatureInfoMatches(_In_ ValidationContext &ValCtx,
                                     _In_reads_bytes_(FeatureInfoSize) const void *pFeatureInfoData,
                                     _In_ uint32_t FeatureInfoSize) {
  // generate Feature Info data from module and memcmp
  unique_ptr<DxilPartWriter> pWriter(NewFeatureInfoWriter(ValCtx.DxilMod));
  VerifyBlobPartMatches(ValCtx, "Feature Info", pWriter.get(), pFeatureInfoData, FeatureInfoSize);
}

_Use_decl_annotations_
bool VerifyFeatureInfoMatches(llvm::Module *pModule,
                              const void *pFeatureInfoData,
                              uint32_t FeatureInfoSize) {
  std::string diagStr;
  raw_string_ostream diagStream(diagStr);
  DiagnosticPrinterRawOStream DiagPrinter(diagStream);
  ValidationContext ValCtx(*pModule, nullptr, pModule->GetOrCreateDxilModule(), DiagPrinter);
  VerifyFeatureInfoMatches(ValCtx, pFeatureInfoData, FeatureInfoSize);
  if (ValCtx.Failed) {
    emitDxilDiag(pModule->getContext(), diagStream.str().c_str());
  }
  return !ValCtx.Failed;
}

_Use_decl_annotations_
HRESULT ValidateDxilContainerParts(llvm::Module *pModule,
                                   llvm::Module *pDebugModule,
                                   const DxilContainerHeader *pContainer,
                                   uint32_t ContainerSize) {

  DXASSERT_NOMSG(pModule);
  if (!pContainer || !IsValidDxilContainer(pContainer, ContainerSize)) {
    return DXC_E_CONTAINER_INVALID;
  }

  DxilModule *pDxilModule = DxilModule::TryGetDxilModule(pModule);
  if (!pDxilModule) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  std::string diagStr;
  raw_string_ostream DiagStream(diagStr);
  DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  ValidationContext ValCtx(*pModule, pDebugModule, *pDxilModule, DiagPrinter);

  DXIL::ShaderKind ShaderKind = pDxilModule->GetShaderModel()->GetKind();
  bool bTess = ShaderKind == DXIL::ShaderKind::Hull || ShaderKind == DXIL::ShaderKind::Domain;

  std::unordered_set<uint32_t> FourCCFound;
  const DxilPartHeader *pRootSignaturePart = nullptr;
  const DxilPartHeader *pPSVPart = nullptr;

  for (auto it = begin(pContainer), itEnd = end(pContainer); it != itEnd; ++it) {
    const DxilPartHeader *pPart = *it;

    char szFourCC[5];
    PartKindToCharArray(pPart->PartFourCC, szFourCC);
    if (FourCCFound.find(pPart->PartFourCC) != FourCCFound.end()) {
      // Two parts with same FourCC found
      ValCtx.EmitFormatError(ValidationRule::ContainerPartRepeated, {szFourCC});
      continue;
    }
    FourCCFound.insert(pPart->PartFourCC);

    switch (pPart->PartFourCC)
    {
    case DFCC_InputSignature:
      VerifySignatureMatches(ValCtx, DXIL::SignatureKind::Input, GetDxilPartData(pPart), pPart->PartSize);
      break;
    case DFCC_OutputSignature:
      VerifySignatureMatches(ValCtx, DXIL::SignatureKind::Output, GetDxilPartData(pPart), pPart->PartSize);
      break;
    case DFCC_PatchConstantSignature:
      if (bTess) {
        VerifySignatureMatches(ValCtx, DXIL::SignatureKind::PatchConstant, GetDxilPartData(pPart), pPart->PartSize);
      } else {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches, {"Program Patch Constant Signature"});
      }
      break;
    case DFCC_FeatureInfo:
      VerifyFeatureInfoMatches(ValCtx, GetDxilPartData(pPart), pPart->PartSize);
      break;
    case DFCC_RootSignature:
      pRootSignaturePart = pPart;
      break;
    case DFCC_PipelineStateValidation:
      pPSVPart = pPart;
      VerifyPSVMatches(ValCtx, GetDxilPartData(pPart), pPart->PartSize);
      break;

    // Skip these
    case DFCC_ResourceDef:
    case DFCC_ShaderStatistics:
    case DFCC_PrivateData:
    case DFCC_DXIL:
    case DFCC_ShaderDebugInfoDXIL:
    case DFCC_ShaderDebugName:
      continue;

    case DFCC_Container:
    default:
      ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid, {szFourCC});
      break;
    }
  }

  // Verify required parts found
  if (FourCCFound.find(DFCC_InputSignature) == FourCCFound.end()) {
    VerifySignatureMatches(ValCtx, DXIL::SignatureKind::Input, nullptr, 0);
  }
  if (FourCCFound.find(DFCC_OutputSignature) == FourCCFound.end()) {
    VerifySignatureMatches(ValCtx, DXIL::SignatureKind::Output, nullptr, 0);
  }
  if (bTess && FourCCFound.find(DFCC_PatchConstantSignature) == FourCCFound.end() &&
      pDxilModule->GetPatchConstantSignature().GetElements().size())
  {
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing, {"Program Patch Constant Signature"});
  }
  if (FourCCFound.find(DFCC_FeatureInfo) == FourCCFound.end()) {
    // Could be optional, but RS1 runtime doesn't handle this case properly.
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing, {"Feature Info"});
  }

  // Validate Root Signature
  if (pPSVPart) {
    if (pRootSignaturePart) {
      try {
        RootSignatureHandle RS;
        RS.LoadSerialized((const uint8_t*)GetDxilPartData(pRootSignaturePart), pRootSignaturePart->PartSize);
        RS.Deserialize();
        IFTBOOL(VerifyRootSignatureWithShaderPSV(RS.GetDesc(),
                                                  pDxilModule->GetShaderModel()->GetKind(),
                                                  GetDxilPartData(pPSVPart), pPSVPart->PartSize,
                                                  DiagStream), DXC_E_INCORRECT_ROOT_SIGNATURE);
      } catch (...) {
        ValCtx.EmitError(ValidationRule::ContainerRootSignatureIncompatible);
      }
    }
  } else {
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing, {"Pipeline State Validation"});
  }

  if (ValCtx.Failed) {
    emitDxilDiag(pModule->getContext(), DiagStream.str().c_str());
    return DXC_E_MALFORMED_CONTAINER;
  }
  return S_OK;
}

static HRESULT FindDxilPart(_In_reads_bytes_(ContainerSize) const void *pContainerBytes,
                            _In_ uint32_t ContainerSize,
                            _In_ DxilFourCC FourCC,
                            _In_ const DxilPartHeader **ppPart) {

  const DxilContainerHeader *pContainer =
    IsDxilContainerLike(pContainerBytes, ContainerSize);

  if (!pContainer) {
    IFR(DXC_E_CONTAINER_INVALID);
  }
  if (!IsValidDxilContainer(pContainer, ContainerSize)) {
    IFR(DXC_E_CONTAINER_INVALID);
  }

  DxilPartIterator it = std::find_if(begin(pContainer), end(pContainer),
    DxilPartIsType(FourCC));
  if (it == end(pContainer)) {
    IFR(DXC_E_CONTAINER_MISSING_DXIL);
  }

  const DxilProgramHeader *pProgramHeader =
    reinterpret_cast<const DxilProgramHeader *>(GetDxilPartData(*it));
  if (!IsValidDxilProgramHeader(pProgramHeader, (*it)->PartSize)) {
    IFR(DXC_E_CONTAINER_INVALID);
  }

  *ppPart = *it;
  return S_OK;
}

_Use_decl_annotations_
HRESULT ValidateLoadModule(const char *pIL,
                           uint32_t ILLength,
                           unique_ptr<llvm::Module> &pModule,
                           LLVMContext &Ctx,
                           llvm::raw_ostream &DiagStream,
                           unsigned bLazyLoad) {

  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  DiagRestore DR(Ctx, &DiagContext);

  std::unique_ptr<llvm::MemoryBuffer> pBitcodeBuf;
  pBitcodeBuf.reset(llvm::MemoryBuffer::getMemBuffer(
      llvm::StringRef(pIL, ILLength), "", false).release());

  ErrorOr<std::unique_ptr<Module>> loadedModuleResult =
      bLazyLoad == 0?
      llvm::parseBitcodeFile(pBitcodeBuf->getMemBufferRef(), Ctx) :
      llvm::getLazyBitcodeModule(std::move(pBitcodeBuf), Ctx);

  // DXIL disallows some LLVM bitcode constructs, like unaccounted-for sub-blocks.
  // These appear as warnings, which the validator should reject.
  if (DiagContext.HasErrors() || DiagContext.HasWarnings() || loadedModuleResult.getError())
    return DXC_E_IR_VERIFICATION_FAILED;

  pModule = std::move(loadedModuleResult.get());
  return S_OK;
}

HRESULT ValidateDxilBitcode(
  _In_reads_bytes_(ILLength) const char *pIL,
  _In_ uint32_t ILLength,
  _In_ llvm::raw_ostream &DiagStream) {

  LLVMContext Ctx;
  std::unique_ptr<llvm::Module> pModule;

  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  Ctx.setDiagnosticHandler(PrintDiagnosticContext::PrintDiagnosticHandler,
                           &DiagContext, true);

  HRESULT hr;
  if (FAILED(hr = ValidateLoadModule(pIL, ILLength, pModule, Ctx, DiagStream,
                                     /*bLazyLoad*/ false)))
    return hr;

  if (FAILED(hr = ValidateDxilModule(pModule.get(), nullptr)))
    return hr;

  DxilModule &dxilModule = pModule->GetDxilModule();
  if (!dxilModule.GetRootSignature().IsEmpty()) {
    unique_ptr<DxilPartWriter> pWriter(NewPSVWriter(dxilModule, 0));
    DXASSERT_NOMSG(pWriter->size());
    CComPtr<AbstractMemoryStream> pOutputStream;
    IFT(CreateMemoryStream(DxcGetThreadMallocNoRef(), &pOutputStream));
    pOutputStream->Reserve(pWriter->size());
    pWriter->write(pOutputStream);
    const DxilVersionedRootSignatureDesc* pDesc = dxilModule.GetRootSignature().GetDesc();
    RootSignatureHandle RS;
    try {
      if (!pDesc) {
        RS.Assign(nullptr, dxilModule.GetRootSignature().GetSerialized());
        RS.Deserialize();
        pDesc = RS.GetDesc();
        if (!pDesc)
          return DXC_E_INCORRECT_ROOT_SIGNATURE;
      }
      IFTBOOL(VerifyRootSignatureWithShaderPSV(pDesc,
                                               dxilModule.GetShaderModel()->GetKind(),
                                               pOutputStream->GetPtr(), pWriter->size(),
                                               DiagStream), DXC_E_INCORRECT_ROOT_SIGNATURE);
    } catch (...) {
      return DXC_E_INCORRECT_ROOT_SIGNATURE;
    }
  }

  if (DiagContext.HasErrors() || DiagContext.HasWarnings()) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  return S_OK;
}

static HRESULT ValidateLoadModuleFromContainer(
    _In_reads_bytes_(ILLength) const void *pContainer,
    _In_ uint32_t ContainerSize, _In_ std::unique_ptr<llvm::Module> &pModule,
    _In_ std::unique_ptr<llvm::Module> &pDebugModule,
    _In_ llvm::LLVMContext &Ctx, LLVMContext &DbgCtx,
    _In_ llvm::raw_ostream &DiagStream, _In_ unsigned bLazyLoad) {
  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  DiagRestore DR(Ctx, &DiagContext);
  DiagRestore DR2(DbgCtx, &DiagContext);

  const DxilPartHeader *pPart = nullptr;
  IFR(FindDxilPart(pContainer, ContainerSize, DFCC_DXIL, &pPart));

  const char *pIL = nullptr;
  uint32_t ILLength = 0;
  GetDxilProgramBitcode(
      reinterpret_cast<const DxilProgramHeader *>(GetDxilPartData(pPart)), &pIL,
      &ILLength);

  IFR(ValidateLoadModule(pIL, ILLength, pModule, Ctx, DiagStream, bLazyLoad));

  HRESULT hr;
  const DxilPartHeader *pDbgPart = nullptr;
  if (FAILED(hr = FindDxilPart(pContainer, ContainerSize,
                               DFCC_ShaderDebugInfoDXIL, &pDbgPart)) &&
      hr != DXC_E_CONTAINER_MISSING_DXIL) {
    return hr;
  }

  if (pDbgPart) {
    GetDxilProgramBitcode(
        reinterpret_cast<const DxilProgramHeader *>(GetDxilPartData(pDbgPart)),
        &pIL, &ILLength);
    if (FAILED(hr = ValidateLoadModule(pIL, ILLength, pDebugModule, DbgCtx,
                                       DiagStream, bLazyLoad))) {
      return hr;
    }
  }

  return S_OK;
}

_Use_decl_annotations_ HRESULT ValidateLoadModuleFromContainer(
    _In_reads_bytes_(ContainerSize) const void *pContainer,
    _In_ uint32_t ContainerSize, _In_ std::unique_ptr<llvm::Module> &pModule,
    _In_ std::unique_ptr<llvm::Module> &pDebugModule,
    _In_ llvm::LLVMContext &Ctx, llvm::LLVMContext &DbgCtx,
    _In_ llvm::raw_ostream &DiagStream) {
  return ValidateLoadModuleFromContainer(pContainer, ContainerSize, pModule,
                                         pDebugModule, Ctx, DbgCtx, DiagStream,
                                         /*bLazyLoad*/ false);
}
// Lazy loads module from container, validating load, but not module.
_Use_decl_annotations_ HRESULT ValidateLoadModuleFromContainerLazy(
    _In_reads_bytes_(ContainerSize) const void *pContainer,
    _In_ uint32_t ContainerSize, _In_ std::unique_ptr<llvm::Module> &pModule,
    _In_ std::unique_ptr<llvm::Module> &pDebugModule,
    _In_ llvm::LLVMContext &Ctx, llvm::LLVMContext &DbgCtx,
    _In_ llvm::raw_ostream &DiagStream) {
  return ValidateLoadModuleFromContainer(pContainer, ContainerSize, pModule,
                                         pDebugModule, Ctx, DbgCtx, DiagStream,
                                         /*bLazyLoad*/ true);
}

_Use_decl_annotations_
HRESULT ValidateDxilContainer(const void *pContainer,
                              uint32_t ContainerSize,
                              llvm::raw_ostream &DiagStream) {
  LLVMContext Ctx, DbgCtx;
  std::unique_ptr<llvm::Module> pModule, pDebugModule;

  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  Ctx.setDiagnosticHandler(PrintDiagnosticContext::PrintDiagnosticHandler,
                           &DiagContext, true);
  DbgCtx.setDiagnosticHandler(PrintDiagnosticContext::PrintDiagnosticHandler,
                              &DiagContext, true);

  IFR(ValidateLoadModuleFromContainer(pContainer, ContainerSize, pModule, pDebugModule,
      Ctx, DbgCtx, DiagStream));

  // Validate DXIL Module
  IFR(ValidateDxilModule(pModule.get(), pDebugModule.get()));

  if (DiagContext.HasErrors() || DiagContext.HasWarnings()) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  return ValidateDxilContainerParts(pModule.get(), pDebugModule.get(),
    IsDxilContainerLike(pContainer, ContainerSize), ContainerSize);
}

} // namespace hlsl
