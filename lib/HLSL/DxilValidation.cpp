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

#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/FileIOHelper.h"

#include "dxc/HLSL/DxilValidation.h"
#include "dxc/DxilContainer/DxilContainerAssembler.h"
#include "dxc/DxilContainer/DxilRuntimeReflection.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilShaderModel.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DXIL/DxilFunctionProps.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/DXIL/DxilInstructions.h"
#include "llvm/Analysis/ReducibilityAnalysis.h"
#include "dxc/DXIL/DxilEntryProps.h"

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
#include "llvm/IR/Verifier.h"
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
#include "dxc/HLSL/DxilPackSignatureElement.h"
#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include <algorithm>
#include <deque>

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
    case hlsl::ValidationRule::MetaNoEntryPropsForEntry: return "EntryPoints must have entry properties.";
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
    case hlsl::ValidationRule::InstrBarrierModeForNonCS: return "sync in a non-Compute/Amplification/Mesh Shader must only sync UAV (sync_uglobal)";
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
    case hlsl::ValidationRule::InstrResourceClassForSamplerGather: return "sample, lod and gather should be on srv resource.";
    case hlsl::ValidationRule::InstrResourceClassForUAVStore: return "store should be on uav resource.";
    case hlsl::ValidationRule::InstrResourceClassForLoad: return "load can only run on UAV/SRV resource";
    case hlsl::ValidationRule::InstrResourceMapToSingleEntry: return "Fail to map resource to resource table";
    case hlsl::ValidationRule::InstrResourceUser: return "Resource should only used by Load/GEP/Call";
    case hlsl::ValidationRule::InstrResourceKindForTraceRay: return "TraceRay should only use RTAccelerationStructure";
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
    case hlsl::ValidationRule::InstrBufferUpdateCounterOnResHasCounter: return "BufferUpdateCounter valid only when HasCounter is true";
    case hlsl::ValidationRule::InstrCBufferOutOfBound: return "Cbuffer access out of bound";
    case hlsl::ValidationRule::InstrCBufferClassForCBufferHandle: return "Expect Cbuffer for CBufferLoad handle";
    case hlsl::ValidationRule::InstrFailToResloveTGSMPointer: return "TGSM pointers must originate from an unambiguous TGSM global variable.";
    case hlsl::ValidationRule::InstrExtractValue: return "ExtractValue should only be used on dxil struct types and cmpxchg";
    case hlsl::ValidationRule::InstrTGSMRaceCond: return "Race condition writing to shared memory detected, consider making this write conditional";
    case hlsl::ValidationRule::InstrAttributeAtVertexNoInterpolation: return "Attribute %0 must have nointerpolation mode in order to use GetAttributeAtVertex function.";
    case hlsl::ValidationRule::InstrCreateHandleImmRangeID: return "Local resource must map to global resource.";
    case hlsl::ValidationRule::InstrSignatureOperationNotInEntry: return "Dxil operation for input output signature must be in entryPoints.";
    case hlsl::ValidationRule::InstrMultipleSetMeshOutputCounts: return "SetMeshOUtputCounts cannot be called multiple times.";
    case hlsl::ValidationRule::InstrMissingSetMeshOutputCounts: return "Missing SetMeshOutputCounts call.";
    case hlsl::ValidationRule::InstrNonDominatingSetMeshOutputCounts: return "Non-Dominating SetMeshOutputCounts call.";
    case hlsl::ValidationRule::InstrMultipleGetMeshPayload: return "GetMeshPayload cannot be called multiple times.";
    case hlsl::ValidationRule::InstrNotOnceDispatchMesh: return "DispatchMesh must be called exactly once in an Amplification shader.";
    case hlsl::ValidationRule::InstrNonDominatingDispatchMesh: return "Non-Dominating DispatchMesh call.";
    case hlsl::ValidationRule::TypesNoVector: return "Vector type '%0' is not allowed";
    case hlsl::ValidationRule::TypesDefined: return "Type '%0' is not defined on DXIL primitives";
    case hlsl::ValidationRule::TypesIntWidth: return "Int type '%0' has an invalid width";
    case hlsl::ValidationRule::TypesNoMultiDim: return "Only one dimension allowed for array type";
    case hlsl::ValidationRule::TypesNoPtrToPtr: return "Pointers to pointers, or pointers in structures are not allowed";
    case hlsl::ValidationRule::TypesI8: return "I8 can only used as immediate value for intrinsic";
    case hlsl::ValidationRule::SmName: return "Unknown shader model '%0'";
    case hlsl::ValidationRule::SmDxilVersion: return "Shader model requires Dxil Version %0,%1";
    case hlsl::ValidationRule::SmOpcode: return "Opcode %0 not valid in shader model %1";
    case hlsl::ValidationRule::SmOperand: return "Operand must be defined in target shader model";
    case hlsl::ValidationRule::SmSemantic: return "Semantic '%0' is invalid as %1 %2";
    case hlsl::ValidationRule::SmNoInterpMode: return "Interpolation mode for '%0' is set but should be undefined";
    case hlsl::ValidationRule::SmConstantInterpMode: return "Interpolation mode for '%0' should be constant";
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
    case hlsl::ValidationRule::SmInvalidSamplerFeedbackType: return "Invalid sampler feedback type";
    case hlsl::ValidationRule::SmSampleCountOnlyOn2DMS: return "Only Texture2DMS/2DMSArray could has sample count";
    case hlsl::ValidationRule::SmCounterOnlyOnStructBuf: return "BufferUpdateCounter valid only on structured buffers";
    case hlsl::ValidationRule::SmGSTotalOutputVertexDataRange: return "Declared output vertex count (%0) multiplied by the total number of declared scalar components of output data (%1) equals %2.  This value cannot be greater than %3";
    case hlsl::ValidationRule::SmMultiStreamMustBePoint: return "Multiple GS output streams are used but '%0' is not pointlist";
    case hlsl::ValidationRule::SmCompletePosition: return "Not all elements of SV_Position were written";
    case hlsl::ValidationRule::SmUndefinedOutput: return "Not all elements of output %0 were written";
    case hlsl::ValidationRule::SmCSNoSignatures: return "Compute shaders must not have shader signatures.";
    case hlsl::ValidationRule::SmCBufferTemplateTypeMustBeStruct: return "D3D12 constant/texture buffer template element can only be a struct";
    case hlsl::ValidationRule::SmResourceRangeOverlap: return "Resource %0 with base %1 size %2 overlap with other resource with base %3 size %4 in space %5";
    case hlsl::ValidationRule::SmCBufferOffsetOverlap: return "CBuffer %0 has offset overlaps at %1";
    case hlsl::ValidationRule::SmCBufferElementOverflow: return "CBuffer %0 size insufficient for element at offset %1";
    case hlsl::ValidationRule::SmCBufferArrayOffsetAlignment: return "CBuffer %0 has unaligned array offset at %1";
    case hlsl::ValidationRule::SmOpcodeInInvalidFunction: return "opcode '%0' should only be used in '%1'";
    case hlsl::ValidationRule::SmViewIDNeedsSlot: return "Pixel shader input signature lacks available space for ViewID";
    case hlsl::ValidationRule::Sm64bitRawBufferLoadStore: return "i64/f64 rawBufferLoad/Store overloads are allowed after SM 6.3";
    case hlsl::ValidationRule::SmRayShaderSignatures: return "Ray tracing shader '%0' should not have any shader signatures";
    case hlsl::ValidationRule::SmRayShaderPayloadSize: return "For shader '%0', %1 size is smaller than argument's allocation size";
    case hlsl::ValidationRule::SmMeshShaderMaxVertexCount: return "MS max vertex output count must be [0..%0].  %1 specified";
    case hlsl::ValidationRule::SmMeshShaderMaxPrimitiveCount: return "MS max primitive output count must be [0..%0].  %1 specified";
    case hlsl::ValidationRule::SmMeshShaderPayloadSize: return "For shader '%0', payload size is greater than %1";
    case hlsl::ValidationRule::SmMeshShaderPayloadSizeDeclared: return "For shader '%0', payload size %1 is greater than declared size of %2 bytes";
    case hlsl::ValidationRule::SmMeshShaderOutputSize: return "For shader '%0', vertex plus primitive output size is greater than %1";
    case hlsl::ValidationRule::SmMeshShaderInOutSize: return "For shader '%0', payload plus output size is greater than %1";
    case hlsl::ValidationRule::SmMeshVSigRowCount: return "For shader '%0', vertex output signatures are taking up more than %1 rows";
    case hlsl::ValidationRule::SmMeshPSigRowCount: return "For shader '%0', primitive output signatures are taking up more than %1 rows";
    case hlsl::ValidationRule::SmMeshTotalSigRowCount: return "For shader '%0', vertex and primitive output signatures are taking up more than %1 rows";
    case hlsl::ValidationRule::SmMaxMSSMSize: return "Total Thread Group Shared Memory storage is %0, exceeded %1";
    case hlsl::ValidationRule::SmAmplificationShaderPayloadSize: return "For shader '%0', payload size is greater than %1";
    case hlsl::ValidationRule::SmAmplificationShaderPayloadSizeDeclared: return "For shader '%0', payload size %1 is greater than declared size of %2 bytes";
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
    case hlsl::ValidationRule::DeclResourceInFnSig: return "Function '%0' uses resource in function signature";
    case hlsl::ValidationRule::DeclPayloadStruct: return "Argument '%0' must be a struct type for payload in shader function '%1'";
    case hlsl::ValidationRule::DeclAttrStruct: return "Argument '%0' must be a struct type for attributes in shader function '%1'";
    case hlsl::ValidationRule::DeclParamStruct: return "Argument '%0' must be a struct type for callable shader function '%1'";
    case hlsl::ValidationRule::DeclExtraArgs: return "Extra argument '%0' not allowed for shader function '%1'";
    case hlsl::ValidationRule::DeclShaderReturnVoid: return "Shader function '%0' must have void return type";
    case hlsl::ValidationRule::DeclShaderMissingArg: return "%0 shader '%1' missing required %2 parameter";
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
// Save status like output write for entries.
struct EntryStatus {
  bool hasOutputPosition[DXIL::kNumOutputStreams];
  unsigned OutputPositionMask[DXIL::kNumOutputStreams];
  std::vector<unsigned> outputCols;
  std::vector<unsigned> patchConstOrPrimCols;
  bool m_bCoverageIn, m_bInnerCoverageIn;
  bool hasViewID;
  unsigned domainLocSize;
  EntryStatus(DxilEntryProps &entryProps)
      : m_bCoverageIn(false), m_bInnerCoverageIn(false), hasViewID(false) {
    for (unsigned i = 0; i < DXIL::kNumOutputStreams; i++) {
      hasOutputPosition[i] = false;
      OutputPositionMask[i] = 0;
    }

    outputCols.resize(entryProps.sig.OutputSignature.GetElements().size(), 0);
    patchConstOrPrimCols.resize(
        entryProps.sig.PatchConstOrPrimSignature.GetElements().size(), 0);
  }
};

struct ValidationContext {
  bool Failed = false;
  Module &M;
  Module *pDebugModule;
  DxilModule &DxilMod;
  const DataLayout &DL;
  DiagnosticPrinterRawOStream &DiagPrinter;
  DebugLoc LastDebugLocEmit;
  ValidationRule LastRuleEmit;
  std::unordered_set<Function *> entryFuncCallSet;
  std::unordered_set<Function *> patchConstFuncCallSet;
  std::unordered_map<unsigned, bool> UavCounterIncMap;
  // TODO: save resource map for each createHandle/createHandleForLib.
  std::unordered_map<Value *, DxilResourceBase *> ResMap;
  std::unordered_map<Function *, std::vector<Function*>> PatchConstantFuncMap;
  std::unordered_map<Function *, std::unique_ptr<EntryStatus>> entryStatusMap;
  bool isLibProfile;
  const unsigned kDxilControlFlowHintMDKind;
  const unsigned kDxilPreciseMDKind;
  const unsigned kDxilNonUniformMDKind;
  const unsigned kLLVMLoopMDKind;
  unsigned m_DxilMajor, m_DxilMinor;

  ValidationContext(Module &llvmModule, Module *DebugModule,
                    DxilModule &dxilModule,
                    DiagnosticPrinterRawOStream &DiagPrn)
      : M(llvmModule), pDebugModule(DebugModule), DxilMod(dxilModule),
        DL(llvmModule.getDataLayout()), DiagPrinter(DiagPrn),
        LastRuleEmit((ValidationRule)-1),
        kDxilControlFlowHintMDKind(llvmModule.getContext().getMDKindID(
            DxilMDHelper::kDxilControlFlowHintMDName)),
        kDxilPreciseMDKind(llvmModule.getContext().getMDKindID(
            DxilMDHelper::kDxilPreciseAttributeMDName)),
        kDxilNonUniformMDKind(llvmModule.getContext().getMDKindID(
            DxilMDHelper::kDxilNonUniformAttributeMDName)),
        kLLVMLoopMDKind(llvmModule.getContext().getMDKindID("llvm.loop")) {
    DxilMod.GetDxilVersion(m_DxilMajor, m_DxilMinor);

    for (Function &F : llvmModule.functions()) {
      if (DxilMod.HasDxilEntryProps(&F)) {
        DxilEntryProps &entryProps = DxilMod.GetDxilEntryProps(&F);
        entryStatusMap[&F] = llvm::make_unique<EntryStatus>(entryProps);
      }
    }

    isLibProfile = dxilModule.GetShaderModel()->IsLib();
    BuildResMap();
    // Collect patch constant map.
    if (isLibProfile) {
      for (Function &F : dxilModule.GetModule()->functions()) {
        if (dxilModule.HasDxilEntryProps(&F)) {
          DxilEntryProps &entryProps = dxilModule.GetDxilEntryProps(&F);
          DxilFunctionProps &props = entryProps.props;
          if (props.IsHS()) {
            PatchConstantFuncMap[props.ShaderProps.HS.patchConstantFunc].emplace_back(&F);
          }
        }
      }
    } else {
      Function *Entry = dxilModule.GetEntryFunction();
      if (!dxilModule.HasDxilEntryProps(Entry)) {
        // must have props.
        EmitError(ValidationRule::MetaNoEntryPropsForEntry);
        return;
      }
      DxilEntryProps &entryProps = dxilModule.GetDxilEntryProps(Entry);
      DxilFunctionProps &props = entryProps.props;
      if (props.IsHS()) {
        PatchConstantFuncMap[props.ShaderProps.HS.patchConstantFunc].emplace_back(Entry);
      }
    }
  }

  void PropagateResMap(Value *V, DxilResourceBase *Res) {
    auto it = ResMap.find(V);
    if (it != ResMap.end()) {
      if (it->second != Res) {
        EmitError(ValidationRule::InstrResourceMapToSingleEntry);
      }
    } else {
      ResMap[V] = Res;
      for (User *U : V->users()) {
        if (GEPOperator *GEP = dyn_cast<GEPOperator>(U)) {
          PropagateResMap(U, Res);
        } else if (CallInst *CI = dyn_cast<CallInst>(U)) {
          // Stop propagate on function call.
          DxilInst_CreateHandleForLib hdl(CI);
          if (hdl) {
            ResMap[CI] = Res;
          }
        } else if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
          PropagateResMap(U, Res);
        } else {
          EmitError(ValidationRule::InstrResourceUser);
        }
      }
    }
  }

  void BuildResMap() {
    hlsl::OP *hlslOP = DxilMod.GetOP();

    if (isLibProfile) {
      std::unordered_set<Value *> ResSet;
      // Start from all global variable in resTab.
      for (auto &Res : DxilMod.GetCBuffers())
        PropagateResMap(Res->GetGlobalSymbol(), Res.get());
      for (auto &Res : DxilMod.GetUAVs())
        PropagateResMap(Res->GetGlobalSymbol(), Res.get());
      for (auto &Res : DxilMod.GetSRVs())
        PropagateResMap(Res->GetGlobalSymbol(), Res.get());
      for (auto &Res : DxilMod.GetSamplers())
        PropagateResMap(Res->GetGlobalSymbol(), Res.get());
    } else {
      // Scan all createHandle.
      for (auto &it : hlslOP->GetOpFuncList(DXIL::OpCode::CreateHandle)) {
        Function *F = it.second;
        if (!F)
          continue;
        for (User *U : F->users()) {
          CallInst *CI = cast<CallInst>(U);
          DxilInst_CreateHandle hdl(CI);
          // Validate Class/RangeID/Index.
          Value *resClass = hdl.get_resourceClass();
          if (!isa<ConstantInt>(resClass)) {
            EmitInstrError(CI, ValidationRule::InstrOpConstRange);
            continue;
          }
          Value *rangeIndex = hdl.get_rangeId();
          if (!isa<ConstantInt>(rangeIndex)) {
            EmitInstrError(CI, ValidationRule::InstrOpConstRange);
            continue;
          }

          DxilResourceBase *Res = nullptr;
          unsigned rangeId = hdl.get_rangeId_val();
          switch (
              static_cast<DXIL::ResourceClass>(hdl.get_resourceClass_val())) {
          default:
            EmitInstrError(CI, ValidationRule::InstrOpConstRange);
            continue;
            break;
          case DXIL::ResourceClass::CBuffer:
            if (DxilMod.GetCBuffers().size() > rangeId) {
              Res = &DxilMod.GetCBuffer(rangeId);
            } else {
              // Emit Error.
              EmitInstrError(CI, ValidationRule::InstrOpConstRange);
              continue;
            }
            break;
          case DXIL::ResourceClass::Sampler:
            if (DxilMod.GetSamplers().size() > rangeId) {
              Res = &DxilMod.GetSampler(rangeId);
            } else {
              // Emit Error.
              EmitInstrError(CI, ValidationRule::InstrOpConstRange);
              continue;
            }
            break;
          case DXIL::ResourceClass::SRV:
            if (DxilMod.GetSRVs().size() > rangeId) {
              Res = &DxilMod.GetSRV(rangeId);
            } else {
              // Emit Error.
              EmitInstrError(CI, ValidationRule::InstrOpConstRange);
              continue;
            }
            break;
          case DXIL::ResourceClass::UAV:
            if (DxilMod.GetUAVs().size() > rangeId) {
              Res = &DxilMod.GetUAV(rangeId);
            } else {
              // Emit Error.
              EmitInstrError(CI, ValidationRule::InstrOpConstRange);
              continue;
            }
            break;
          }

          ConstantInt *cIndex = dyn_cast<ConstantInt>(hdl.get_index());
          if (!Res->GetGlobalSymbol()
                   ->getType()
                   ->getPointerElementType()
                   ->isArrayTy()) {
            if (!cIndex) {
              // index must be 0 for none array resource.
              EmitInstrError(CI, ValidationRule::InstrOpConstRange);
              continue;
            }
          }
          if (cIndex) {
            unsigned index = cIndex->getLimitedValue();
            if (index < Res->GetLowerBound() || index > Res->GetUpperBound()) {
              // index out of range.
              EmitInstrError(CI, ValidationRule::InstrOpConstRange);
              continue;
            }
          }

          ResMap[CI] = Res;
        }
      }
    }
  }

  bool HasEntryStatus(Function *F) {
    return entryStatusMap.find(F) != entryStatusMap.end();
  }

  EntryStatus &GetEntryStatus(Function *F) { return *entryStatusMap[F]; }

  DxilResourceBase *GetResourceFromVal(Value *resVal);

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
    EmitFormatError(rule, { GV->getName() });
  }

  // This is the least desirable mechanism, as it has no context.
  void EmitError(ValidationRule rule) {
    DiagPrinter << GetValidationRuleText(rule) << '\n';
    Failed = true;
  }

  void FormatRuleText(std::string &ruleText, ArrayRef<StringRef> args) {
    std::string escapedArg;
    // Consider changing const char * to StringRef
    for (unsigned i = 0; i < args.size(); i++) {
      std::string argIdx = "%" + std::to_string(i);
      StringRef pArg = args[i];
      if (pArg == "")
        pArg = "<null>";
      if (pArg[0] == 1) {
        escapedArg = "";
        raw_string_ostream os(escapedArg);
        dxilutil::PrintEscapedString(pArg, os);
        os.flush();
        pArg = escapedArg;
      }

      std::string::size_type offset = ruleText.find(argIdx);
      if (offset == std::string::npos)
        continue;

      unsigned size = argIdx.size();
      ruleText.replace(offset, size, pArg);
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
    }

    // Print the error header matched by IDE regexes
    DiagPrinter << "error: ";

    // Print the debug location, if any, as matched by IDE regexes
    if (L) {
      L.print(DiagStream());
      DiagPrinter << ": ";
    }

    BasicBlock *BB = I->getParent();
    Function *F = BB->getParent();

    // Printthe instruction
    std::string InstrStr;
    raw_string_ostream InstrStream(InstrStr);
    I->print(InstrStream);
    InstrStream.flush();
    StringRef InstrStrRef = InstrStr;
    InstrStrRef = InstrStrRef.ltrim(); // Ignore indentation
    DiagPrinter << "at '" << InstrStrRef << "'";

    // Print the parent block name
    DiagPrinter << " in block '";
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
        idx++;
      }
      DiagPrinter << "#" << idx;
    }
    DiagPrinter << "'";

    // Print the function name
    DiagPrinter << " of function '" << F->getName() << "': ";

    // Parent will print the message
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
                                    DXIL::ShaderKind SK,
                                    unsigned major,
                                    unsigned minor) {
  unsigned op = (unsigned)opcode;
  /* <py::lines('VALOPCODESM-TEXT')>hctdb_instrhelp.get_valopcode_sm_text()</py>*/
  // VALOPCODESM-TEXT:BEGIN
  // Instructions: ThreadId=93, GroupId=94, ThreadIdInGroup=95,
  // FlattenedThreadIdInGroup=96
  if ((93 <= op && op <= 96))
    return (SK == DXIL::ShaderKind::Compute || SK == DXIL::ShaderKind::Mesh || SK == DXIL::ShaderKind::Amplification);
  // Instructions: DomainLocation=105
  if (op == 105)
    return (SK == DXIL::ShaderKind::Domain);
  // Instructions: LoadOutputControlPoint=103, LoadPatchConstant=104
  if ((103 <= op && op <= 104))
    return (SK == DXIL::ShaderKind::Domain || SK == DXIL::ShaderKind::Hull);
  // Instructions: EmitStream=97, CutStream=98, EmitThenCutStream=99,
  // GSInstanceID=100
  if ((97 <= op && op <= 100))
    return (SK == DXIL::ShaderKind::Geometry);
  // Instructions: PrimitiveID=108
  if (op == 108)
    return (SK == DXIL::ShaderKind::Geometry || SK == DXIL::ShaderKind::Domain || SK == DXIL::ShaderKind::Hull);
  // Instructions: StorePatchConstant=106, OutputControlPointID=107
  if ((106 <= op && op <= 107))
    return (SK == DXIL::ShaderKind::Hull);
  // Instructions: QuadReadLaneAt=122, QuadOp=123
  if ((122 <= op && op <= 123))
    return (SK == DXIL::ShaderKind::Library || SK == DXIL::ShaderKind::Compute || SK == DXIL::ShaderKind::Amplification || SK == DXIL::ShaderKind::Mesh || SK == DXIL::ShaderKind::Pixel);
  // Instructions: WaveIsFirstLane=110, WaveGetLaneIndex=111,
  // WaveGetLaneCount=112, WaveAnyTrue=113, WaveAllTrue=114,
  // WaveActiveAllEqual=115, WaveActiveBallot=116, WaveReadLaneAt=117,
  // WaveReadLaneFirst=118, WaveActiveOp=119, WaveActiveBit=120,
  // WavePrefixOp=121, WaveAllBitCount=135, WavePrefixBitCount=136
  if ((110 <= op && op <= 121) || (135 <= op && op <= 136))
    return (SK == DXIL::ShaderKind::Library || SK == DXIL::ShaderKind::Compute || SK == DXIL::ShaderKind::Amplification || SK == DXIL::ShaderKind::Mesh || SK == DXIL::ShaderKind::Pixel || SK == DXIL::ShaderKind::Vertex || SK == DXIL::ShaderKind::Hull || SK == DXIL::ShaderKind::Domain || SK == DXIL::ShaderKind::Geometry);
  // Instructions: Sample=60, SampleBias=61, SampleCmp=64, CalculateLOD=81,
  // DerivCoarseX=83, DerivCoarseY=84, DerivFineX=85, DerivFineY=86
  if ((60 <= op && op <= 61) || op == 64 || op == 81 || (83 <= op && op <= 86))
    return (SK == DXIL::ShaderKind::Library || SK == DXIL::ShaderKind::Pixel);
  // Instructions: RenderTargetGetSamplePosition=76,
  // RenderTargetGetSampleCount=77, Discard=82, EvalSnapped=87,
  // EvalSampleIndex=88, EvalCentroid=89, SampleIndex=90, Coverage=91,
  // InnerCoverage=92
  if ((76 <= op && op <= 77) || op == 82 || (87 <= op && op <= 92))
    return (SK == DXIL::ShaderKind::Pixel);
  // Instructions: AttributeAtVertex=137
  if (op == 137)
    return (major > 6 || (major == 6 && minor >= 1))
        && (SK == DXIL::ShaderKind::Pixel);
  // Instructions: ViewID=138
  if (op == 138)
    return (major > 6 || (major == 6 && minor >= 1))
        && (SK == DXIL::ShaderKind::Vertex || SK == DXIL::ShaderKind::Hull || SK == DXIL::ShaderKind::Domain || SK == DXIL::ShaderKind::Geometry || SK == DXIL::ShaderKind::Pixel || SK == DXIL::ShaderKind::Mesh);
  // Instructions: RawBufferLoad=139, RawBufferStore=140
  if ((139 <= op && op <= 140))
    return (major > 6 || (major == 6 && minor >= 2));
  // Instructions: CreateHandleForLib=160
  if (op == 160)
    return (major > 6 || (major == 6 && minor >= 3));
  // Instructions: IgnoreHit=155, AcceptHitAndEndSearch=156
  if ((155 <= op && op <= 156))
    return (major > 6 || (major == 6 && minor >= 3))
        && (SK == DXIL::ShaderKind::AnyHit);
  // Instructions: CallShader=159
  if (op == 159)
    return (major > 6 || (major == 6 && minor >= 3))
        && (SK == DXIL::ShaderKind::Library || SK == DXIL::ShaderKind::ClosestHit || SK == DXIL::ShaderKind::RayGeneration || SK == DXIL::ShaderKind::Miss || SK == DXIL::ShaderKind::Callable);
  // Instructions: ReportHit=158
  if (op == 158)
    return (major > 6 || (major == 6 && minor >= 3))
        && (SK == DXIL::ShaderKind::Library || SK == DXIL::ShaderKind::Intersection);
  // Instructions: InstanceID=141, InstanceIndex=142, HitKind=143,
  // ObjectRayOrigin=149, ObjectRayDirection=150, ObjectToWorld=151,
  // WorldToObject=152, PrimitiveIndex=161
  if ((141 <= op && op <= 143) || (149 <= op && op <= 152) || op == 161)
    return (major > 6 || (major == 6 && minor >= 3))
        && (SK == DXIL::ShaderKind::Library || SK == DXIL::ShaderKind::Intersection || SK == DXIL::ShaderKind::AnyHit || SK == DXIL::ShaderKind::ClosestHit);
  // Instructions: RayFlags=144, WorldRayOrigin=147, WorldRayDirection=148,
  // RayTMin=153, RayTCurrent=154
  if (op == 144 || (147 <= op && op <= 148) || (153 <= op && op <= 154))
    return (major > 6 || (major == 6 && minor >= 3))
        && (SK == DXIL::ShaderKind::Library || SK == DXIL::ShaderKind::Intersection || SK == DXIL::ShaderKind::AnyHit || SK == DXIL::ShaderKind::ClosestHit || SK == DXIL::ShaderKind::Miss);
  // Instructions: TraceRay=157
  if (op == 157)
    return (major > 6 || (major == 6 && minor >= 3))
        && (SK == DXIL::ShaderKind::Library || SK == DXIL::ShaderKind::RayGeneration || SK == DXIL::ShaderKind::ClosestHit || SK == DXIL::ShaderKind::Miss);
  // Instructions: DispatchRaysIndex=145, DispatchRaysDimensions=146
  if ((145 <= op && op <= 146))
    return (major > 6 || (major == 6 && minor >= 3))
        && (SK == DXIL::ShaderKind::Library || SK == DXIL::ShaderKind::RayGeneration || SK == DXIL::ShaderKind::Intersection || SK == DXIL::ShaderKind::AnyHit || SK == DXIL::ShaderKind::ClosestHit || SK == DXIL::ShaderKind::Miss || SK == DXIL::ShaderKind::Callable);
  // Instructions: Dot2AddHalf=162, Dot4AddI8Packed=163, Dot4AddU8Packed=164
  if ((162 <= op && op <= 164))
    return (major > 6 || (major == 6 && minor >= 4));
  // Instructions: WriteSamplerFeedbackLevel=176, WriteSamplerFeedbackGrad=177,
  // AllocateRayQuery=178, RayQuery_TraceRayInline=179, RayQuery_Proceed=180,
  // RayQuery_Abort=181, RayQuery_CommitNonOpaqueTriangleHit=182,
  // RayQuery_CommitProceduralPrimitiveHit=183, RayQuery_CommittedStatus=184,
  // RayQuery_CandidateType=185, RayQuery_CandidateObjectToWorld3x4=186,
  // RayQuery_CandidateWorldToObject3x4=187,
  // RayQuery_CommittedObjectToWorld3x4=188,
  // RayQuery_CommittedWorldToObject3x4=189,
  // RayQuery_CandidateProceduralPrimitiveNonOpaque=190,
  // RayQuery_CandidateTriangleFrontFace=191,
  // RayQuery_CommittedTriangleFrontFace=192,
  // RayQuery_CandidateTriangleBarycentrics=193,
  // RayQuery_CommittedTriangleBarycentrics=194, RayQuery_RayFlags=195,
  // RayQuery_WorldRayOrigin=196, RayQuery_WorldRayDirection=197,
  // RayQuery_RayTMin=198, RayQuery_CandidateTriangleRayT=199,
  // RayQuery_CommittedRayT=200, RayQuery_CandidateInstanceIndex=201,
  // RayQuery_CandidateInstanceID=202, RayQuery_CandidateGeometryIndex=203,
  // RayQuery_CandidatePrimitiveIndex=204, RayQuery_CandidateObjectRayOrigin=205,
  // RayQuery_CandidateObjectRayDirection=206,
  // RayQuery_CommittedInstanceIndex=207, RayQuery_CommittedInstanceID=208,
  // RayQuery_CommittedGeometryIndex=209, RayQuery_CommittedPrimitiveIndex=210,
  // RayQuery_CommittedObjectRayOrigin=211,
  // RayQuery_CommittedObjectRayDirection=212,
  // RayQuery_CandidateInstanceContributionToHitGroupIndex=214,
  // RayQuery_CommittedInstanceContributionToHitGroupIndex=215
  if ((176 <= op && op <= 212) || (214 <= op && op <= 215))
    return (major > 6 || (major == 6 && minor >= 5));
  // Instructions: DispatchMesh=173
  if (op == 173)
    return (major > 6 || (major == 6 && minor >= 5))
        && (SK == DXIL::ShaderKind::Amplification);
  // Instructions: WaveMatch=165, WaveMultiPrefixOp=166,
  // WaveMultiPrefixBitCount=167
  if ((165 <= op && op <= 167))
    return (major > 6 || (major == 6 && minor >= 5))
        && (SK == DXIL::ShaderKind::Library || SK == DXIL::ShaderKind::Compute || SK == DXIL::ShaderKind::Amplification || SK == DXIL::ShaderKind::Mesh || SK == DXIL::ShaderKind::Pixel || SK == DXIL::ShaderKind::Vertex || SK == DXIL::ShaderKind::Hull || SK == DXIL::ShaderKind::Domain || SK == DXIL::ShaderKind::Geometry);
  // Instructions: GeometryIndex=213
  if (op == 213)
    return (major > 6 || (major == 6 && minor >= 5))
        && (SK == DXIL::ShaderKind::Library || SK == DXIL::ShaderKind::Intersection || SK == DXIL::ShaderKind::AnyHit || SK == DXIL::ShaderKind::ClosestHit);
  // Instructions: WriteSamplerFeedback=174, WriteSamplerFeedbackBias=175
  if ((174 <= op && op <= 175))
    return (major > 6 || (major == 6 && minor >= 5))
        && (SK == DXIL::ShaderKind::Library || SK == DXIL::ShaderKind::Pixel);
  // Instructions: SetMeshOutputCounts=168, EmitIndices=169, GetMeshPayload=170,
  // StoreVertexOutput=171, StorePrimitiveOutput=172
  if ((168 <= op && op <= 172))
    return (major > 6 || (major == 6 && minor >= 5))
        && (SK == DXIL::ShaderKind::Mesh);
  return true;
  // VALOPCODESM-TEXT:END
}

static unsigned ValidateSignatureRowCol(Instruction *I,
                                        DxilSignatureElement &SE, Value *rowVal,
                                        Value *colVal, EntryStatus &Status,
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
      Status.outputCols[SE.GetID()] |= 1 << col;
    if (SE.IsPatchConstOrPrim())
      Status.patchConstOrPrimCols[SE.GetID()] |= 1 << col;
  }

  return col;
}

static DxilSignatureElement *
ValidateSignatureAccess(Instruction *I, DxilSignature &sig, Value *sigID,
                        Value *rowVal, Value *colVal, EntryStatus &Status,
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

  unsigned col = ValidateSignatureRowCol(I, SE, rowVal, colVal, Status, ValCtx);

  if (isOutput && SE.GetSemantic()->GetKind() == DXIL::SemanticKind::Position) {
    unsigned mask = Status.OutputPositionMask[SE.GetOutputStream()];
    mask |= 1 << col;
    if (SE.GetOutputStream() < DXIL::kNumOutputStreams)
      Status.OutputPositionMask[SE.GetOutputStream()] = mask;
  }
  return &SE;
}

static DXIL::SamplerKind GetSamplerKind(Value *samplerHandle,
                                        ValidationContext &ValCtx) {
  if (!isa<CallInst>(samplerHandle)) {
    ValCtx.EmitError(ValidationRule::InstrHandleNotFromCreateHandle);
    return DXIL::SamplerKind::Invalid;
  }

  DxilResourceBase *Res = ValCtx.GetResourceFromVal(samplerHandle);
  if (!Res) {
      ValCtx.EmitInstrError(cast<CallInst>(samplerHandle),
          ValidationRule::InstrHandleNotFromCreateHandle);
      return DXIL::SamplerKind::Invalid;
  }

  if (Res->GetClass() != DXIL::ResourceClass::Sampler) {
    // must be sampler.
    return DXIL::SamplerKind::Invalid;
  }

  return ((DxilSampler*)Res)->GetSamplerKind();
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
  // TODO: validate ROV is used only in PS.

  DxilResourceBase *Res = ValCtx.GetResourceFromVal(handle);
  if (!Res) {
    ValCtx.EmitInstrError(cast<CallInst>(handle),
                          ValidationRule::InstrHandleNotFromCreateHandle);
    return DXIL::ResourceKind::Invalid;
  }

  ResClass = Res->GetClass();

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

  resIndex = Res->GetID();

  CompTy = ((DxilResource*)Res)->GetCompType().GetKind();

  return Res->GetKind();
}

DxilFieldAnnotation *GetFieldAnnotation(Type *Ty,
                                        DxilTypeSystem &typeSys,
                                        std::deque<unsigned> &offsets) {
  unsigned CurIdx = 1;
  unsigned LastIdx = offsets.size() - 1;
  DxilStructAnnotation *StructAnnot = nullptr;

  for (; CurIdx < offsets.size(); ++CurIdx) {
    if (const StructType *EltST = dyn_cast<StructType>(Ty)) {
      if (DxilStructAnnotation *EltAnnot = typeSys.GetStructAnnotation(EltST)) {
        StructAnnot = EltAnnot;
        Ty = EltST->getElementType(offsets[CurIdx]);
        if (CurIdx == LastIdx) {
          return &StructAnnot->GetFieldAnnotation(offsets[CurIdx]);
        }
      } else {
        return nullptr;
      }
    } else if (const ArrayType *AT = dyn_cast<ArrayType>(Ty)) {
      Ty = AT->getElementType();
      StructAnnot = nullptr;
    } else {
      if (StructAnnot)
        return &StructAnnot->GetFieldAnnotation(offsets[CurIdx]);
    }
  }
  return nullptr;
}


DxilResourceBase *ValidationContext::GetResourceFromVal(Value *resVal) {
  auto it = ResMap.find(resVal);
  if (it != ResMap.end())
    return it->second;
  else
    return nullptr;
}

static DxilResource *GetResource(Value *handle, ValidationContext &ValCtx) {
  if (!isa<CallInst>(handle)) {
    ValCtx.EmitError(ValidationRule::InstrHandleNotFromCreateHandle);
    return nullptr;
  }

  DxilResourceBase *Res = ValCtx.GetResourceFromVal(handle);
  if (!Res) {
    ValCtx.EmitInstrError(cast<CallInst>(handle),
                          ValidationRule::InstrHandleNotFromCreateHandle);
    return nullptr;
  }

  DXIL::ResourceClass ResClass = Res->GetClass();

  switch (ResClass) {
  case DXIL::ResourceClass::SRV:
  case DXIL::ResourceClass::UAV:
    break;
  case DXIL::ResourceClass::CBuffer:
    return nullptr;
  case DXIL::ResourceClass::Sampler:
    return nullptr;
  default:
    // Emit invalid res class
    return nullptr;
  }

  return (DxilResource *)Res;
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
  isSampleCompTy |= compTy == DXIL::ComponentType::F16;
  isSampleCompTy |= compTy == DXIL::ComponentType::SNormF16;
  isSampleCompTy |= compTy == DXIL::ComponentType::UNormF16;
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
  if (!isa<CallInst>(cbHandle)) {
    ValCtx.EmitError(ValidationRule::InstrHandleNotFromCreateHandle);
    return -1;
  }

  DxilResourceBase *Res = ValCtx.GetResourceFromVal(cbHandle);
  if (!Res) {
    ValCtx.EmitInstrError(cast<CallInst>(cbHandle),
                          ValidationRule::InstrHandleNotFromCreateHandle);
    return -1;
  }

  if (Res->GetClass() != DXIL::ResourceClass::CBuffer) {
    ValCtx.EmitInstrError(cast<CallInst>(cbHandle),
                          ValidationRule::InstrCBufferClassForCBufferHandle);
    return -1;
  }

  return ((DxilCBuffer *)Res)->GetSize();
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

static void ValidateSignatureDxilOp(CallInst *CI, DXIL::OpCode opcode,
                                    ValidationContext &ValCtx) {
  Function *F = CI->getParent()->getParent();
  DxilModule &DM = ValCtx.DxilMod;
  bool bIsPatchConstantFunc = false;
  if (!DM.HasDxilEntryProps(F)) {
    auto it = ValCtx.PatchConstantFuncMap.find(F);
    if (it == ValCtx.PatchConstantFuncMap.end()) {
      // Missing entry props.
      ValCtx.EmitInstrError(CI,
                            ValidationRule::InstrSignatureOperationNotInEntry);
      return;
    }
    // Use hull entry instead of patch constant function.
    F = it->second.front();
    bIsPatchConstantFunc = true;
  }
  if (!ValCtx.HasEntryStatus(F)) {
    return;
  }

  EntryStatus &Status = ValCtx.GetEntryStatus(F);
  DxilEntryProps &EntryProps = DM.GetDxilEntryProps(F);
  DxilFunctionProps &props = EntryProps.props;
  DxilEntrySignature &S = EntryProps.sig;

  switch (opcode) {
  case DXIL::OpCode::LoadInput: {
    Value *inputID = CI->getArgOperand(DXIL::OperandIndex::kLoadInputIDOpIdx);
    DxilSignature &inputSig = S.InputSignature;
    Value *row = CI->getArgOperand(DXIL::OperandIndex::kLoadInputRowOpIdx);
    Value *col = CI->getArgOperand(DXIL::OperandIndex::kLoadInputColOpIdx);
    ValidateSignatureAccess(CI, inputSig, inputID, row, col, Status, ValCtx);

    // Check vertexID in ps/vs. and none array input.
    Value *vertexID =
        CI->getArgOperand(DXIL::OperandIndex::kLoadInputVertexIDOpIdx);
    bool usedVertexID = vertexID && !isa<UndefValue>(vertexID);
    if (props.IsVS() || props.IsPS()) {
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
        if (props.IsGS()) {
          DXIL::InputPrimitive inputPrimitive =
              props.ShaderProps.GS.inputPrimitive;
          high = GetNumVertices(inputPrimitive);
        } else if (props.IsDS()) {
          high = props.ShaderProps.DS.inputControlPoints;
        } else if (props.IsHS()) {
          high = props.ShaderProps.HS.inputControlPoints;
        } else {
          ValCtx.EmitFormatError(ValidationRule::SmOpcodeInInvalidFunction,
                                 {"LoadInput", "VS/HS/DS/GS/PS"});
        }
        if (immVertexID < low || immVertexID >= high) {
          std::string range = std::to_string(low) + "~" + std::to_string(high);
          ValCtx.EmitOperandOutOfRange(CI, "VertexID", range,
                                       std::to_string(immVertexID));
        }
      }
    }
  } break;
  case DXIL::OpCode::DomainLocation: {
    Value *colValue =
        CI->getArgOperand(DXIL::OperandIndex::kDomainLocationColOpIdx);
    if (!isa<ConstantInt>(colValue)) {
      // col must be const
      ValCtx.EmitInstrFormatError(CI, ValidationRule::InstrOpConst,
                                  {"Col", "DomainLocation"});
    } else {
      unsigned col = cast<ConstantInt>(colValue)->getLimitedValue();
      if (col >= Status.domainLocSize) {
        ValCtx.EmitError(ValidationRule::SmDomainLocationIdxOOB);
      }
    }
  } break;
  case DXIL::OpCode::StoreOutput:
  case DXIL::OpCode::StoreVertexOutput: 
  case DXIL::OpCode::StorePrimitiveOutput: {
    Value *outputID =
        CI->getArgOperand(DXIL::OperandIndex::kStoreOutputIDOpIdx);
    DxilSignature &outputSig = opcode == DXIL::OpCode::StorePrimitiveOutput ?
      S.PatchConstOrPrimSignature : S.OutputSignature;
    Value *row = CI->getArgOperand(DXIL::OperandIndex::kStoreOutputRowOpIdx);
    Value *col = CI->getArgOperand(DXIL::OperandIndex::kStoreOutputColOpIdx);
    ValidateSignatureAccess(CI, outputSig, outputID, row, col, Status, ValCtx);
  } break;
  case DXIL::OpCode::OutputControlPointID: {
    // Only used in hull shader.
    Function *func = CI->getParent()->getParent();
    // Make sure this is inside hs shader entry function.
    if (!(props.IsHS() &&  F == func)) {
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
    DxilSignature &outputSig = S.OutputSignature;
    Value *row = CI->getArgOperand(DXIL::OperandIndex::kStoreOutputRowOpIdx);
    Value *col = CI->getArgOperand(DXIL::OperandIndex::kStoreOutputColOpIdx);
    ValidateSignatureAccess(CI, outputSig, outputID, row, col, Status, ValCtx);
  } break;
  case DXIL::OpCode::StorePatchConstant: {
    // Only used in patch constant function.
    Function *func = CI->getParent()->getParent();
    if (!bIsPatchConstantFunc) {
      ValCtx.EmitFormatError(ValidationRule::SmOpcodeInInvalidFunction,
                             {"StorePatchConstant", "PatchConstant function"});
    } else {
      auto &hullShaders = ValCtx.PatchConstantFuncMap[func];
      for (Function *F : hullShaders) {
        EntryStatus &Status = ValCtx.GetEntryStatus(F);
        DxilEntryProps &EntryProps = DM.GetDxilEntryProps(F);
        DxilEntrySignature &S = EntryProps.sig;
        Value *outputID =
            CI->getArgOperand(DXIL::OperandIndex::kStoreOutputIDOpIdx);
        DxilSignature &outputSig = S.PatchConstOrPrimSignature;
        Value *row =
            CI->getArgOperand(DXIL::OperandIndex::kStoreOutputRowOpIdx);
        Value *col =
            CI->getArgOperand(DXIL::OperandIndex::kStoreOutputColOpIdx);
        ValidateSignatureAccess(CI, outputSig, outputID, row, col, Status,
                                ValCtx);
      }
    }
  } break;
  case DXIL::OpCode::Coverage:
    Status.m_bCoverageIn = true;
    break;
  case DXIL::OpCode::InnerCoverage:
    Status.m_bInnerCoverageIn = true;
    break;
  case DXIL::OpCode::ViewID:
    Status.hasViewID = true;
    break;
  case DXIL::OpCode::EvalCentroid:
  case DXIL::OpCode::EvalSampleIndex:
  case DXIL::OpCode::EvalSnapped: {
    // Eval* share same operand index with load input.
    Value *inputID = CI->getArgOperand(DXIL::OperandIndex::kLoadInputIDOpIdx);
    DxilSignature &inputSig = S.InputSignature;
    Value *row = CI->getArgOperand(DXIL::OperandIndex::kLoadInputRowOpIdx);
    Value *col = CI->getArgOperand(DXIL::OperandIndex::kLoadInputColOpIdx);
    DxilSignatureElement *pSE =
        ValidateSignatureAccess(CI, inputSig, inputID, row, col, Status, ValCtx);
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
    DxilSignature &inputSig = S.InputSignature;
    Value *row = CI->getArgOperand(DXIL::OperandIndex::kLoadInputRowOpIdx);
    Value *col = CI->getArgOperand(DXIL::OperandIndex::kLoadInputColOpIdx);
    DxilSignatureElement *pSE =
        ValidateSignatureAccess(CI, inputSig, Attribute, row, col, Status, ValCtx);
    if (pSE && pSE->GetInterpolationMode()->GetKind() !=
                   hlsl::InterpolationMode::Kind::Constant) {
      ValCtx.EmitInstrFormatError(
          CI, ValidationRule::InstrAttributeAtVertexNoInterpolation,
          {pSE->GetName()});
    }
  } break;
  case DXIL::OpCode::CutStream:
  case DXIL::OpCode::EmitThenCutStream:
  case DXIL::OpCode::EmitStream: {
    if (props.IsGS()) {
      auto &GS = props.ShaderProps.GS;
      unsigned streamMask = 0;
      for (size_t i = 0; i < _countof(GS.streamPrimitiveTopologies); ++i) {
        if (GS.streamPrimitiveTopologies[i] !=
            DXIL::PrimitiveTopology::Undefined) {
          streamMask |= 1 << i;
        }
      }
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
  case DXIL::OpCode::EmitIndices: {
    if (!props.IsMS()) {
      ValCtx.EmitInstrFormatError(CI, ValidationRule::SmOpcodeInInvalidFunction,
                                  {"EmitIndices", "Mesh shader"});
    }
  } break;
  case DXIL::OpCode::SetMeshOutputCounts: {
    if (!props.IsMS()) {
      ValCtx.EmitInstrFormatError(CI, ValidationRule::SmOpcodeInInvalidFunction,
                                  {"SetMeshOutputCounts", "Mesh shader"});
    }
  } break;
  case DXIL::OpCode::GetMeshPayload: {
    if (!props.IsMS()) {
      ValCtx.EmitInstrFormatError(CI, ValidationRule::SmOpcodeInInvalidFunction,
                                  {"GetMeshPayload", "Mesh shader"});
    }
  } break;
  case DXIL::OpCode::DispatchMesh: {
    if (!props.IsAS()) {
      ValCtx.EmitInstrFormatError(CI, ValidationRule::SmOpcodeInInvalidFunction,
                                  {"DispatchMesh", "Amplification shader"});
    }
  } break;
  default:
    break;
  }

  if (Status.m_bCoverageIn && Status.m_bInnerCoverageIn) {
    ValCtx.EmitError(ValidationRule::SmPSCoverageAndInnerCoverage);
  }
}

static void ValidateImmOperandForMathDxilOp(CallInst *CI, DXIL::OpCode opcode,
                                    ValidationContext &ValCtx) {
  switch (opcode) {
  // Imm input value validation.
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
  default:
    break;
  }
}

static void ValidateResourceDxilOp(CallInst *CI, DXIL::OpCode opcode,
                                   ValidationContext &ValCtx) {
  switch (opcode) {
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
        ValCtx.EmitInstrError(CI, ValidationRule::InstrMipLevelForGetDimension);
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
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceKindForGetDim);
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
      ValCtx.EmitInstrError(CI, ValidationRule::InstrSamplerModeForLOD);
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
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceKindForCalcLOD);
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
  case DXIL::OpCode::BufferStore: {
    DxilInst_BufferStore bufSt(CI);
    DXIL::ComponentType compTy;
    DXIL::ResourceClass resClass;
    unsigned resIndex;
    DXIL::ResourceKind resKind = GetResourceKindAndCompTy(
        bufSt.get_uav(), compTy, resClass, resIndex, ValCtx);

    if (resClass != DXIL::ResourceClass::UAV) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceClassForUAVStore);
    }

    ConstantInt *mask = dyn_cast<ConstantInt>(bufSt.get_mask());
    if (!mask) {
      // Mask for buffer store should be immediate.
      ValCtx.EmitInstrFormatError(CI, ValidationRule::InstrOpConst,
                                  {"Mask", "BufferStore"});
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
        ValCtx.EmitInstrError(CI,
                              ValidationRule::InstrWriteMaskForTypedUAVStore);
      }
      break;
    case DXIL::ResourceKind::StructuredBuffer:
      if (isa<UndefValue>(offset)) {
        ValCtx.EmitInstrError(CI,
                              ValidationRule::InstrCoordinateCountForStructBuf);
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
    DXIL::ResourceKind resKind = GetResourceKindAndCompTy(
        texSt.get_srv(), compTy, resClass, resIndex, ValCtx);

    if (resClass != DXIL::ResourceClass::UAV) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceClassForUAVStore);
    }

    ConstantInt *mask = dyn_cast<ConstantInt>(texSt.get_mask());
    if (!mask) {
      // Mask for buffer store should be immediate.
      ValCtx.EmitInstrFormatError(CI, ValidationRule::InstrOpConst,
                                  {"Mask", "TextureStore"});
      return;
    }
    unsigned uMask = mask->getLimitedValue();
    if (uMask != 0xf) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrWriteMaskForTypedUAVStore);
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
      ValCtx.EmitInstrError(CI,
                            ValidationRule::InstrResourceKindForTextureStore);
      break;
    }
  } break;
  case DXIL::OpCode::BufferLoad: {
    DxilInst_BufferLoad bufLd(CI);
    DXIL::ComponentType compTy;
    DXIL::ResourceClass resClass;
    unsigned resIndex;
    DXIL::ResourceKind resKind = GetResourceKindAndCompTy(
        bufLd.get_srv(), compTy, resClass, resIndex, ValCtx);

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
        ValCtx.EmitInstrError(CI,
                              ValidationRule::InstrCoordinateCountForStructBuf);
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
    DXIL::ResourceKind resKind = GetResourceKindAndCompTy(
        texLd.get_srv(), compTy, resClass, resIndex, ValCtx);

    Value *mipLevel = texLd.get_mipLevelOrSampleCount();

    if (resClass == DXIL::ResourceClass::UAV) {
      bool noOffset = isa<UndefValue>(texLd.get_offset0());
      noOffset &= isa<UndefValue>(texLd.get_offset1());
      noOffset &= isa<UndefValue>(texLd.get_offset2());
      if (!noOffset) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrOffsetOnUAVLoad);
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
        ValCtx.EmitInstrError(CI, ValidationRule::InstrSampleIndexForLoad2DMS);
      }
    } break;
    default:
      ValCtx.EmitInstrError(CI,
                            ValidationRule::InstrResourceKindForTextureLoad);
      break;
    }
  } break;
  case DXIL::OpCode::CBufferLoad: {
    DxilInst_CBufferLoad CBLoad(CI);
    Value *regIndex = CBLoad.get_byteOffset();
    if (ConstantInt *cIndex = dyn_cast<ConstantInt>(regIndex)) {
      int offset = cIndex->getLimitedValue();
      int size = GetCBufSize(CBLoad.get_handle(), ValCtx);
      if (size > 0 && offset >= size) {
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
      if (size > 0 && offset >= size) {
        ValCtx.EmitInstrError(CI, ValidationRule::InstrCBufferOutOfBound);
      }
    }
  } break;
  case DXIL::OpCode::RawBufferLoad: {
    hlsl::OP *hlslOP = ValCtx.DxilMod.GetOP();
    if (!ValCtx.DxilMod.GetShaderModel()->IsSM63Plus()) {
      Type *Ty = hlslOP->GetOverloadType(DXIL::OpCode::RawBufferLoad,
                                         CI->getCalledFunction());
      if (ValCtx.DL.getTypeAllocSizeInBits(Ty) > 32) {
        ValCtx.EmitInstrError(CI, ValidationRule::Sm64bitRawBufferLoadStore);
      }
    }
    DxilInst_RawBufferLoad bufLd(CI);
    DXIL::ComponentType compTy;
    DXIL::ResourceClass resClass;
    unsigned resIndex;
    DXIL::ResourceKind resKind = GetResourceKindAndCompTy(
        bufLd.get_srv(), compTy, resClass, resIndex, ValCtx);

    if (resClass != DXIL::ResourceClass::SRV &&
        resClass != DXIL::ResourceClass::UAV) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceClassForLoad);
    }

    Value *offset = bufLd.get_elementOffset();
    Value *align = bufLd.get_alignment();
    unsigned alignSize = 0;
    if (!isa<ConstantInt>(align)) {
      ValCtx.EmitInstrError(CI,
                            ValidationRule::InstrCoordinateCountForRawTypedBuf);
    } else {
      alignSize = bufLd.get_alignment_val();
    }
    switch (resKind) {
    case DXIL::ResourceKind::RawBuffer:
      if (!isa<UndefValue>(offset)) {
        ValCtx.EmitInstrError(
            CI, ValidationRule::InstrCoordinateCountForRawTypedBuf);
      }
      break;
    case DXIL::ResourceKind::StructuredBuffer:
      if (isa<UndefValue>(offset)) {
        ValCtx.EmitInstrError(CI,
                              ValidationRule::InstrCoordinateCountForStructBuf);
      }
      break;
    default:
      ValCtx.EmitInstrError(
          CI, ValidationRule::InstrResourceKindForBufferLoadStore);
      break;
    }
  } break;
  case DXIL::OpCode::RawBufferStore: {
    hlsl::OP *hlslOP = ValCtx.DxilMod.GetOP();
    if (!ValCtx.DxilMod.GetShaderModel()->IsSM63Plus()) {
      Type *Ty = hlslOP->GetOverloadType(DXIL::OpCode::RawBufferStore,
                                         CI->getCalledFunction());
      if (ValCtx.DL.getTypeAllocSizeInBits(Ty) > 32) {
        ValCtx.EmitInstrError(CI, ValidationRule::Sm64bitRawBufferLoadStore);
      }
    }
    DxilInst_RawBufferStore bufSt(CI);
    DXIL::ComponentType compTy;
    DXIL::ResourceClass resClass;
    unsigned resIndex;
    DXIL::ResourceKind resKind = GetResourceKindAndCompTy(
        bufSt.get_uav(), compTy, resClass, resIndex, ValCtx);

    if (resClass != DXIL::ResourceClass::UAV) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceClassForUAVStore);
    }

    ConstantInt *mask = dyn_cast<ConstantInt>(bufSt.get_mask());
    if (!mask) {
      // Mask for buffer store should be immediate.
      ValCtx.EmitInstrFormatError(CI, ValidationRule::InstrOpConst,
                                  {"Mask", "BufferStore"});
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

    Value *offset = bufSt.get_elementOffset();
    Value *align = bufSt.get_alignment();
    unsigned alignSize = 0;
    if (!isa<ConstantInt>(align)) {
      ValCtx.EmitInstrError(CI,
                            ValidationRule::InstrCoordinateCountForRawTypedBuf);
    } else {
      alignSize = bufSt.get_alignment_val();
    }
    switch (resKind) {
    case DXIL::ResourceKind::RawBuffer:
      if (!isa<UndefValue>(offset)) {
        ValCtx.EmitInstrError(
            CI, ValidationRule::InstrCoordinateCountForRawTypedBuf);
      }
      break;
    case DXIL::ResourceKind::StructuredBuffer:
      if (isa<UndefValue>(offset)) {
        ValCtx.EmitInstrError(CI,
                              ValidationRule::InstrCoordinateCountForStructBuf);
      }
      break;
    default:
      ValCtx.EmitInstrError(
          CI, ValidationRule::InstrResourceKindForBufferLoadStore);
      break;
    }
  } break;
  case DXIL::OpCode::TraceRay: {
    DxilInst_TraceRay traceRay(CI);
    Value *hdl = traceRay.get_AccelerationStructure();
    DxilResourceBase *Res = ValCtx.GetResourceFromVal(hdl);
    if (!Res) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceKindForTraceRay);
      return;
    }
    if (Res->GetKind() != DXIL::ResourceKind::RTAccelerationStructure) {
      ValCtx.EmitInstrError(CI, ValidationRule::InstrResourceKindForTraceRay);
    }
  } break;
  default:
    break;
  }
}

static void ValidateDxilOperationCallInProfile(CallInst *CI,
                                               DXIL::OpCode opcode,
                                               const ShaderModel *pSM,
                                               ValidationContext &ValCtx) {
  DXIL::ShaderKind shaderKind = pSM ? pSM->GetKind() : DXIL::ShaderKind::Invalid;
  llvm::Function *F = CI->getParent()->getParent();
  if (DXIL::ShaderKind::Library == shaderKind) {
    if (ValCtx.DxilMod.HasDxilFunctionProps(F))
      shaderKind = ValCtx.DxilMod.GetDxilFunctionProps(F).shaderKind;
    else if (ValCtx.DxilMod.IsPatchConstantShader(F))
      shaderKind = DXIL::ShaderKind::Hull;
  }

  // These shader models are treted like compute
  bool isCSLike = shaderKind == DXIL::ShaderKind::Compute ||
                  shaderKind == DXIL::ShaderKind::Mesh ||
                  shaderKind == DXIL::ShaderKind::Amplification;
  // Is called from a library function
  bool isLibFunc = shaderKind == DXIL::ShaderKind::Library;

  switch (opcode) {
  // Imm input value validation.
  case DXIL::OpCode::Asin:
  case DXIL::OpCode::Acos:
  case DXIL::OpCode::Log:
  case DXIL::OpCode::DerivFineX:
  case DXIL::OpCode::DerivFineY:
  case DXIL::OpCode::DerivCoarseX:
  case DXIL::OpCode::DerivCoarseY:
    ValidateImmOperandForMathDxilOp(CI, opcode, ValCtx);
    break;
  // Resource validation.
  case DXIL::OpCode::GetDimensions:
  case DXIL::OpCode::CalculateLOD:
  case DXIL::OpCode::TextureGather:
  case DXIL::OpCode::TextureGatherCmp:
  case DXIL::OpCode::Sample:
  case DXIL::OpCode::SampleCmp:
  case DXIL::OpCode::SampleCmpLevelZero:
  case DXIL::OpCode::SampleBias:
  case DXIL::OpCode::SampleGrad:
  case DXIL::OpCode::SampleLevel:
  case DXIL::OpCode::CheckAccessFullyMapped:
  case DXIL::OpCode::BufferStore:
  case DXIL::OpCode::TextureStore:
  case DXIL::OpCode::BufferLoad:
  case DXIL::OpCode::TextureLoad:
  case DXIL::OpCode::CBufferLoad:
  case DXIL::OpCode::CBufferLoadLegacy:
  case DXIL::OpCode::RawBufferLoad:
  case DXIL::OpCode::RawBufferStore:
    ValidateResourceDxilOp(CI, opcode, ValCtx);
    break;
  // Input output.
  case DXIL::OpCode::LoadInput:
  case DXIL::OpCode::DomainLocation:
  case DXIL::OpCode::StoreOutput:
  case DXIL::OpCode::StoreVertexOutput:
  case DXIL::OpCode::StorePrimitiveOutput:
  case DXIL::OpCode::OutputControlPointID:
  case DXIL::OpCode::LoadOutputControlPoint:
  case DXIL::OpCode::StorePatchConstant:
  case DXIL::OpCode::Coverage:
  case DXIL::OpCode::InnerCoverage:
  case DXIL::OpCode::ViewID:
  case DXIL::OpCode::EvalCentroid:
  case DXIL::OpCode::EvalSampleIndex:
  case DXIL::OpCode::EvalSnapped:
  case DXIL::OpCode::AttributeAtVertex:
  case DXIL::OpCode::EmitStream:
  case DXIL::OpCode::EmitThenCutStream:
  case DXIL::OpCode::CutStream:
    ValidateSignatureDxilOp(CI, opcode, ValCtx);
    break;
  // Special.
  case DXIL::OpCode::BufferUpdateCounter: {
    DxilInst_BufferUpdateCounter updateCounter(CI);
    DxilResource *res = GetResource(updateCounter.get_uav(), ValCtx);

    if (!res) {
      return;
    }

    if (res->GetClass() != DXIL::ResourceClass::UAV) {
      ValCtx.EmitInstrError(CI,
                               ValidationRule::InstrBufferUpdateCounterOnUAV);
    }

    if (res->GetKind() != DXIL::ResourceKind::StructuredBuffer) {
      ValCtx.EmitInstrError(CI,
                               ValidationRule::SmCounterOnlyOnStructBuf);
    }

    if (!res->HasCounter()) {
      ValCtx.EmitInstrError(
          CI, ValidationRule::InstrBufferUpdateCounterOnResHasCounter);
    }

    Value *inc = updateCounter.get_inc();
    if (ConstantInt *cInc = dyn_cast<ConstantInt>(inc)) {
      bool isInc = cInc->getLimitedValue() == 1;
      if (!ValCtx.isLibProfile) {
        unsigned resIndex = res->GetLowerBound();
        if (ValCtx.UavCounterIncMap.count(resIndex)) {
          if (isInc != ValCtx.UavCounterIncMap[resIndex]) {
            ValCtx.EmitInstrError(CI, ValidationRule::InstrOnlyOneAllocConsume);
          }
        } else {
          ValCtx.UavCounterIncMap[resIndex] = isInc;
        }
      } else {
        // TODO: validate ValidationRule::InstrOnlyOneAllocConsume for lib
        // profile.
      }
    } else {
        ValCtx.EmitInstrFormatError(CI, ValidationRule::InstrOpConst, {"inc", "BufferUpdateCounter"});
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
    const unsigned ut =
        static_cast<unsigned>(DXIL::BarrierMode::UAVFenceThreadGroup);
    unsigned barrierMode = cMode->getLimitedValue();

    if (isCSLike || isLibFunc) {
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
  case DXIL::OpCode::CreateHandleForLib:
    if (!ValCtx.isLibProfile) {
      ValCtx.EmitFormatError(ValidationRule::SmOpcodeInInvalidFunction,
                             {"CreateHandleForLib", "Library"});
    }
    break;
  default:
    // TODO: make sure every opcode is checked.
    // Skip opcodes don't need special check.
    break;
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
  if (!IsDxilFunction(F) && !ValCtx.isLibProfile) {
    ValCtx.EmitGlobalValueError(F, ValidationRule::DeclDxilFnExtern);
    return;
  }

  if (F->use_empty()) {
    ValCtx.EmitGlobalValueError(F, ValidationRule::DeclUsedExternalFunction);
    return;
  }

  const ShaderModel *pSM = ValCtx.DxilMod.GetShaderModel();
  OP *hlslOP = ValCtx.DxilMod.GetOP();
  bool isDxilOp = OP::IsDxilOpFunc(F);
  Type *voidTy = Type::getVoidTy(F->getContext());
  for (User *user : F->users()) {
    CallInst *CI = dyn_cast<CallInst>(user);
    if (!CI) {
      ValCtx.EmitGlobalValueError(F, ValidationRule::DeclFnIsCalled);
      continue;
    }

    // Skip call to external user defined function
    if (!isDxilOp)
      continue;

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

    unsigned major = pSM->GetMajor();
    unsigned minor = pSM->GetMinor();
    if (ValCtx.isLibProfile) {
      Function *callingFunction = CI->getParent()->getParent();
      DXIL::ShaderKind SK = DXIL::ShaderKind::Library;
      if (ValCtx.DxilMod.HasDxilFunctionProps(callingFunction))
        SK = ValCtx.DxilMod.GetDxilFunctionProps(callingFunction).shaderKind;
      else if (ValCtx.DxilMod.IsPatchConstantShader(callingFunction))
        SK = DXIL::ShaderKind::Hull;
      if (!ValidateOpcodeInProfile(dxilOpcode, SK, major, minor)) {
        // Opcode not available in profile.
        // produces: "lib_6_3(ps)", or "lib_6_3(anyhit)" for shader types
        // Or: "lib_6_3(lib)" for library function
        std::string shaderModel = pSM->GetName();
        shaderModel += std::string("(") + ShaderModel::GetKindName(SK) + ")";
        ValCtx.EmitInstrFormatError(CI, ValidationRule::SmOpcode,
          { hlslOP->GetOpCodeName(dxilOpcode), shaderModel });
        continue;
      }
    } else {
      if (!ValidateOpcodeInProfile(dxilOpcode, pSM->GetKind(), major, minor)) {
        // Opcode not available in profile.
        ValCtx.EmitInstrFormatError(CI, ValidationRule::SmOpcode,
          { hlslOP->GetOpCodeName(dxilOpcode), pSM->GetName() });
        continue;
      }
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
  return (1 <= op && op <= 3) || (8 <= op && op <= 29) || (31 <= op && op <= 41) || (44 <= op && op <= 50) || op == 57;
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

// outer type may be: [ptr to][1 dim array of]( UDT struct | scalar )
// inner type (UDT struct member) may be: [N dim array of]( UDT struct | scalar )
// scalar type may be: ( float(16|32|64) | int(16|32|64) )
static bool ValidateType(Type *Ty, ValidationContext &ValCtx, bool bInner = false) {
  DXASSERT_NOMSG(Ty != nullptr);
  if (Ty->isPointerTy()) {
    Type *EltTy = Ty->getPointerElementType();
    if (bInner || EltTy->isPointerTy()) {
      ValCtx.EmitTypeError(Ty, ValidationRule::TypesNoPtrToPtr);
      return false;
    }
    Ty = EltTy;
  }
  if (Ty->isArrayTy()) {
    Type *EltTy = Ty->getArrayElementType();
    if (!bInner && isa<ArrayType>(EltTy)) {
      // Outermost array should be converted to single-dim,
      // but arrays inside struct are allowed to be multi-dim
      ValCtx.EmitTypeError(Ty, ValidationRule::TypesNoMultiDim);
      return false;
    }
    while (EltTy->isArrayTy())
      EltTy = EltTy->getArrayElementType();
    Ty = EltTy;
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
      if (!ValidateType(e, ValCtx, /*bInner*/true)) {
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
  // Lib profile allow all types except those hit ValidationRule::InstrDxilStructUser.
  if (ValCtx.isLibProfile)
    return true;

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
  (void)(DxilMod);
  Type *Ty = V->getType();
  if (Ty->isIntegerTy()) {
    return 16 == Ty->getIntegerBitWidth();
  }
  return Ty->isHalfTy();
}

static void ValidateMsIntrinsics(Function *F,
                                 ValidationContext &ValCtx,
                                 CallInst *setMeshOutputCounts,
                                 CallInst *getMeshPayload) {
  if (ValCtx.DxilMod.HasDxilFunctionProps(F)) {
    DXIL::ShaderKind shaderKind = ValCtx.DxilMod.GetDxilFunctionProps(F).shaderKind;
    if (shaderKind != DXIL::ShaderKind::Mesh)
      return;
  } else {
    return;
  }

  DominatorTreeAnalysis DTA;
  DominatorTree DT = DTA.run(*F);

  for (auto b = F->begin(), bend = F->end(); b != bend; ++b) {
    bool foundSetMeshOutputCountsInCurrentBB = false;
    for (auto i = b->begin(), iend = b->end(); i != iend; ++i) {
      llvm::Instruction &I = *i;

      // Calls to external functions.
      CallInst *CI = dyn_cast<CallInst>(&I);
      if (CI) {
        Function *FCalled = CI->getCalledFunction();
        if (FCalled->isDeclaration()) {
          // External function validation will diagnose.
          if (!IsDxilFunction(FCalled)) {
            continue;
          }

          if (CI == setMeshOutputCounts) {
            foundSetMeshOutputCountsInCurrentBB = true;
          }
          Value *opcodeVal = CI->getOperand(0);
          ConstantInt *OpcodeConst = dyn_cast<ConstantInt>(opcodeVal);
          unsigned opcode = OpcodeConst->getLimitedValue();
          DXIL::OpCode dxilOpcode = (DXIL::OpCode)opcode;

          if (dxilOpcode == DXIL::OpCode::StoreVertexOutput ||
              dxilOpcode == DXIL::OpCode::StorePrimitiveOutput ||
              dxilOpcode == DXIL::OpCode::EmitIndices) {
            if (setMeshOutputCounts == nullptr) {
              ValCtx.EmitInstrError(&I, ValidationRule::InstrMissingSetMeshOutputCounts);
            } else if (!foundSetMeshOutputCountsInCurrentBB &&
                       !DT.dominates(setMeshOutputCounts->getParent(), I.getParent())) {
              ValCtx.EmitInstrError(&I, ValidationRule::InstrNonDominatingSetMeshOutputCounts);
            }
          }
        }
      }
    }
  }

  if (getMeshPayload) {
    PointerType *payloadPTy = cast<PointerType>(getMeshPayload->getType());
    StructType *payloadTy = cast<StructType>(payloadPTy->getPointerElementType());
    const DataLayout &DL = F->getParent()->getDataLayout();
    unsigned payloadSize = DL.getTypeAllocSize(payloadTy);

    DxilFunctionProps &prop = ValCtx.DxilMod.GetDxilFunctionProps(F);

    if (payloadSize > DXIL::kMaxMSASPayloadBytes ||
        prop.ShaderProps.MS.payloadSizeInBytes > DXIL::kMaxMSASPayloadBytes) {
      ValCtx.EmitFormatError(ValidationRule::SmMeshShaderPayloadSize,
        { F->getName(), std::to_string(DXIL::kMaxMSASPayloadBytes) });
    }

    if (prop.ShaderProps.MS.payloadSizeInBytes < payloadSize) {
      ValCtx.EmitFormatError(ValidationRule::SmMeshShaderPayloadSizeDeclared,
        { F->getName(), std::to_string(payloadSize),
          std::to_string(prop.ShaderProps.MS.payloadSizeInBytes) });
    }
  }
}

static void ValidateAsIntrinsics(Function *F, ValidationContext &ValCtx, CallInst *dispatchMesh) {
  if (ValCtx.DxilMod.HasDxilFunctionProps(F)) {
    DXIL::ShaderKind shaderKind = ValCtx.DxilMod.GetDxilFunctionProps(F).shaderKind;
    if (shaderKind != DXIL::ShaderKind::Amplification)
      return;

    if (dispatchMesh) {
      DxilInst_DispatchMesh dispatchMeshCall(dispatchMesh);
      Value *operandVal = dispatchMeshCall.get_payload();
      Type *payloadTy = operandVal->getType();
      const DataLayout &DL = F->getParent()->getDataLayout();
      unsigned payloadSize = DL.getTypeAllocSize(payloadTy);

      DxilFunctionProps &prop = ValCtx.DxilMod.GetDxilFunctionProps(F);

      if (payloadSize > DXIL::kMaxMSASPayloadBytes ||
          prop.ShaderProps.AS.payloadSizeInBytes > DXIL::kMaxMSASPayloadBytes) {
        ValCtx.EmitFormatError(
            ValidationRule::SmAmplificationShaderPayloadSize,
            {F->getName(), std::to_string(DXIL::kMaxMSASPayloadBytes)});
      }

      if (prop.ShaderProps.AS.payloadSizeInBytes < payloadSize) {
        ValCtx.EmitFormatError(ValidationRule::SmAmplificationShaderPayloadSizeDeclared,
          { F->getName(), std::to_string(payloadSize),
            std::to_string(prop.ShaderProps.AS.payloadSizeInBytes) });
      }
    }

  }
  else {
    return;
  }

  if (dispatchMesh == nullptr) {
    ValCtx.EmitError(ValidationRule::InstrNotOnceDispatchMesh);
    return;
  }

  PostDominatorTree PDT;
  PDT.runOnFunction(*F);

  if (!PDT.dominates(dispatchMesh->getParent(), &F->getEntryBlock())) {
    ValCtx.EmitInstrError(dispatchMesh, ValidationRule::InstrNonDominatingDispatchMesh);
  }

  Function *dispatchMeshFunc = dispatchMesh->getCalledFunction();
  FunctionType *dispatchMeshFuncTy = dispatchMeshFunc->getFunctionType();
  PointerType *payloadPTy = cast<PointerType>(dispatchMeshFuncTy->getParamType(4));
  StructType *payloadTy = cast<StructType>(payloadPTy->getPointerElementType());
  const DataLayout &DL = F->getParent()->getDataLayout();
  unsigned payloadSize = DL.getTypeAllocSize(payloadTy);

  if (payloadSize > DXIL::kMaxMSASPayloadBytes) {
    ValCtx.EmitFormatError(ValidationRule::SmAmplificationShaderPayloadSize,
      { F->getName(), std::to_string(DXIL::kMaxMSASPayloadBytes) });
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

static void ValidateNonUniformMetadata(Instruction &I, MDNode *pMD,
                                       ValidationContext &ValCtx) {
  if (!ValCtx.isLibProfile) {
    ValCtx.EmitMetaError(pMD, ValidationRule::MetaUsed);
  }
  if (!isa<GetElementPtrInst>(I)) {
    ValCtx.EmitMetaError(pMD, ValidationRule::MetaWellFormed);
  }
  if (pMD->getNumOperands() != 1) {
    ValCtx.EmitMetaError(pMD, ValidationRule::MetaWellFormed);
  }
  uint64_t val;
  if (!GetNodeOperandAsInt(ValCtx, pMD, 0, &val)) {
    ValCtx.EmitMetaError(pMD, ValidationRule::MetaWellFormed);
  }
  if (val != 1) {
    ValCtx.EmitMetaError(pMD, ValidationRule::MetaValueRange);
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
    } else if (MD.first == ValCtx.kDxilNonUniformMDKind) {
      ValidateNonUniformMetadata(*I, MD.second, ValCtx);
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

static bool IsLLVMInstructionAllowedForLib(Instruction &I, ValidationContext &ValCtx) {
  if (!(ValCtx.isLibProfile ||
        ValCtx.DxilMod.GetShaderModel()->IsMS() ||
        ValCtx.DxilMod.GetShaderModel()->IsAS()))
    return false;
  switch (I.getOpcode()) {
  case Instruction::InsertElement:
  case Instruction::ExtractElement:
  case Instruction::ShuffleVector:
    return true;
  case Instruction::Unreachable:
    if (Instruction *Prev = I.getPrevNode()) {
      if (CallInst *CI = dyn_cast<CallInst>(Prev)) {
        Function *F = CI->getCalledFunction();
        if (IsDxilFunction(F) &&
            F->hasFnAttribute(Attribute::AttrKind::NoReturn)) {
          return true;
        }
      }
    }
    return false;
  default:
    return false;
  }
}

static void ValidateFunctionBody(Function *F, ValidationContext &ValCtx) {
  bool SupportsMinPrecision =
      ValCtx.DxilMod.GetGlobalFlags() & DXIL::kEnableMinPrecision;
  SmallVector<CallInst *, 16> gradientOps;
  SmallVector<CallInst *, 16> barriers;
  CallInst *setMeshOutputCounts = nullptr;
  CallInst *getMeshPayload = nullptr;
  CallInst *dispatchMesh = nullptr;
  for (auto b = F->begin(), bend = F->end(); b != bend; ++b) {
    for (auto i = b->begin(), iend = b->end(); i != iend; ++i) {
      llvm::Instruction &I = *i;

      if (I.hasMetadata()) {

        ValidateInstructionMetadata(&I, ValCtx);
      }

      // Instructions must be allowed.
      if (!IsLLVMInstructionAllowed(I)) {
        if (!IsLLVMInstructionAllowedForLib(I, ValCtx)) {
          ValCtx.EmitInstrError(&I, ValidationRule::InstrAllowed);
          continue;
        }
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

          if (dxilOpcode == DXIL::OpCode::SetMeshOutputCounts) {
            // validate the call count of SetMeshOutputCounts
            if (setMeshOutputCounts != nullptr) {
              ValCtx.EmitInstrError(&I, ValidationRule::InstrMultipleSetMeshOutputCounts);
            }
            setMeshOutputCounts = CI;
          }

          if (dxilOpcode == DXIL::OpCode::GetMeshPayload) {
            // validate the call count of GetMeshPayload
            if (getMeshPayload != nullptr) {
              ValCtx.EmitInstrError(&I, ValidationRule::InstrMultipleGetMeshPayload);
            }
            getMeshPayload = CI;
          }

          if (dxilOpcode == DXIL::OpCode::DispatchMesh) {
            // validate the call count of DispatchMesh
            if (dispatchMesh != nullptr) {
              ValCtx.EmitInstrError(&I, ValidationRule::InstrNotOnceDispatchMesh);
            }
            dispatchMesh = CI;
          }
        }
        continue;
      }

      for (Value *op : I.operands()) {
        if (isa<UndefValue>(op)) {
          bool legalUndef = isa<PHINode>(&I);
          if (InsertElementInst *InsertInst = dyn_cast<InsertElementInst>(&I)) {
            legalUndef = op == I.getOperand(0);
          }
          if (ShuffleVectorInst *Shuf = dyn_cast<ShuffleVectorInst>(&I)) {
            legalUndef = op == I.getOperand(1);
          }

          if (!legalUndef)
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
        if ((isa<StructType>(FromTy) || isa<StructType>(ToTy)) && !ValCtx.isLibProfile) {
          ValCtx.EmitInstrError(Cast, ValidationRule::InstrStructBitCast);
          continue;
        }

        bool IsMinPrecisionTy =
            (ValCtx.DL.getTypeStoreSize(FromTy) < 4 ||
             ValCtx.DL.getTypeStoreSize(ToTy) < 4) &&
            ValCtx.DxilMod.GetUseMinPrecision();
        if (IsMinPrecisionTy) {
          ValCtx.EmitInstrError(Cast, ValidationRule::InstrMinPrecisonBitCast);
        }
      } break;
      }

      if (PointerType *PT = dyn_cast<PointerType>(I.getType())) {
        if (PT->getAddressSpace() == DXIL::kTGSMAddrSpace) {
          if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I)) {
            Value *Ptr = GEP->getPointerOperand();
            // Allow inner constant GEP
            if (isa<ConstantExpr>(Ptr) && isa<GEPOperator>(Ptr))
              Ptr = cast<GEPOperator>(Ptr)->getPointerOperand();
            if (!isa<GlobalVariable>(Ptr)) {
              ValCtx.EmitInstrError(
                  &I, ValidationRule::InstrFailToResloveTGSMPointer);
            }
          } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(&I)) {
            Value *Ptr = BCI->getOperand(0);
            // Allow inner constant GEP
            if (isa<ConstantExpr>(Ptr) && isa<GEPOperator>(Ptr))
              Ptr = cast<GEPOperator>(Ptr)->getPointerOperand();
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

  ValidateMsIntrinsics(F, ValCtx, setMeshOutputCounts, getMeshPayload);

  ValidateAsIntrinsics(F, ValCtx, dispatchMesh);
}

static void ValidateFunction(Function &F, ValidationContext &ValCtx) {
  if (F.isDeclaration()) {
    ValidateExternalFunction(&F, ValCtx);
    if (F.isIntrinsic() || IsDxilFunction(&F))
      return;
  } else {
    DXIL::ShaderKind shaderKind = DXIL::ShaderKind::Library;
    bool isShader = ValCtx.DxilMod.HasDxilFunctionProps(&F);
    unsigned numUDTShaderArgs = 0;
    if (isShader) {
      shaderKind = ValCtx.DxilMod.GetDxilFunctionProps(&F).shaderKind;
      switch (shaderKind) {
      case DXIL::ShaderKind::AnyHit:
      case DXIL::ShaderKind::ClosestHit:
        numUDTShaderArgs = 2;
        break;
      case DXIL::ShaderKind::Miss:
      case DXIL::ShaderKind::Callable:
        numUDTShaderArgs = 1;
        break;
      default:
        break;
      }
    } else {
      isShader = ValCtx.DxilMod.IsPatchConstantShader(&F);
    }

    // Entry function should not have parameter.
    if (isShader && 0 == numUDTShaderArgs && !F.arg_empty())
      ValCtx.EmitFormatError(ValidationRule::FlowFunctionCall, { F.getName() });

    // Shader functions should return void.
    if (isShader && !F.getReturnType()->isVoidTy())
      ValCtx.EmitFormatError(ValidationRule::DeclShaderReturnVoid, { F.getName() });

    auto ArgFormatError = [&](Argument &arg, ValidationRule rule) {
      if (arg.hasName())
        ValCtx.EmitFormatError(rule, { arg.getName().str(), F.getName() });
      else
        ValCtx.EmitFormatError(rule, { std::to_string(arg.getArgNo()), F.getName() });
    };

    // Validate parameter type.
    unsigned numArgs = 0;
    for (auto &arg : F.args()) {
      Type *argTy = arg.getType();
      if (argTy->isPointerTy())
        argTy = argTy->getPointerElementType();

      numArgs++;
      if (numUDTShaderArgs) {
        if (arg.getArgNo() >= numUDTShaderArgs) {
          ArgFormatError(arg, ValidationRule::DeclExtraArgs);
        } else if (!argTy->isStructTy()) {
          ArgFormatError(arg,
            shaderKind == DXIL::ShaderKind::Callable
              ? ValidationRule::DeclParamStruct
              : arg.getArgNo() == 0 ? ValidationRule::DeclPayloadStruct
                                    : ValidationRule::DeclAttrStruct);
        }
        continue;
      }

      while (argTy->isArrayTy()) {
        argTy = argTy->getArrayElementType();
      }

      if (argTy->isStructTy() && !ValCtx.isLibProfile) {
        ArgFormatError(arg, ValidationRule::DeclFnFlattenParam);
        break;
      }
    }

    if (numArgs < numUDTShaderArgs) {
      StringRef argType[2] = { shaderKind == DXIL::ShaderKind::Callable ?
                                  "params" : "payload", "attributes" };
      for (unsigned i = numArgs; i < numUDTShaderArgs; i++) {
        ValCtx.EmitFormatError(ValidationRule::DeclShaderMissingArg,
          { ShaderModel::GetKindName(shaderKind), F.getName(), argType[i] });
      }
    }

    ValidateFunctionBody(&F, ValCtx);
  }

  // function params & return type must not contain resources
  if (dxilutil::ContainsHLSLObjectType(F.getReturnType())) {
    ValCtx.EmitGlobalValueError(&F, ValidationRule::DeclResourceInFnSig);
    return;
  }
  for (auto &Arg : F.args()) {
    if (dxilutil::ContainsHLSLObjectType(Arg.getType())) {
      ValCtx.EmitGlobalValueError(&F, ValidationRule::DeclResourceInFnSig);
      return;
    }
  }

  // TODO: Remove attribute for lib?
  if (!ValCtx.isLibProfile)
    ValidateFunctionAttribute(&F, ValCtx);

  if (F.hasMetadata()) {
    ValidateFunctionMetadata(&F, ValCtx);
  }
}

static void ValidateGlobalVariable(GlobalVariable &GV,
                                   ValidationContext &ValCtx) {
  bool isInternalGV =
      dxilutil::IsStaticGlobal(&GV) || dxilutil::IsSharedMemoryGlobal(&GV);

  if (ValCtx.isLibProfile) {
    auto isCBufferGlobal = [&](const std::vector<std::unique_ptr<DxilCBuffer>> &ResTab) -> bool {
      for (auto &Res : ResTab)
        if (Res->GetGlobalSymbol() == &GV)
          return true;
      return false;
    };
    auto isResourceGlobal = [&](const std::vector<std::unique_ptr<DxilResource>> &ResTab) -> bool {
      for (auto &Res : ResTab)
        if (Res->GetGlobalSymbol() == &GV)
          return true;
      return false;
    };
    auto isSamplerGlobal = [&](const std::vector<std::unique_ptr<DxilSampler>> &ResTab) -> bool {
      for (auto &Res : ResTab)
        if (Res->GetGlobalSymbol() == &GV)
          return true;
      return false;
    };

    bool isRes = isCBufferGlobal(ValCtx.DxilMod.GetCBuffers());
    isRes |= isResourceGlobal(ValCtx.DxilMod.GetUAVs());
    isRes |= isResourceGlobal(ValCtx.DxilMod.GetSRVs());
    isRes |= isSamplerGlobal(ValCtx.DxilMod.GetSamplers());
    isInternalGV |= isRes;
  }

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

  if (M.GetShaderModel()->IsMS()) {
    if (TGSMSize > DXIL::kMaxMSSMSize) {
      ValCtx.EmitFormatError(ValidationRule::SmMaxMSSMSize,
                             { std::to_string(TGSMSize),
                               std::to_string(DXIL::kMaxMSSMSize) });
    }
  } else if (TGSMSize > DXIL::kMaxTGSMSize) {
    ValCtx.EmitFormatError(ValidationRule::SmMaxTGSMSize,
                           { std::to_string(TGSMSize),
                             std::to_string(DXIL::kMaxTGSMSize) });
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
        if ((majorVer == 1 && minorVer <= DXIL::kDxilMinor) &&
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

static void ValidateBitcode(ValidationContext &ValCtx) {
  if (llvm::verifyModule(ValCtx.M, &ValCtx.DiagStream())) {
    ValCtx.EmitError(ValidationRule::BitcodeValid);
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
      if (!name.startswith_lower("llvm.")) {
        ValCtx.EmitFormatError(ValidationRule::MetaKnown, {name.str()});
      }
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
  if (ValCtx.isLibProfile && !res.IsAllocated()) {
    // Skip unallocated resource for library.
    return;
  }
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
  case DXIL::ResourceKind::RTAccelerationStructure:
    // TODO: check profile.
    break;
  case DXIL::ResourceKind::FeedbackTexture2D:
  case DXIL::ResourceKind::FeedbackTexture2DArray:
    if (res.GetSamplerFeedbackType() >= DXIL::SamplerFeedbackType::LastEntry)
      ValCtx.EmitResourceError(&res, ValidationRule::SmInvalidSamplerFeedbackType);
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
    if (!res.IsStructuredBuffer() && !res.IsRawBuffer() && !res.IsFeedbackTexture())
      ValCtx.EmitResourceError(&res, ValidationRule::SmInvalidResourceCompType);
    break;
  }

  if (res.IsStructuredBuffer()) {
    unsigned stride = res.GetElementStride();
    bool alignedTo4Bytes = (stride & 3) == 0;
    if (!alignedTo4Bytes && ValCtx.M.GetDxilModule().GetUseMinPrecision()) {
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
  DXASSERT(((base + 15) & ~(0xf)) == base, "otherwise, base for struct is not aligned");
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
      if (((offset + 15) & ~(0xf)) != offset) {
        ValCtx.EmitFormatError(
            ValidationRule::SmCBufferArrayOffsetAlignment,
            {cbName, std::to_string(offset)});
        continue;
      }
      unsigned arrayCount = 1;
      while (isa<ArrayType>(EltTy)) {
        arrayCount *= EltTy->getArrayNumElements();
        EltTy = EltTy->getArrayElementType();
      }

      DxilStructAnnotation *EltAnnotation = nullptr;
      if (StructType *EltST = dyn_cast<StructType>(EltTy))
        EltAnnotation = typeSys.GetStructAnnotation(EltST);

      unsigned alignedEltSize = ((EltSize + 15) & ~(0xf));
      unsigned arraySize = ((arrayCount - 1) * alignedEltSize) + EltSize;
      bOutOfBound = (offset + arraySize) > cbSize;

      if (!bOutOfBound) {
        // If we didn't care about gaps where elements could be placed with user offsets,
        // we could: recurse once if EltAnnotation, then allocate the rest if arrayCount > 1

        unsigned arrayBase = base + offset;
        if (!EltAnnotation) {
          if (nullptr != constAllocator.Insert(
                &fieldAnnotation, arrayBase, arrayBase + arraySize - 1)) {
            ValCtx.EmitFormatError(
                ValidationRule::SmCBufferOffsetOverlap,
                {cbName, std::to_string(arrayBase)});
          }
        } else {
          for (unsigned idx = 0; idx < arrayCount; idx++) {
            CollectCBufferRanges(EltAnnotation, constAllocator,
                                 arrayBase, typeSys, cbName, ValCtx);
            arrayBase += alignedEltSize;
          }
        }
      }
    } else {
      StructType *EltST = cast<StructType>(EltTy);
      unsigned structBase = base + offset;
      bOutOfBound = (offset + EltSize) > cbSize;
      if (!bOutOfBound) {
        if (DxilStructAnnotation *EltAnnotation = typeSys.GetStructAnnotation(EltST)) {
          CollectCBufferRanges(EltAnnotation, constAllocator,
                               structBase, typeSys, cbName, ValCtx);
        } else {
          if (EltSize > 0 && nullptr != constAllocator.Insert(
                &fieldAnnotation, structBase, structBase + EltSize - 1)) {
            ValCtx.EmitFormatError(
                ValidationRule::SmCBufferOffsetOverlap,
                {cbName, std::to_string(structBase)});
          }
        }
      }
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
  SpacesAllocator<unsigned, DxilResourceBase> uavAllocator;

  for (auto &uav : uavs) {
    if (uav->IsROV()) {
      if (!ValCtx.DxilMod.GetShaderModel()->IsPS() && !ValCtx.isLibProfile) {
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
  // TODO: validate flags foreach entry.
  if (ValCtx.isLibProfile)
    return;

  ShaderFlags calcFlags;
  ValCtx.DxilMod.CollectShaderFlagsForModule(calcFlags);
  const uint64_t mask = ShaderFlags::GetShaderFlagsRawForCollection();
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
  bool compBool = false;

  switch (compKind) {
  case CompType::Kind::U64: compWidth = 64; compInt = true; break;
  case CompType::Kind::I64: compWidth = 64; compInt = true; break;
  case CompType::Kind::U32: compWidth = 32; compInt = true; break;
  case CompType::Kind::I32: compWidth = 32; compInt = true; break;
  case CompType::Kind::U16: compWidth = 16; compInt = true; break;
  case CompType::Kind::I16: compWidth = 16; compInt = true; break;
  case CompType::Kind::I1: compWidth = 1; compBool = true; break;
  case CompType::Kind::F64: compWidth = 64; compFloat = true; break;
  case CompType::Kind::F32: compWidth = 32; compFloat = true; break;
  case CompType::Kind::F16: compWidth = 16; compFloat = true; break;
  case CompType::Kind::SNormF64: compWidth = 64; compFloat = true; break;
  case CompType::Kind::SNormF32: compWidth = 32; compFloat = true; break;
  case CompType::Kind::SNormF16: compWidth = 16; compFloat = true; break;
  case CompType::Kind::UNormF64: compWidth = 64; compFloat = true; break;
  case CompType::Kind::UNormF32: compWidth = 32; compFloat = true; break;
  case CompType::Kind::UNormF16: compWidth = 16; compFloat = true; break;
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
  case DXIL::SemanticKind::ShadingRate:
    if ((compKind != CompType::Kind::U32 && compKind != CompType::Kind::U16) || SE.GetCols() != 1) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticCompType,
                             {SE.GetSemantic()->GetName(), "uint"});
    }
    break;
  case DXIL::SemanticKind::CullPrimitive: {
    if (!(compBool && compWidth == 1) || SE.GetCols() != 1) {
      ValCtx.EmitFormatError(ValidationRule::MetaSemanticCompType,
                             {SE.GetSemantic()->GetName(), "bool"});
    }
  } break;
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
    // and ClipDistance/CullDistance, which have other custom constraints.
    if (!bIsTessfactor && !bIsClipCull && SE.GetRows() > 1) {
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
                              EntryStatus &Status,
                              unsigned maxScalars) {
  DxilSignatureAllocator allocator[DXIL::kNumOutputStreams] = {
      {32, ValCtx.DxilMod.GetUseMinPrecision()},
      {32, ValCtx.DxilMod.GetUseMinPrecision()},
      {32, ValCtx.DxilMod.GetUseMinPrecision()},
      {32, ValCtx.DxilMod.GetUseMinPrecision()}};
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
      for (unsigned rowIdx = 0; rowIdx < E->GetRows(); rowIdx++)
        clipcullRowSet[streamId].insert(E->GetStartRow() + rowIdx);
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
      Status.hasOutputPosition[E->GetOutputStream()] = true;
    }
  }

  if (Status.hasViewID && S.IsInput() && ValCtx.DxilMod.GetShaderModel()->GetKind() == DXIL::ShaderKind::Pixel) {
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

static void ValidateConstantInterpModeSignature(ValidationContext &ValCtx, const DxilSignature &S) {
  for (auto &E : S.GetElements()) {
    if (!E->GetInterpolationMode()->IsConstant()) {
      ValCtx.EmitSignatureError(E.get(), ValidationRule::SmConstantInterpMode);
    }
  }
}

static void ValidateEntrySignatures(ValidationContext &ValCtx,
                                    const DxilEntryProps &entryProps,
                                    EntryStatus &Status,
                                    Function &F) {
  const DxilFunctionProps &props = entryProps.props;
  const DxilEntrySignature &S = entryProps.sig;

  if (props.IsRay()) {
    // No signatures allowed
    if (!S.InputSignature.GetElements().empty() ||
        !S.OutputSignature.GetElements().empty() ||
        !S.PatchConstOrPrimSignature.GetElements().empty()) {
      ValCtx.EmitFormatError(ValidationRule::SmRayShaderSignatures, { F.getName() });
    }

    // Validate payload/attribute/params sizes
    unsigned payloadSize = 0;
    unsigned attrSize = 0;
    auto itPayload = F.arg_begin();
    auto itAttr = itPayload;
    if (itAttr != F.arg_end())
      itAttr++;
    DataLayout DL(F.getParent());
    switch (props.shaderKind) {
    case DXIL::ShaderKind::AnyHit:
    case DXIL::ShaderKind::ClosestHit:
      if (itAttr != F.arg_end()) {
        Type *Ty = itAttr->getType();
        if (Ty->isPointerTy())
          Ty = Ty->getPointerElementType();
        attrSize = (unsigned)std::min(DL.getTypeAllocSize(Ty), (uint64_t)UINT_MAX);
      }
    case DXIL::ShaderKind::Miss:
    case DXIL::ShaderKind::Callable:
      if (itPayload != F.arg_end()) {
        Type *Ty = itPayload->getType();
        if (Ty->isPointerTy())
          Ty = Ty->getPointerElementType();
        payloadSize = (unsigned)std::min(DL.getTypeAllocSize(Ty), (uint64_t)UINT_MAX);
      }
      break;
    }
    if (props.ShaderProps.Ray.payloadSizeInBytes < payloadSize) {
      ValCtx.EmitFormatError(ValidationRule::SmRayShaderPayloadSize,
        { F.getName(), props.IsCallable() ? "params" : "payload" });
    }
    if (props.ShaderProps.Ray.attributeSizeInBytes < attrSize) {
      ValCtx.EmitFormatError(ValidationRule::SmRayShaderPayloadSize,
        { F.getName(), "attribute" });
    }
    return;
  }

  bool isPS = props.IsPS();
  bool isVS = props.IsVS();
  bool isGS = props.IsGS();
  bool isCS = props.IsCS();
  bool isMS = props.IsMS();

  if (isPS) {
    // PS output no interp mode.
    ValidateNoInterpModeSignature(ValCtx, S.OutputSignature);
  } else if (isVS) {
    // VS input no interp mode.
    ValidateNoInterpModeSignature(ValCtx, S.InputSignature);
  }

  if (isMS) {
    // primitive output constant interp mode.
    ValidateConstantInterpModeSignature(ValCtx, S.PatchConstOrPrimSignature);
  } else {
    // patch constant no interp mode.
    ValidateNoInterpModeSignature(ValCtx, S.PatchConstOrPrimSignature);
  }

  unsigned maxInputScalars = DXIL::kMaxInputTotalScalars;
  unsigned maxOutputScalars = 0;
  unsigned maxPatchConstantScalars = 0;

  switch (props.shaderKind) {
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
  case DXIL::ShaderKind::Mesh:
    maxOutputScalars = DXIL::kMaxOutputTotalScalars;
    maxPatchConstantScalars = DXIL::kMaxOutputTotalScalars;
    break;
  case DXIL::ShaderKind::Amplification:
  default:
    break;
  }

  ValidateSignature(ValCtx, S.InputSignature, Status, maxInputScalars);
  ValidateSignature(ValCtx, S.OutputSignature, Status, maxOutputScalars);
  ValidateSignature(ValCtx, S.PatchConstOrPrimSignature, Status,
                    maxPatchConstantScalars);

  if (isPS) {
    // Gather execution information.
    hlsl::PSExecutionInfo PSExec;
    for (auto &E :S.InputSignature.GetElements()) {
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

    for (auto &E : S.OutputSignature.GetElements()) {
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
    const DxilSignature &outputSig = S.OutputSignature;
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
    unsigned maxVertexCount = props.ShaderProps.GS.maxVertexCount;
    unsigned outputScalarCount = 0;
    const DxilSignature &outSig = S.OutputSignature;
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
      if (!S.InputSignature.GetElements().empty() ||
          !S.OutputSignature.GetElements().empty() ||
          !S.PatchConstOrPrimSignature.GetElements().empty()) {
        ValCtx.EmitError(ValidationRule::SmCSNoSignatures);
      }
  }

  if (isMS) {
    unsigned VertexSignatureRows = S.OutputSignature.GetRowCount();
    if (VertexSignatureRows > DXIL::kMaxMSVSigRows) {
      ValCtx.EmitFormatError(
        ValidationRule::SmMeshVSigRowCount,
        { F.getName(), std::to_string(DXIL::kMaxMSVSigRows) });
    }
    unsigned PrimitiveSignatureRows = S.PatchConstOrPrimSignature.GetRowCount();
    if (PrimitiveSignatureRows > DXIL::kMaxMSPSigRows) {
      ValCtx.EmitFormatError(
        ValidationRule::SmMeshPSigRowCount,
        { F.getName(), std::to_string(DXIL::kMaxMSPSigRows) });
    }
    if (VertexSignatureRows + PrimitiveSignatureRows > DXIL::kMaxMSTotalSigRows) {
      ValCtx.EmitFormatError(
        ValidationRule::SmMeshTotalSigRowCount,
        { F.getName(), std::to_string(DXIL::kMaxMSTotalSigRows) });
    }

    const unsigned kScalarSizeForMSAttributes = 4;
    #define ALIGN32(n) (((n) + 31) & ~31)
    unsigned maxAlign32VertexCount = ALIGN32(props.ShaderProps.MS.maxVertexCount);
    unsigned maxAlign32PrimitiveCount = ALIGN32(props.ShaderProps.MS.maxPrimitiveCount);
    unsigned totalOutputScalars = 0;
    for (auto &SE : S.OutputSignature.GetElements()) {
      totalOutputScalars += SE->GetRows() * SE->GetCols() * maxAlign32VertexCount;
    }
    for (auto &SE : S.PatchConstOrPrimSignature.GetElements()) {
      totalOutputScalars += SE->GetRows() * SE->GetCols() * maxAlign32PrimitiveCount;
    }

    if (totalOutputScalars*kScalarSizeForMSAttributes > DXIL::kMaxMSOutputTotalBytes) {
      ValCtx.EmitFormatError(
        ValidationRule::SmMeshShaderOutputSize,
        { F.getName(), std::to_string(DXIL::kMaxMSOutputTotalBytes) });
    }

    unsigned totalInputOutputBytes = totalOutputScalars*kScalarSizeForMSAttributes + props.ShaderProps.MS.payloadSizeInBytes;
    if (totalInputOutputBytes > DXIL::kMaxMSInputOutputTotalBytes) {
      ValCtx.EmitFormatError(
        ValidationRule::SmMeshShaderInOutSize,
        { F.getName(), std::to_string(DXIL::kMaxMSInputOutputTotalBytes) });
    }
  }
}

static void ValidateEntrySignatures(ValidationContext &ValCtx) {
  DxilModule &DM = ValCtx.DxilMod;
  if (ValCtx.isLibProfile) {
    for (Function &F : DM.GetModule()->functions()) {
      if (DM.HasDxilEntryProps(&F)) {
        DxilEntryProps &entryProps = DM.GetDxilEntryProps(&F);
        EntryStatus &Status = ValCtx.GetEntryStatus(&F);
        ValidateEntrySignatures(ValCtx, entryProps, Status, F);
      }
    }
  } else {
    Function *Entry = DM.GetEntryFunction();
    if (!DM.HasDxilEntryProps(Entry)) {
      // must have props.
      ValCtx.EmitError(ValidationRule::MetaNoEntryPropsForEntry);
      return;
    }
    EntryStatus &Status = ValCtx.GetEntryStatus(Entry);
    DxilEntryProps &entryProps = DM.GetDxilEntryProps(Entry);
    ValidateEntrySignatures(ValCtx, entryProps, Status, *Entry);
  }
}

static void CheckPatchConstantSemantic(ValidationContext &ValCtx,
                                       const DxilEntryProps &EntryProps,
                                       EntryStatus &Status) {
  const DxilFunctionProps &props = EntryProps.props;
  bool isHS = props.IsHS();

  DXIL::TessellatorDomain domain =
      isHS ? props.ShaderProps.HS.domain : props.ShaderProps.DS.domain;

  const DxilSignature &patchConstantSig = EntryProps.sig.PatchConstOrPrimSignature;

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

  Status.domainLocSize = 0;

  switch (domain) {
  case DXIL::TessellatorDomain::IsoLine:
    domainName = "IsoLine";
    edgeSize = kIsolineEdgeSize;
    insideSize = kIsolineInsideSize;
    Status.domainLocSize = kIsolineDomainLocSize;
    break;
  case DXIL::TessellatorDomain::Tri:
    domainName = "Tri";
    edgeSize = kTriEdgeSize;
    insideSize = kTriInsideSize;
    Status.domainLocSize = kTriDomainLocSize;
    break;
  case DXIL::TessellatorDomain::Quad:
    domainName = "Quad";
    edgeSize = kQuadEdgeSize;
    insideSize = kQuadInsideSize;
    Status.domainLocSize = kQuadDomainLocSize;
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
                                std::to_string(SE->GetCols()), domainName,
                                std::to_string(edgeSize)});
      }
    } else if (kind == kInsideSemantic) {
      bFoundInsideSemantic = true;
      if (SE->GetRows() != insideSize || SE->GetCols() > 1) {
        ValCtx.EmitFormatError(
            ValidationRule::SmInsideTessFactorSizeMatchDomain,
            {std::to_string(SE->GetRows()), std::to_string(SE->GetCols()),
             domainName, std::to_string(insideSize)});
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

static void ValidatePassThruHS(ValidationContext &ValCtx,
                               const DxilEntryProps &entryProps, Function *F) {
  // Check pass thru HS.
  if (F->isDeclaration()) {
    const auto &props = entryProps.props;
    if (props.IsHS()) {
      const auto &HS = props.ShaderProps.HS;
      if (HS.inputControlPoints < HS.outputControlPoints) {
        ValCtx.EmitError(ValidationRule::SmHullPassThruControlPointCountMatch);
      }

      // Check declared control point outputs storage amounts are ok to pass
      // through (less output storage than input for control points).
      const DxilSignature &outSig = entryProps.sig.OutputSignature;
      unsigned totalOutputCPScalars = 0;
      for (auto &SE : outSig.GetElements()) {
        totalOutputCPScalars += SE->GetRows() * SE->GetCols();
      }
      if (totalOutputCPScalars * HS.outputControlPoints >
          DXIL::kMaxHSOutputControlPointsTotalScalars) {
        ValCtx.EmitError(ValidationRule::SmOutputControlPointsTotalScalars);
      }
    } else {
      ValCtx.EmitError(ValidationRule::MetaEntryFunction);
    }
  }
}

static void ValidateEntryProps(ValidationContext &ValCtx,
                               const DxilEntryProps &entryProps,
                               EntryStatus &Status) {
  const DxilFunctionProps &props = entryProps.props;
  DXIL::ShaderKind ShaderType = props.shaderKind;

  if (ShaderType == DXIL::ShaderKind::Compute) {
    const auto &CS = props.ShaderProps.CS;
    unsigned x = CS.numThreads[0];
    unsigned y = CS.numThreads[1];
    unsigned z = CS.numThreads[2];

    unsigned threadsInGroup = x * y * z;

    if ((x < DXIL::kMinCSThreadGroupX) || (x > DXIL::kMaxCSThreadGroupX)) {
      ValCtx.EmitFormatError(ValidationRule::SmThreadGroupChannelRange,
                             {"X", std::to_string(x),
                              std::to_string(DXIL::kMinCSThreadGroupX),
                              std::to_string(DXIL::kMaxCSThreadGroupX)});
    }
    if ((y < DXIL::kMinCSThreadGroupY) || (y > DXIL::kMaxCSThreadGroupY)) {
      ValCtx.EmitFormatError(ValidationRule::SmThreadGroupChannelRange,
                             {"Y", std::to_string(y),
                              std::to_string(DXIL::kMinCSThreadGroupY),
                              std::to_string(DXIL::kMaxCSThreadGroupY)});
    }
    if ((z < DXIL::kMinCSThreadGroupZ) || (z > DXIL::kMaxCSThreadGroupZ)) {
      ValCtx.EmitFormatError(ValidationRule::SmThreadGroupChannelRange,
                             {"Z", std::to_string(z),
                              std::to_string(DXIL::kMinCSThreadGroupZ),
                              std::to_string(DXIL::kMaxCSThreadGroupZ)});
    }

    if (threadsInGroup > DXIL::kMaxCSThreadsPerGroup) {
      ValCtx.EmitFormatError(ValidationRule::SmMaxTheadGroup,
                             {std::to_string(threadsInGroup),
                              std::to_string(DXIL::kMaxCSThreadsPerGroup)});
    }

    // type of threadID, thread group ID take care by DXIL operation overload
    // check.
  } else if (ShaderType == DXIL::ShaderKind::Mesh) {
    const auto &MS = props.ShaderProps.MS;
    unsigned x = MS.numThreads[0];
    unsigned y = MS.numThreads[1];
    unsigned z = MS.numThreads[2];

    unsigned threadsInGroup = x * y * z;

    if ((x < DXIL::kMinMSASThreadGroupX) || (x > DXIL::kMaxMSASThreadGroupX)) {
      ValCtx.EmitFormatError(ValidationRule::SmThreadGroupChannelRange,
                             {"X", std::to_string(x),
                              std::to_string(DXIL::kMinMSASThreadGroupX),
                              std::to_string(DXIL::kMaxMSASThreadGroupX)});
    }
    if ((y < DXIL::kMinMSASThreadGroupY) || (y > DXIL::kMaxMSASThreadGroupY)) {
      ValCtx.EmitFormatError(ValidationRule::SmThreadGroupChannelRange,
                             {"Y", std::to_string(y),
                              std::to_string(DXIL::kMinMSASThreadGroupY),
                              std::to_string(DXIL::kMaxMSASThreadGroupY)});
    }
    if ((z < DXIL::kMinMSASThreadGroupZ) || (z > DXIL::kMaxMSASThreadGroupZ)) {
      ValCtx.EmitFormatError(ValidationRule::SmThreadGroupChannelRange,
                             {"Z", std::to_string(z),
                              std::to_string(DXIL::kMinMSASThreadGroupZ),
                              std::to_string(DXIL::kMaxMSASThreadGroupZ)});
    }

    if (threadsInGroup > DXIL::kMaxMSASThreadsPerGroup) {
      ValCtx.EmitFormatError(ValidationRule::SmMaxTheadGroup,
                             {std::to_string(threadsInGroup),
                              std::to_string(DXIL::kMaxMSASThreadsPerGroup)});
    }

    // type of threadID, thread group ID take care by DXIL operation overload
    // check.

    unsigned maxVertexCount = MS.maxVertexCount;
    if (maxVertexCount > DXIL::kMaxMSOutputVertexCount) {
      ValCtx.EmitFormatError(
        ValidationRule::SmMeshShaderMaxVertexCount,
          { std::to_string(DXIL::kMaxMSOutputVertexCount),
            std::to_string(maxVertexCount) });
    }

    unsigned maxPrimitiveCount = MS.maxPrimitiveCount;
    if (maxPrimitiveCount > DXIL::kMaxMSOutputPrimitiveCount) {
      ValCtx.EmitFormatError(
        ValidationRule::SmMeshShaderMaxPrimitiveCount,
          { std::to_string(DXIL::kMaxMSOutputPrimitiveCount),
            std::to_string(maxPrimitiveCount) });
    }
  } else if (ShaderType == DXIL::ShaderKind::Amplification) {
    const auto &AS = props.ShaderProps.AS;
    unsigned x = AS.numThreads[0];
    unsigned y = AS.numThreads[1];
    unsigned z = AS.numThreads[2];

    unsigned threadsInGroup = x * y * z;

    if ((x < DXIL::kMinMSASThreadGroupX) || (x > DXIL::kMaxMSASThreadGroupX)) {
      ValCtx.EmitFormatError(ValidationRule::SmThreadGroupChannelRange,
                             {"X", std::to_string(x),
                              std::to_string(DXIL::kMinMSASThreadGroupX),
                              std::to_string(DXIL::kMaxMSASThreadGroupX)});
    }
    if ((y < DXIL::kMinMSASThreadGroupY) || (y > DXIL::kMaxMSASThreadGroupY)) {
      ValCtx.EmitFormatError(ValidationRule::SmThreadGroupChannelRange,
                             {"Y", std::to_string(y),
                              std::to_string(DXIL::kMinMSASThreadGroupY),
                              std::to_string(DXIL::kMaxMSASThreadGroupY)});
    }
    if ((z < DXIL::kMinMSASThreadGroupZ) || (z > DXIL::kMaxMSASThreadGroupZ)) {
      ValCtx.EmitFormatError(ValidationRule::SmThreadGroupChannelRange,
                             {"Z", std::to_string(z),
                              std::to_string(DXIL::kMinMSASThreadGroupZ),
                              std::to_string(DXIL::kMaxMSASThreadGroupZ)});
    }

    if (threadsInGroup > DXIL::kMaxMSASThreadsPerGroup) {
      ValCtx.EmitFormatError(ValidationRule::SmMaxTheadGroup,
                             {std::to_string(threadsInGroup),
                              std::to_string(DXIL::kMaxMSASThreadsPerGroup)});
    }

    // type of threadID, thread group ID take care by DXIL operation overload
    // check.
  } else if (ShaderType == DXIL::ShaderKind::Domain) {
    const auto &DS = props.ShaderProps.DS;
    DXIL::TessellatorDomain domain = DS.domain;
    if (domain >= DXIL::TessellatorDomain::LastEntry)
      domain = DXIL::TessellatorDomain::Undefined;
    unsigned inputControlPointCount = DS.inputControlPoints;

    if (inputControlPointCount > DXIL::kMaxIAPatchControlPointCount) {
      ValCtx.EmitFormatError(
          ValidationRule::SmDSInputControlPointCountRange,
          {std::to_string(DXIL::kMaxIAPatchControlPointCount),
           std::to_string(inputControlPointCount)});
    }
    if (domain == DXIL::TessellatorDomain::Undefined) {
      ValCtx.EmitError(ValidationRule::SmValidDomain);
    }
    CheckPatchConstantSemantic(ValCtx, entryProps, Status);
  } else if (ShaderType == DXIL::ShaderKind::Hull) {
    const auto &HS = props.ShaderProps.HS;
    DXIL::TessellatorDomain domain = HS.domain;
    if (domain >= DXIL::TessellatorDomain::LastEntry)
      domain = DXIL::TessellatorDomain::Undefined;
    unsigned inputControlPointCount = HS.inputControlPoints;
    if (inputControlPointCount == 0) {
      const DxilSignature &inputSig = entryProps.sig.InputSignature;
      if (!inputSig.GetElements().empty()) {
        ValCtx.EmitError(ValidationRule::SmZeroHSInputControlPointWithInput);
      }
    } else if (inputControlPointCount > DXIL::kMaxIAPatchControlPointCount) {
      ValCtx.EmitFormatError(
          ValidationRule::SmHSInputControlPointCountRange,
          {std::to_string(DXIL::kMaxIAPatchControlPointCount),
           std::to_string(inputControlPointCount)});
    }

    unsigned outputControlPointCount = HS.outputControlPoints;
    if (outputControlPointCount > DXIL::kMaxIAPatchControlPointCount) {
      ValCtx.EmitFormatError(
          ValidationRule::SmOutputControlPointCountRange,
          {std::to_string(DXIL::kMaxIAPatchControlPointCount),
           std::to_string(outputControlPointCount)});
    }
    if (domain == DXIL::TessellatorDomain::Undefined) {
      ValCtx.EmitError(ValidationRule::SmValidDomain);
    }
    DXIL::TessellatorPartitioning partition = HS.partition;
    if (partition == DXIL::TessellatorPartitioning::Undefined) {
      ValCtx.EmitError(ValidationRule::MetaTessellatorPartition);
    }

    DXIL::TessellatorOutputPrimitive tessOutputPrimitive = HS.outputPrimitive;
    if (tessOutputPrimitive == DXIL::TessellatorOutputPrimitive::Undefined ||
        tessOutputPrimitive == DXIL::TessellatorOutputPrimitive::LastEntry) {
      ValCtx.EmitError(ValidationRule::MetaTessellatorOutputPrimitive);
    }

    float maxTessFactor = HS.maxTessFactor;
    if (maxTessFactor < DXIL::kHSMaxTessFactorLowerBound ||
        maxTessFactor > DXIL::kHSMaxTessFactorUpperBound) {
      ValCtx.EmitFormatError(ValidationRule::MetaMaxTessFactor,
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

    CheckPatchConstantSemantic(ValCtx, entryProps, Status);
  } else if (ShaderType == DXIL::ShaderKind::Geometry) {
    const auto &GS = props.ShaderProps.GS;
    unsigned maxVertexCount = GS.maxVertexCount;
    if (maxVertexCount > DXIL::kMaxGSOutputVertexCount) {
      ValCtx.EmitFormatError(ValidationRule::SmGSOutputVertexCountRange,
                             {std::to_string(DXIL::kMaxGSOutputVertexCount),
                              std::to_string(maxVertexCount)});
    }

    unsigned instanceCount = GS.instanceCount;
    if (instanceCount > DXIL::kMaxGSInstanceCount || instanceCount < 1) {
      ValCtx.EmitFormatError(ValidationRule::SmGSInstanceCountRange,
                             {std::to_string(DXIL::kMaxGSInstanceCount),
                              std::to_string(instanceCount)});
    }

    DXIL::PrimitiveTopology topo = DXIL::PrimitiveTopology::Undefined;
    bool bTopoMismatch = false;
    for (size_t i = 0; i < _countof(GS.streamPrimitiveTopologies); ++i) {
      if (GS.streamPrimitiveTopologies[i] !=
          DXIL::PrimitiveTopology::Undefined) {
        if (topo == DXIL::PrimitiveTopology::Undefined)
          topo = GS.streamPrimitiveTopologies[i];
        else if (topo != GS.streamPrimitiveTopologies[i]) {
          bTopoMismatch = true;
          break;
        }
      }
    }
    if (bTopoMismatch)
      topo = DXIL::PrimitiveTopology::Undefined;
    switch (topo) {
    case DXIL::PrimitiveTopology::PointList:
    case DXIL::PrimitiveTopology::LineStrip:
    case DXIL::PrimitiveTopology::TriangleStrip:
      break;
    default: {
      ValCtx.EmitError(ValidationRule::SmGSValidOutputPrimitiveTopology);
    } break;
    }

    DXIL::InputPrimitive inputPrimitive = GS.inputPrimitive;
    unsigned VertexCount = GetNumVertices(inputPrimitive);
    if (VertexCount == 0 && inputPrimitive != DXIL::InputPrimitive::Undefined) {
      ValCtx.EmitError(ValidationRule::SmGSValidInputPrimitive);
    }
  }
}

static void ValidateShaderState(ValidationContext &ValCtx) {
  DxilModule &DM = ValCtx.DxilMod;
  if (ValCtx.isLibProfile) {
    for (Function &F : DM.GetModule()->functions()) {
      if (DM.HasDxilEntryProps(&F)) {
        DxilEntryProps &entryProps = DM.GetDxilEntryProps(&F);
        EntryStatus &Status = ValCtx.GetEntryStatus(&F);
        ValidateEntryProps(ValCtx, entryProps, Status);
        ValidatePassThruHS(ValCtx, entryProps, &F);
      }
    }
  } else {
    Function *Entry = DM.GetEntryFunction();
    if (!DM.HasDxilEntryProps(Entry)) {
      // must have props.
      ValCtx.EmitError(ValidationRule::MetaNoEntryPropsForEntry);
      return;
    }
    EntryStatus &Status = ValCtx.GetEntryStatus(Entry);
    DxilEntryProps &entryProps = DM.GetDxilEntryProps(Entry);
    ValidateEntryProps(ValCtx, entryProps, Status);
    ValidatePassThruHS(ValCtx, entryProps, Entry);
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

static void ValidateUninitializedOutput(ValidationContext &ValCtx,
                                        const DxilEntryProps &entryProps,
                                        EntryStatus &Status) {
  const DxilFunctionProps &props = entryProps.props;
  // For HS only need to check Tessfactor which is in patch constant sig.
  if (props.IsHS()) {
    std::vector<unsigned> &patchConstOrPrimCols = Status.patchConstOrPrimCols;
    const DxilSignature &patchConstSig = entryProps.sig.PatchConstOrPrimSignature;
    for (auto &E : patchConstSig.GetElements()) {
      unsigned mask = patchConstOrPrimCols[E->GetID()];
      unsigned requireMask = (1 << E->GetCols()) - 1;
      // TODO: check other case uninitialized output is allowed.
      if (mask != requireMask && !E->GetSemantic()->IsArbitrary()) {
        ValCtx.EmitFormatError(ValidationRule::SmUndefinedOutput,
                               {E->GetName()});
      }
    }
    return;
  }
  const DxilSignature &outSig = entryProps.sig.OutputSignature;
  std::vector<unsigned> &outputCols = Status.outputCols;
  for (auto &E : outSig.GetElements()) {
    unsigned mask = outputCols[E->GetID()];
    unsigned requireMask = (1 << E->GetCols()) - 1;
    // TODO: check other case uninitialized output is allowed.
    if (mask != requireMask && !E->GetSemantic()->IsArbitrary() &&
        E->GetSemantic()->GetKind() != Semantic::Kind::Target) {
      ValCtx.EmitFormatError(ValidationRule::SmUndefinedOutput, {E->GetName()});
    }
  }


  if (!props.IsGS()) {
    unsigned posMask = Status.OutputPositionMask[0];
    if (posMask != 0xf && Status.hasOutputPosition[0]) {
      ValCtx.EmitError(ValidationRule::SmCompletePosition);
    }
  } else {
    const auto &GS = props.ShaderProps.GS;
    unsigned streamMask = 0;
    for (size_t i = 0; i < _countof(GS.streamPrimitiveTopologies); ++i) {
      if (GS.streamPrimitiveTopologies[i] !=
          DXIL::PrimitiveTopology::Undefined) {
        streamMask |= 1<<i;
      }
    }

    for (unsigned i = 0; i < DXIL::kNumOutputStreams; i++) {
      if (streamMask & (1 << i)) {
        unsigned posMask = Status.OutputPositionMask[i];
        if (posMask != 0xf && Status.hasOutputPosition[i]) {
          ValCtx.EmitError(ValidationRule::SmCompletePosition);
        }
      }
    }
  }
}

static void ValidateUninitializedOutput(ValidationContext &ValCtx) {
  DxilModule &DM = ValCtx.DxilMod;
  if (ValCtx.isLibProfile) {
    for (Function &F : DM.GetModule()->functions()) {
      if (DM.HasDxilEntryProps(&F)) {
        DxilEntryProps &entryProps = DM.GetDxilEntryProps(&F);
        EntryStatus &Status = ValCtx.GetEntryStatus(&F);
        ValidateUninitializedOutput(ValCtx, entryProps, Status);
      }
    }
  } else {
    Function *Entry = DM.GetEntryFunction();
    if (!DM.HasDxilEntryProps(Entry)) {
      // must have props.
      ValCtx.EmitError(ValidationRule::MetaNoEntryPropsForEntry);
      return;
    }
    EntryStatus &Status = ValCtx.GetEntryStatus(Entry);
    DxilEntryProps &entryProps = DM.GetDxilEntryProps(Entry);
    ValidateUninitializedOutput(ValCtx, entryProps, Status);
  }
}

void GetValidationVersion(_Out_ unsigned *pMajor, _Out_ unsigned *pMinor) {
  /* <py::lines('VALRULE-TEXT')>hctdb_instrhelp.get_validation_version()</py>*/
  // VALRULE-TEXT:BEGIN
  // 1.0 is the first validator.
  // 1.1 adds:
  // - ILDN container part support
  // 1.2 adds:
  // - Metadata for floating point denorm mode
  // 1.3 adds:
  // - Library support
  // - Raytracing support
  // - i64/f64 overloads for rawBufferLoad/Store
  // 1.4 adds:
  // - packed u8x4/i8x4 dot with accumulate to i32
  // - half dot2 with accumulate to float
  // 1.5 adds:
  // - WaveMatch, WaveMultiPrefixOp, WaveMultiPrefixBitCount
  // - HASH container part support
  // - Mesh and Amplification shaders
  // - DXR 1.1 & RayQuery support
  *pMajor = 1;
  *pMinor = 6;
  // VALRULE-TEXT:END
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
  if (pDxilModule->HasMetadataErrors()) {
    DiagPrinter << "Metadata error encountered in non-critical metadata (such as Type Annotations).\n";
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  ValidationContext ValCtx(*pModule, pDebugModule, *pDxilModule, DiagPrinter);

  ValidateBitcode(ValCtx);

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

  ValidateShaderFlags(ValCtx);

  ValidateEntrySignatures(ValCtx);

  ValidateUninitializedOutput(ValCtx);
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
  case hlsl::DXIL::SignatureKind::PatchConstOrPrim:
    if (ValCtx.DxilMod.GetShaderModel()->GetKind() == DXIL::ShaderKind::Mesh)
      pName = "Program Primitive Signature";
    else
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


static void VerifyRDATMatches(_In_ ValidationContext &ValCtx,
                              _In_reads_bytes_(RDATSize) const void *pRDATData,
                              _In_ uint32_t RDATSize) {
  const char *PartName = "Runtime Data (RDAT)";
  // If DxilModule subobjects already loaded, validate these against the RDAT blob,
  // otherwise, load subobject into DxilModule to generate reference RDAT.
  if (!ValCtx.DxilMod.GetSubobjects()) {
    RDAT::DxilRuntimeData rdat(pRDATData, RDATSize);
    auto *pSubobjReader = rdat.GetSubobjectTableReader();
    if (pSubobjReader && pSubobjReader->GetCount() > 0) {
      ValCtx.DxilMod.ResetSubobjects(new DxilSubobjects());
      if (!LoadSubobjectsFromRDAT(*ValCtx.DxilMod.GetSubobjects(), pSubobjReader)) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches, { PartName });
        return;
      }
    }
  }

  unique_ptr<DxilPartWriter> pWriter(NewRDATWriter(ValCtx.DxilMod, 0));
  VerifyBlobPartMatches(ValCtx, PartName, pWriter.get(), pRDATData, RDATSize);

  // Verify no errors when runtime reflection from RDAT:
  RDAT::DxilRuntimeReflection *pReflection = RDAT::CreateDxilRuntimeReflection();
  if (!pReflection->InitFromRDAT(pRDATData, RDATSize)) {
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches, { PartName });
    return;
  }
}

_Use_decl_annotations_
bool VerifyRDATMatches(llvm::Module *pModule,
                       const void *pRDATData,
                       uint32_t RDATSize) {
  std::string diagStr;
  raw_string_ostream diagStream(diagStr);
  DiagnosticPrinterRawOStream DiagPrinter(diagStream);
  ValidationContext ValCtx(*pModule, nullptr, pModule->GetOrCreateDxilModule(), DiagPrinter);
  VerifyRDATMatches(ValCtx, pRDATData, RDATSize);
  if (ValCtx.Failed) {
    emitDxilDiag(pModule->getContext(), diagStream.str().c_str());
  }
  return !ValCtx.Failed;
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
  bool bTessOrMesh = ShaderKind == DXIL::ShaderKind::Hull ||
                     ShaderKind == DXIL::ShaderKind::Domain ||
                     ShaderKind == DXIL::ShaderKind::Mesh;

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
      if (ValCtx.isLibProfile) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid, { szFourCC });
      } else {
        VerifySignatureMatches(ValCtx, DXIL::SignatureKind::Input, GetDxilPartData(pPart), pPart->PartSize);
      }
      break;
    case DFCC_OutputSignature:
      if (ValCtx.isLibProfile) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid, { szFourCC });
      } else {
        VerifySignatureMatches(ValCtx, DXIL::SignatureKind::Output, GetDxilPartData(pPart), pPart->PartSize);
      }
      break;
    case DFCC_PatchConstantSignature:
      if (ValCtx.isLibProfile) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid, { szFourCC });
      } else {
        if (bTessOrMesh) {
          VerifySignatureMatches(ValCtx, DXIL::SignatureKind::PatchConstOrPrim, GetDxilPartData(pPart), pPart->PartSize);
        } else {
          ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches, {"Program Patch Constant Signature"});
        }
      }
      break;
    case DFCC_FeatureInfo:
      VerifyFeatureInfoMatches(ValCtx, GetDxilPartData(pPart), pPart->PartSize);
      break;
    case DFCC_RootSignature:
      pRootSignaturePart = pPart;
      if (ValCtx.isLibProfile) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid, { szFourCC });
      }
      break;
    case DFCC_PipelineStateValidation:
      pPSVPart = pPart;
      if (ValCtx.isLibProfile) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid, { szFourCC });
      } else {
        VerifyPSVMatches(ValCtx, GetDxilPartData(pPart), pPart->PartSize);
      }
      break;

    // Skip these
    case DFCC_ResourceDef:
    case DFCC_ShaderStatistics:
    case DFCC_PrivateData:
    case DFCC_DXIL:
    case DFCC_ShaderDebugInfoDXIL:
    case DFCC_ShaderDebugName:
      continue;

    case DFCC_ShaderHash:
      if (pPart->PartSize != sizeof(DxilShaderHash)) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid, { szFourCC });
      }
      break;

    // Runtime Data (RDAT) for libraries
    case DFCC_RuntimeData:
      if (ValCtx.isLibProfile) {
        VerifyRDATMatches(ValCtx, GetDxilPartData(pPart), pPart->PartSize);
      } else {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid, { szFourCC });
      }
      break;

    case DFCC_Container:
    default:
      ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid, {szFourCC});
      break;
    }
  }

  // Verify required parts found
  if (ValCtx.isLibProfile) {
    if (FourCCFound.find(DFCC_RuntimeData) == FourCCFound.end()) {
      ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing, { "Runtime Data (RDAT)" });
    }
  } else {
    if (FourCCFound.find(DFCC_InputSignature) == FourCCFound.end()) {
      VerifySignatureMatches(ValCtx, DXIL::SignatureKind::Input, nullptr, 0);
    }
    if (FourCCFound.find(DFCC_OutputSignature) == FourCCFound.end()) {
      VerifySignatureMatches(ValCtx, DXIL::SignatureKind::Output, nullptr, 0);
    }
    if (bTessOrMesh && FourCCFound.find(DFCC_PatchConstantSignature) == FourCCFound.end() &&
        pDxilModule->GetPatchConstOrPrimSignature().GetElements().size())
    {
      ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing, { "Program Patch Constant Signature" });
    }
    if (FourCCFound.find(DFCC_FeatureInfo) == FourCCFound.end()) {
      // Could be optional, but RS1 runtime doesn't handle this case properly.
      ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing, { "Feature Info" });
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
                                                   DiagStream),
                  DXC_E_INCORRECT_ROOT_SIGNATURE);
        } catch (...) {
          ValCtx.EmitError(ValidationRule::ContainerRootSignatureIncompatible);
        }
      }
    } else {
      ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing, {"Pipeline State Validation"});
    }
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
  auto &SerializedRootSig = dxilModule.GetSerializedRootSignature();
  if (!SerializedRootSig.empty()) {
    unique_ptr<DxilPartWriter> pWriter(NewPSVWriter(dxilModule, 0));
    DXASSERT_NOMSG(pWriter->size());
    CComPtr<AbstractMemoryStream> pOutputStream;
    IFT(CreateMemoryStream(DxcGetThreadMallocNoRef(), &pOutputStream));
    pOutputStream->Reserve(pWriter->size());
    pWriter->write(pOutputStream);
    try {
      const DxilVersionedRootSignatureDesc* pDesc = nullptr;
      DeserializeRootSignature(SerializedRootSig.data(), SerializedRootSig.size(), &pDesc);
      if (!pDesc) {
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
