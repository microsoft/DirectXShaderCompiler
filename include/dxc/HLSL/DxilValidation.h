///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilValidation.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file provides support for validating DXIL shaders.                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <system_error>

namespace llvm {
class Module;
}

namespace hlsl {

/* <py::lines('VALRULE-ENUM')>hctdb_instrhelp.get_valrule_enum()</py>*/
// VALRULE-ENUM:BEGIN
// Known validation rules
enum class ValidationRule : unsigned {
  // Bitcode
  BitcodeValid, // TODO - Module must be bitcode-valid

  // Declaration
  DeclDxilFnExtern, // External function must be a DXIL function
  DeclDxilNsReserved, // The DXIL reserved prefixes must only be used by built-in functions and types
  DeclFnFlattenParam, // Function parameters must not use struct types
  DeclFnIsCalled, // Functions can only be used by call instructions
  DeclNotUsedExternal, // External declaration should not be used
  DeclUsedExternalFunction, // External function must be used
  DeclUsedInternal, // Internal declaration must be used

  // Instruction
  InstrAllowed, // Instructions must be of an allowed type
  InstrBarrierModeForNonCS, // sync in a non-Compute Shader must only sync UAV (sync_uglobal)
  InstrBarrierModeNoMemory, // sync must include some form of memory barrier - _u (UAV) and/or _g (Thread Group Shared Memory).  Only _t (thread group sync) is optional. 
  InstrBarrierModeUselessUGroup, // sync can't specify both _ugroup and _uglobal. If both are needed, just specify _uglobal.
  InstrBufferUpdateCounterOnUAV, // BufferUpdateCounter valid only on UAV
  InstrCBufferClassForCBufferHandle, // Expect Cbuffer for CBufferLoad handle
  InstrCBufferOutOfBound, // Cbuffer access out of bound
  InstrCallOload, // Call to DXIL intrinsic must match overload signature
  InstrCannotPullPosition, // pull-model evaluation of position disallowed
  InstrCoordinateCountForRawTypedBuf, // raw/typed buffer don't need 2 coordinates
  InstrCoordinateCountForStructBuf, // structured buffer require 2 coordinates
  InstrDeterminateDerivative, // gradient operation uses a value that may not be defined for all pixels (in UAV loads can not participate in gradient operations)
  InstrDxilStructUser, // Dxil struct types should only used by ExtractValue
  InstrDxilStructUserOutOfBound, // Index out of bound when extract value from dxil struct types
  InstrERR_ALIAS_ARRAY_INDEX_OUT_OF_BOUNDS, // TODO - ERR_ALIAS_ARRAY_INDEX_OUT_OF_BOUNDS
  InstrERR_ATTRIBUTE_PARAM_SIDE_EFFECT, // TODO - expressions with side effects are illegal as attribute parameters for root signature
  InstrERR_GUARANTEED_RACE_CONDITION_GSM, // TODO - race condition writing to shared memory detected, consider making this write conditional.
  InstrERR_GUARANTEED_RACE_CONDITION_UAV, // TODO - race condition writing to shared resource detected, consider making this write conditional.
  InstrERR_LOOP_CONDITION_OUT_OF_BOUNDS, // TODO - cannot unroll loop with an out-of-bounds array reference in the condition
  InstrERR_NON_LITERAL_RESOURCE, // TODO - Resources being indexed cannot come from conditional expressions, they must come from literal expressions.
  InstrERR_NON_LITERAL_STREAM, // TODO - stream parameter must come from a literal expression
  InstrERR_RESOURCE_UNINITIALIZED, // TODO - Resource being indexed is uninitialized.
  InstrEvalInterpolationMode, // Interpolation mode on %0 used with eval_* instruction must be linear, linear_centroid, linear_noperspective, linear_noperspective_centroid, linear_sample or linear_noperspective_sample
  InstrFailToResloveTGSMPointer, // TGSM pointers must originate from an unambiguous TGSM global variable.
  InstrHandleNotFromCreateHandle, // Resource handle should returned by createHandle
  InstrImmBiasForSampleB, // bias amount for sample_b must be in the range [%0,%1], but %2 was specified as an immediate
  InstrInBoundsAccess, // TODO - Access to out-of-bounds memory is disallowed
  InstrMinPrecisionNotPrecise, // Instructions marked precise may not refer to minprecision values
  InstrMipLevelForGetDimension, // Use mip level on buffer when GetDimensions
  InstrMipOnUAVLoad, // uav load don't support mipLevel/sampleIndex
  InstrNoIDivByZero, // TODO - No signed integer division by zero
  InstrNoIndefiniteAcos, // TODO - No indefinite arccosine
  InstrNoIndefiniteAsin, // TODO - No indefinite arcsine
  InstrNoIndefiniteDsxy, // TODO - No indefinite derivative calculation
  InstrNoIndefiniteLog, // TODO - No indefinite logarithm
  InstrNoPtrCast, // TODO - Cast between pointer types disallowed
  InstrNoReadingUninitialized, // Instructions should not read uninitialized value
  InstrNoUDivByZero, // TODO - No unsigned integer division by zero
  InstrOffsetOnUAVLoad, // uav load don't support offset
  InstrOload, // DXIL intrinsic overload must be valid
  InstrOnlyOneAllocConsume, // RWStructuredBuffers may increment or decrement their counters, but not both.
  InstrOpCodeResType, // TODO - DXIL intrinsic operating on a resource must be of the correct type
  InstrOpCodeReserved, // Instructions must not reference reserved opcodes
  InstrOpConst, // DXIL intrinsic requires an immediate constant operand
  InstrOpConstRange, // TODO - Constant values must be in-range for operation
  InstrOperandRange, // TODO - DXIL intrinsic operand must be within defined range
  InstrPtrArea, // TODO - Pointer must refer to a defined area
  InstrResID, // TODO - DXIL instruction must refer to valid resource IDs
  InstrResourceClassForLoad, // load can only run on UAV/SRV resource
  InstrResourceClassForSamplerGather, // sample, lod and gather should on srv resource.
  InstrResourceClassForUAVStore, // store should on uav resource.
  InstrResourceCoordinateMiss, // coord uninitialized
  InstrResourceCoordinateTooMany, // out of bound coord must be undef
  InstrResourceKindForBufferLoadStore, // buffer load/store only works on Raw/Typed/StructuredBuffer
  InstrResourceKindForCalcLOD, // lod requires resource declared as texture1D/2D/3D/Cube/CubeArray/1DArray/2DArray
  InstrResourceKindForGather, // gather requires resource declared as texture/2D/Cube/2DArray/CubeArray
  InstrResourceKindForGetDim, // Invalid resource kind on GetDimensions
  InstrResourceKindForSample, // sample/_l/_d requires resource declared as texture1D/2D/3D/Cube/1DArray/2DArray/CubeArray
  InstrResourceKindForSampleC, // samplec requires resource declared as texture1D/2D/Cube/1DArray/2DArray/CubeArray
  InstrResourceKindForTextureLoad, // texture load only works on Texture1D/1DArray/2D/2DArray/3D/MS2D/MS2DArray
  InstrResourceKindForTextureStore, // texture store only works on Texture1D/1DArray/2D/2DArray/3D
  InstrResourceOffsetMiss, // offset uninitialized
  InstrResourceOffsetTooMany, // out of bound offset must be undef
  InstrSampleCompType, // sample_* instructions require resource to be declared to return UNORM, SNORM or FLOAT.
  InstrSampleIndexForLoad2DMS, // load on Texture2DMS/2DMSArray require sampleIndex
  InstrSamplerModeForLOD, // lod instruction requires sampler declared in default mode
  InstrSamplerModeForSample, // sample/_l/_d/_cl_s/gather instruction requires sampler declared in default mode
  InstrSamplerModeForSampleC, // sample_c_*/gather_c instructions require sampler declared in comparison mode
  InstrTextureLod, // TODO - Level-of-detail is only defined for Texture1D, Texture2D, Texture3D and TextureCube
  InstrTextureOffset, // offset texture instructions must take offset which can resolve to integer literal in the range -8 to 7
  InstrTextureOpArgs, // TODO - Instructions that depend on texture type must match operands
  InstrTypeCast, // TODO - Type cast must be valid
  InstrUndefResultForGetDimension, // GetDimensions used undef dimension %0 on %1
  InstrWAR_GRADIENT_IN_VARYING_FLOW, // TODO - gradient instruction used in a loop with varying iteration; partial derivatives may have undefined value
  InstrWriteMaskForTypedUAVStore, // store on typed uav must write to all four components of the UAV
  InstrWriteMaskMatchValueForUAVStore, // uav store write mask must match store value mask, write mask is %0 and store value mask is %1

  // Metadata
  MetaBranchFlatten, // Can't use branch and flatten attributes together
  MetaDenseResIDs, // Resource identifiers must be zero-based and dense
  MetaEntryFunction, // entrypoint not found
  MetaFlagsUsage, // Flags must match usage
  MetaForceCaseOnSwitch, // Attribute forcecase only works for switch
  MetaFunctionAnnotation, // Cannot find function annotation for %0
  MetaGlcNotOnAppendConsume, // globallycoherent cannot be used with append/consume buffers
  MetaIntegerInterpMode, // signature %0 specifies invalid interpolation mode for integer component type.
  MetaInterpModeInOneRow, // Interpolation mode cannot vary for different cols of a row. Vary at %0 row %1
  MetaInterpModeValid, // Interpolation mode must be valid
  MetaInvalidControlFlowHint, // Invalid control flow hint
  MetaKnown, // Named metadata should be known
  MetaMaxTessFactor, // Hull Shader MaxTessFactor must be [%0..%1].  %2 specified
  MetaNoSemanticOverlap, // Semantics must not overlap
  MetaRequired, // TODO - Required metadata missing
  MetaSemaKindValid, // Semantic kind must be valid
  MetaSemanticCompType, // %0 must be %1
  MetaSemanticLen, // Semantic length must be at least 1 and at most 64
  MetaSignatureCompType, // signature %0 specifies unrecognized or invalid component type
  MetaSignatureOutOfRange, // signature %0 is out of range at row %1 col %2 size %3.
  MetaSignatureOverlap, // signature %0 use overlaped address at row %1 col %2 size %3.
  MetaStructBufAlignment, // StructuredBuffer stride not aligned
  MetaStructBufAlignmentOutOfBound, // StructuredBuffer stride out of bounds
  MetaTarget, // Target triple must be 'dxil-ms-dx'
  MetaTessellatorOutputPrimitive, // Invalid Tessellator Output Primitive specified. Must be point, line, triangleCW or triangleCCW.
  MetaTessellatorPartition, // Invalid Tessellator Partitioning specified. Must be integer, pow2, fractional_odd or fractional_even.
  MetaTextureType, // elements of typed buffers and textures must fit in four 32-bit quantities
  MetaUsed, // TODO - All metadata must be used
  MetaValidSamplerMode, // Invalid sampler mode on sampler 
  MetaValueRange, // Metadata value must be within range
  MetaWellFormed, // TODO - Metadata must be well-formed in operand count and types

  // Program flow
  FlowBranchLimit, // TODO - Flow control blocks can nest up to 64 deep per subroutine (and main)
  FlowCallLimit, // Subroutines can nest up to 32 levels deep
  FlowDeadLoop, // Loop must have break
  FlowNoRecusion, // Recursion is not permitted
  FlowReducible, // Execution flow must be reducible

  // Shader model
  SmAppendAndConsumeOnSameUAV, // BufferUpdateCounter inc and dec on a given UAV (%d) cannot both be in the same shader for shader model less than 5.1.
  SmCBufferElementOverflow, // CBuffer elements must not overflow
  SmCBufferOffsetOverlap, // CBuffer offsets must not overlap
  SmCBufferTemplateTypeMustBeStruct, // D3D12 constant/texture buffer template element can only be a struct
  SmCSNoReturn, // Compute shaders can't return values, outputs must be written in writable resources (UAVs).
  SmCompletePosition, // Not all elements of SV_Position were written
  SmCounterOnlyOnStructBuf, // BufferUpdateCounter valid only on structured buffers
  SmDSInputControlPointCountRange, // DS input control point count must be [0..%0].  %1 specified
  SmDomainLocationIdxOOB, // DomainLocation component index out of bounds for the domain.
  SmERR_BIND_RESOURCE_RANGE_OVERFLOW, // TODO - ERR_BIND_RESOURCE_RANGE_OVERFLOW
  SmERR_MAX_CBUFFER_EXCEEDED, // TODO - The maximum number of constant buffer slots is exceeded for a library (slot index=%u, max slots=%u)
  SmERR_MAX_CONST_EXCEEDED, // TODO - ERR_MAX_CONST_EXCEEDED
  SmERR_MAX_SAMPLER_EXCEEDED, // TODO - The maximum number of sampler slots is exceeded for a library (slot index=%u, max slots=%u)
  SmERR_MAX_TEXTURE_EXCEEDED, // TODO - The maximum number of texture slots is exceeded for a library (slot index=%u, max slots=%u)
  SmERR_UNABLE_TO_BIND_RESOURCE, // TODO - ERR_UNABLE_TO_BIND_RESOURCE
  SmERR_UNABLE_TO_BIND_UNBOUNDED_RESOURCE, // TODO - ERR_UNABLE_TO_BIND_UNBOUNDED_RESOURCE
  SmGSInstanceCountRange, // GS instance count must be [1..%0].  %1 specified
  SmGSOutputVertexCountRange, // GS output vertex count must be [0..%0].  %1 specified
  SmGSTotalOutputVertexDataRange, // Declared output vertex count (%0) multiplied by the total number of declared scalar components of output data (%1) equals %2.  This value cannot be greater than %3
  SmGSValidInputPrimitive, // GS input primitive unrecognized
  SmGSValidOutputPrimitiveTopology, // GS output primitive topology unrecognized
  SmHSInputControlPointCountRange, // HS input control point count must be [1..%0].  %1 specified
  SmHullPassThruControlPointCountMatch, // For pass thru hull shader, input control point count must match output control point count
  SmInsideTessFactorSizeMatchDomain, // InsideTessFactor size mismatch the domain.
  SmInvalidResourceCompType, // Invalid resource return type
  SmInvalidResourceKind, // Invalid resources kind
  SmInvalidTextureKindOnUAV, // Texture2DMS[Array] or TextureCube[Array] resources are not supported with UAVs
  SmIsoLineOutputPrimitiveMismatch, // Hull Shader declared with IsoLine Domain must specify output primitive point or line. Triangle_cw or triangle_ccw output are not compatible with the IsoLine Domain.
  SmMaxTGSMSize, // Total Thread Group Shared Memory storage is %0, exceeded %1
  SmMaxTheadGroup, // Declared Thread Group Count %0 (X*Y*Z) is beyond the valid maximum of %1
  SmMultiStreamMustBePoint, // When multiple GS output streams are used they must be pointlists
  SmName, // Target shader model name must be known
  SmNoInterpMode, // Interpolation mode must be undefined for VS input/PS output/patch constant.
  SmNoPSOutputIdx, // Pixel shader output registers are not indexable.
  SmOpcode, // Opcode must be defined in target shader model
  SmOpcodeInInvalidFunction, // Invalid DXIL opcode usage like StorePatchConstant in patch constant function
  SmOperand, // Operand must be defined in target shader model
  SmOutputControlPointCountRange, // output control point count must be [0..%0].  %1 specified
  SmOutputControlPointsTotalScalars, // Total number of scalars across all HS output control points must not exceed 
  SmPSConsistentInterp, // Interpolation mode for PS input position must be linear_noperspective_centroid or linear_noperspective_sample when outputting oDepthGE or oDepthLE and not running at sample frequency (which is forced by inputting SV_SampleIndex or declaring an input linear_sample or linear_noperspective_sample)
  SmPSCoverageAndInnerCoverage, // InnerCoverage and Coverage are mutually exclusive.
  SmPSOutputSemantic, // Pixel Shader allows output semantics to be SV_Target, SV_Depth, SV_DepthGreaterEqual, SV_DepthLessEqual, SV_Coverage or SV_StencilRef, %0 found
  SmPatchConstantOnlyForHSDS, // patch constant signature only valid in HS and DS
  SmROVOnlyInPS, // RasterizerOrdered objects are only allowed in 5.0+ pixel shaders
  SmResLimit, // TODO - Resource limit exceeded for target shader model
  SmResourceRangeOverlap, // Resource ranges must not overlap
  SmSampleCountOnlyOn2DMS, // Only Texture2DMS/2DMSArray could has sample count
  SmSemantic, // Semantic must be defined in target shader model
  SmStreamIndexRange, // Stream index (%0) must between 0 and %1
  SmTessFactorForDomain, // Required TessFactor for domain not found declared anywhere in Patch Constant data
  SmTessFactorSizeMatchDomain, // TessFactor size mismatch the domain.
  SmThreadGroupChannelRange, // Declared Thread Group %0 size %1 outside valid range [%2..%3]
  SmTriOutputPrimitiveMismatch, // Hull Shader declared with Tri Domain must specify output primitive point, triangle_cw or triangle_ccw. Line output is not compatible with the Tri domain
  SmUndefinedOutput, // Not all elements of output %0 were written
  SmValidDomain, // Invalid Tessellator Domain specified. Must be isoline, tri or quad

  // Type system
  TypesDefined, // Type must be defined based on DXIL primitives
  TypesIntWidth, // Int type must be of valid width
  TypesNoVector, // Vector types must not be present

  // Uniform analysis
  UniNoWaveSensitiveGradient, // Gradient operations are not affected by wave-sensitive data or control flow.
};
// VALRULE-ENUM:END

const char *GetValidationRuleText(ValidationRule value);
void GetValidationVersion(_Out_ unsigned *pMajor, _Out_ unsigned *pMinor);
std::error_code ValidateDxilModule(_In_ llvm::Module *pModule,
                                   _In_opt_ llvm::Module *pDebugModule);

}
