///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilValidation.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// This file provides support for validating DXIL shaders.                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include "dxc/Support/Global.h"
#include "dxc/DXIL/DxilConstants.h"
#include "dxc/Support/WinAdapter.h"

namespace llvm {
class Module;
class LLVMContext;
class raw_ostream;
class DiagnosticPrinter;
class DiagnosticInfo;
}

namespace hlsl {

/* <py::lines('VALRULE-ENUM')>hctdb_instrhelp.get_valrule_enum()</py>*/
// VALRULE-ENUM:BEGIN
// Known validation rules
enum class ValidationRule : unsigned {
  // Bitcode
  BitcodeValid, // TODO - Module must be bitcode-valid

  // Container
  ContainerPartInvalid, // DXIL Container must not contain unknown parts
  ContainerPartMatches, // DXIL Container Parts must match Module
  ContainerPartMissing, // DXIL Container requires certain parts, corresponding to module
  ContainerPartRepeated, // DXIL Container must have only one of each part type
  ContainerRootSignatureIncompatible, // Root Signature in DXIL Container must be compatible with shader

  // Declaration
  DeclAttrStruct, // Attributes parameter must be struct type
  DeclDxilFnExtern, // External function must be a DXIL function
  DeclDxilNsReserved, // The DXIL reserved prefixes must only be used by built-in functions and types
  DeclExtraArgs, // Extra arguments not allowed for shader functions
  DeclFnAttribute, // Functions should only contain known function attributes
  DeclFnFlattenParam, // Function parameters must not use struct types
  DeclFnIsCalled, // Functions can only be used by call instructions
  DeclNotUsedExternal, // External declaration should not be used
  DeclParamStruct, // Callable function parameter must be struct type
  DeclPayloadStruct, // Payload parameter must be struct type
  DeclResourceInFnSig, // Resources not allowed in function signatures
  DeclShaderMissingArg, // payload/params/attributes parameter is required for certain shader types
  DeclShaderReturnVoid, // Shader functions must return void
  DeclUsedExternalFunction, // External function must be used
  DeclUsedInternal, // Internal declaration must be used

  // Instruction
  InstrAllowed, // Instructions must be of an allowed type.
  InstrAttributeAtVertexNoInterpolation, // Attribute %0 must have nointerpolation mode in order to use GetAttributeAtVertex function.
  InstrBarrierModeForNonCS, // sync in a non-Compute/Amplification/Mesh Shader must only sync UAV (sync_uglobal).
  InstrBarrierModeNoMemory, // sync must include some form of memory barrier - _u (UAV) and/or _g (Thread Group Shared Memory).  Only _t (thread group sync) is optional.
  InstrBarrierModeUselessUGroup, // sync can't specify both _ugroup and _uglobal. If both are needed, just specify _uglobal.
  InstrBufferUpdateCounterOnResHasCounter, // BufferUpdateCounter valid only when HasCounter is true.
  InstrBufferUpdateCounterOnUAV, // BufferUpdateCounter valid only on UAV.
  InstrCBufferClassForCBufferHandle, // Expect Cbuffer for CBufferLoad handle.
  InstrCBufferOutOfBound, // Cbuffer access out of bound.
  InstrCallOload, // Call to DXIL intrinsic must match overload signature
  InstrCannotPullPosition, // pull-model evaluation of position disallowed
  InstrCheckAccessFullyMapped, // CheckAccessFullyMapped should only be used on resource status.
  InstrCoordinateCountForRawTypedBuf, // raw/typed buffer don't need 2 coordinates.
  InstrCoordinateCountForStructBuf, // structured buffer require 2 coordinates.
  InstrCreateHandleImmRangeID, // Local resource must map to global resource.
  InstrDxilStructUser, // Dxil struct types should only be used by ExtractValue.
  InstrDxilStructUserOutOfBound, // Index out of bound when extract value from dxil struct types.
  InstrEvalInterpolationMode, // Interpolation mode on %0 used with eval_* instruction must be linear, linear_centroid, linear_noperspective, linear_noperspective_centroid, linear_sample or linear_noperspective_sample.
  InstrExtractValue, // ExtractValue should only be used on dxil struct types and cmpxchg.
  InstrFailToResloveTGSMPointer, // TGSM pointers must originate from an unambiguous TGSM global variable.
  InstrHandleNotFromCreateHandle, // Resource handle should returned by createHandle.
  InstrImmBiasForSampleB, // bias amount for sample_b must be in the range [%0,%1], but %2 was specified as an immediate.
  InstrInBoundsAccess, // Access to out-of-bounds memory is disallowed.
  InstrMinPrecisionNotPrecise, // Instructions marked precise may not refer to minprecision values.
  InstrMinPrecisonBitCast, // Bitcast on minprecison types is not allowed.
  InstrMipLevelForGetDimension, // Use mip level on buffer when GetDimensions.
  InstrMipOnUAVLoad, // uav load don't support mipLevel/sampleIndex.
  InstrMissingSetMeshOutputCounts, // Missing SetMeshOutputCounts call.
  InstrMultipleGetMeshPayload, // GetMeshPayload cannot be called multiple times.
  InstrMultipleSetMeshOutputCounts, // SetMeshOUtputCounts cannot be called multiple times.
  InstrNoGenericPtrAddrSpaceCast, // Address space cast between pointer types must have one part to be generic address space.
  InstrNoIDivByZero, // No signed integer division by zero.
  InstrNoIndefiniteAcos, // No indefinite arccosine.
  InstrNoIndefiniteAsin, // No indefinite arcsine.
  InstrNoIndefiniteDsxy, // No indefinite derivative calculation.
  InstrNoIndefiniteLog, // No indefinite logarithm.
  InstrNoReadingUninitialized, // Instructions should not read uninitialized value.
  InstrNoUDivByZero, // No unsigned integer division by zero.
  InstrNonDominatingDispatchMesh, // Non-Dominating DispatchMesh call.
  InstrNonDominatingSetMeshOutputCounts, // Non-Dominating SetMeshOutputCounts call.
  InstrNotOnceDispatchMesh, // DispatchMesh must be called exactly once in an Amplification shader.
  InstrOffsetOnUAVLoad, // uav load don't support offset.
  InstrOload, // DXIL intrinsic overload must be valid.
  InstrOnlyOneAllocConsume, // RWStructuredBuffers may increment or decrement their counters, but not both.
  InstrOpCodeReserved, // Instructions must not reference reserved opcodes.
  InstrOpConst, // DXIL intrinsic requires an immediate constant operand
  InstrOpConstRange, // Constant values must be in-range for operation.
  InstrOperandRange, // DXIL intrinsic operand must be within defined range
  InstrPtrBitCast, // Pointer type bitcast must be have same size.
  InstrResourceClassForLoad, // load can only run on UAV/SRV resource.
  InstrResourceClassForSamplerGather, // sample, lod and gather should be on srv resource.
  InstrResourceClassForUAVStore, // store should be on uav resource.
  InstrResourceCoordinateMiss, // coord uninitialized.
  InstrResourceCoordinateTooMany, // out of bound coord must be undef.
  InstrResourceKindForBufferLoadStore, // buffer load/store only works on Raw/Typed/StructuredBuffer.
  InstrResourceKindForCalcLOD, // lod requires resource declared as texture1D/2D/3D/Cube/CubeArray/1DArray/2DArray.
  InstrResourceKindForGather, // gather requires resource declared as texture/2D/Cube/2DArray/CubeArray.
  InstrResourceKindForGetDim, // Invalid resource kind on GetDimensions.
  InstrResourceKindForSample, // sample/_l/_d requires resource declared as texture1D/2D/3D/Cube/1DArray/2DArray/CubeArray.
  InstrResourceKindForSampleC, // samplec requires resource declared as texture1D/2D/Cube/1DArray/2DArray/CubeArray.
  InstrResourceKindForTextureLoad, // texture load only works on Texture1D/1DArray/2D/2DArray/3D/MS2D/MS2DArray.
  InstrResourceKindForTextureStore, // texture store only works on Texture1D/1DArray/2D/2DArray/3D.
  InstrResourceKindForTraceRay, // TraceRay should only use RTAccelerationStructure.
  InstrResourceMapToSingleEntry, // Fail to map resource to resource table.
  InstrResourceOffsetMiss, // offset uninitialized.
  InstrResourceOffsetTooMany, // out of bound offset must be undef.
  InstrResourceUser, // Resource should only be used by Load/GEP/Call.
  InstrSampleCompType, // sample_* instructions require resource to be declared to return UNORM, SNORM or FLOAT.
  InstrSampleIndexForLoad2DMS, // load on Texture2DMS/2DMSArray require sampleIndex.
  InstrSamplerModeForLOD, // lod instruction requires sampler declared in default mode.
  InstrSamplerModeForSample, // sample/_l/_d/_cl_s/gather instruction requires sampler declared in default mode.
  InstrSamplerModeForSampleC, // sample_c_*/gather_c instructions require sampler declared in comparison mode.
  InstrSignatureOperationNotInEntry, // Dxil operation for input output signature must be in entryPoints.
  InstrStatus, // Resource status should only be used by CheckAccessFullyMapped.
  InstrStructBitCast, // Bitcast on struct types is not allowed.
  InstrTGSMRaceCond, // Race condition writing to shared memory detected, consider making this write conditional.
  InstrTextureOffset, // offset texture instructions must take offset which can resolve to integer literal in the range -8 to 7.
  InstrUndefResultForGetDimension, // GetDimensions used undef dimension %0 on %1.
  InstrUndefinedValueForUAVStore, // Assignment of undefined values to UAV.
  InstrWriteMaskForTypedUAVStore, // store on typed uav must write to all four components of the UAV.
  InstrWriteMaskMatchValueForUAVStore, // uav store write mask must match store value mask, write mask is %0 and store value mask is %1.

  // Metadata
  MetaBarycentricsFloat3, // only 'float3' type is allowed for SV_Barycentrics.
  MetaBarycentricsInterpolation, // SV_Barycentrics cannot be used with 'nointerpolation' type.
  MetaBarycentricsTwoPerspectives, // There can only be up to two input attributes of SV_Barycentrics with different perspective interpolation mode.
  MetaBranchFlatten, // Can't use branch and flatten attributes together.
  MetaClipCullMaxComponents, // Combined elements of SV_ClipDistance and SV_CullDistance must fit in 8 components
  MetaClipCullMaxRows, // Combined elements of SV_ClipDistance and SV_CullDistance must fit in two rows.
  MetaControlFlowHintNotOnControlFlow, // Control flow hint only works on control flow inst.
  MetaDenseResIDs, // Resource identifiers must be zero-based and dense.
  MetaDuplicateSysValue, // System value may only appear once in signature
  MetaEntryFunction, // entrypoint not found.
  MetaFlagsUsage, // Flags must match usage.
  MetaForceCaseOnSwitch, // Attribute forcecase only works for switch.
  MetaGlcNotOnAppendConsume, // globallycoherent cannot be used with append/consume buffers: '%0'.
  MetaIntegerInterpMode, // Interpolation mode on integer must be Constant
  MetaInterpModeInOneRow, // Interpolation mode must be identical for all elements packed into the same row.
  MetaInterpModeValid, // Interpolation mode must be valid
  MetaInvalidControlFlowHint, // Invalid control flow hint.
  MetaKnown, // Named metadata should be known
  MetaMaxTessFactor, // Hull Shader MaxTessFactor must be [%0..%1].  %2 specified.
  MetaNoEntryPropsForEntry, // Entry point %0 must have entry properties.
  MetaNoSemanticOverlap, // Semantics must not overlap
  MetaRequired, // TODO - Required metadata missing.
  MetaSemaKindMatchesName, // Semantic name must match system value, when defined.
  MetaSemaKindValid, // Semantic kind must be valid
  MetaSemanticCompType, // %0 must be %1.
  MetaSemanticIndexMax, // System value semantics have a maximum valid semantic index
  MetaSemanticLen, // Semantic length must be at least 1 and at most 64.
  MetaSemanticShouldBeAllocated, // Semantic should have a valid packing location
  MetaSemanticShouldNotBeAllocated, // Semantic should have a packing location of -1
  MetaSignatureCompType, // signature %0 specifies unrecognized or invalid component type.
  MetaSignatureDataWidth, // Data width must be identical for all elements packed into the same row.
  MetaSignatureIllegalComponentOrder, // Component ordering for packed elements must be: arbitrary < system value < system generated value
  MetaSignatureIndexConflict, // Only elements with compatible indexing rules may be packed together
  MetaSignatureOutOfRange, // Signature elements must fit within maximum signature size
  MetaSignatureOverlap, // Signature elements may not overlap in packing location.
  MetaStructBufAlignment, // StructuredBuffer stride not aligned
  MetaStructBufAlignmentOutOfBound, // StructuredBuffer stride out of bounds
  MetaSystemValueRows, // System value may only have 1 row
  MetaTarget, // Target triple must be 'dxil-ms-dx'
  MetaTessellatorOutputPrimitive, // Invalid Tessellator Output Primitive specified. Must be point, line, triangleCW or triangleCCW.
  MetaTessellatorPartition, // Invalid Tessellator Partitioning specified. Must be integer, pow2, fractional_odd or fractional_even.
  MetaTextureType, // elements of typed buffers and textures must fit in four 32-bit quantities.
  MetaUsed, // All metadata must be used by dxil.
  MetaValidSamplerMode, // Invalid sampler mode on sampler .
  MetaValueRange, // Metadata value must be within range.
  MetaWellFormed, // TODO - Metadata must be well-formed in operand count and types.

  // Program flow
  FlowDeadLoop, // Loop must have break.
  FlowFunctionCall, // Function with parameter is not permitted
  FlowNoRecusion, // Recursion is not permitted.
  FlowReducible, // Execution flow must be reducible.

  // Shader model
  Sm64bitRawBufferLoadStore, // i64/f64 rawBufferLoad/Store overloads are allowed after SM 6.3.
  SmAmplificationShaderPayloadSize, // For amplification shader with entry '%0', payload size %1 is greater than maximum size of %2 bytes.
  SmAmplificationShaderPayloadSizeDeclared, // For amplification shader with entry '%0', payload size %1 is greater than declared size of %2 bytes.
  SmAppendAndConsumeOnSameUAV, // BufferUpdateCounter inc and dec on a given UAV (%d) cannot both be in the same shader for shader model less than 5.1.
  SmCBufferArrayOffsetAlignment, // CBuffer array offset must be aligned to 16-bytes
  SmCBufferElementOverflow, // CBuffer elements must not overflow
  SmCBufferOffsetOverlap, // CBuffer offsets must not overlap
  SmCBufferTemplateTypeMustBeStruct, // D3D12 constant/texture buffer template element can only be a struct.
  SmCSNoSignatures, // Compute shaders must not have shader signatures.
  SmCompletePosition, // Not all elements of SV_Position were written.
  SmConstantInterpMode, // Interpolation mode must be constant for MS primitive output.
  SmCounterOnlyOnStructBuf, // BufferUpdateCounter valid only on structured buffers.
  SmDSInputControlPointCountRange, // DS input control point count must be [0..%0].  %1 specified.
  SmDomainLocationIdxOOB, // DomainLocation component index out of bounds for the domain.
  SmDxilVersion, // Target shader model requires specific Dxil Version
  SmGSInstanceCountRange, // GS instance count must be [1..%0].  %1 specified.
  SmGSOutputVertexCountRange, // GS output vertex count must be [0..%0].  %1 specified.
  SmGSTotalOutputVertexDataRange, // Declared output vertex count (%0) multiplied by the total number of declared scalar components of output data (%1) equals %2.  This value cannot be greater than %3.
  SmGSValidInputPrimitive, // GS input primitive unrecognized.
  SmGSValidOutputPrimitiveTopology, // GS output primitive topology unrecognized.
  SmHSInputControlPointCountRange, // HS input control point count must be [0..%0].  %1 specified.
  SmHullPassThruControlPointCountMatch, // For pass thru hull shader, input control point count must match output control point count
  SmInsideTessFactorSizeMatchDomain, // InsideTessFactor rows, columns (%0, %1) invalid for domain %2.  Expected %3 rows and 1 column.
  SmInvalidResourceCompType, // Invalid resource return type.
  SmInvalidResourceKind, // Invalid resources kind.
  SmInvalidSamplerFeedbackType, // Invalid sampler feedback type.
  SmInvalidTextureKindOnUAV, // Texture2DMS[Array] or TextureCube[Array] resources are not supported with UAVs.
  SmIsoLineOutputPrimitiveMismatch, // Hull Shader declared with IsoLine Domain must specify output primitive point or line. Triangle_cw or triangle_ccw output are not compatible with the IsoLine Domain.
  SmMaxMSSMSize, // Total Thread Group Shared Memory storage is %0, exceeded %1.
  SmMaxTGSMSize, // Total Thread Group Shared Memory storage is %0, exceeded %1.
  SmMaxTheadGroup, // Declared Thread Group Count %0 (X*Y*Z) is beyond the valid maximum of %1.
  SmMeshPSigRowCount, // For shader '%0', primitive output signatures are taking up more than %1 rows.
  SmMeshShaderInOutSize, // For shader '%0', payload plus output size is greater than %1.
  SmMeshShaderMaxPrimitiveCount, // MS max primitive output count must be [0..%0].  %1 specified.
  SmMeshShaderMaxVertexCount, // MS max vertex output count must be [0..%0].  %1 specified.
  SmMeshShaderOutputSize, // For shader '%0', vertex plus primitive output size is greater than %1.
  SmMeshShaderPayloadSize, // For mesh shader with entry '%0', payload size %1 is greater than maximum size of %2 bytes.
  SmMeshShaderPayloadSizeDeclared, // For mesh shader with entry '%0', payload size %1 is greater than declared size of %2 bytes.
  SmMeshTotalSigRowCount, // For shader '%0', vertex and primitive output signatures are taking up more than %1 rows.
  SmMeshVSigRowCount, // For shader '%0', vertex output signatures are taking up more than %1 rows.
  SmMultiStreamMustBePoint, // When multiple GS output streams are used they must be pointlists
  SmName, // Target shader model name must be known
  SmNoInterpMode, // Interpolation mode must be undefined for VS input/PS output/patch constant.
  SmNoPSOutputIdx, // Pixel shader output registers are not indexable.
  SmOpcode, // Opcode must be defined in target shader model
  SmOpcodeInInvalidFunction, // Invalid DXIL opcode usage like StorePatchConstant in patch constant function
  SmOperand, // Operand must be defined in target shader model.
  SmOutputControlPointCountRange, // output control point count must be [0..%0].  %1 specified.
  SmOutputControlPointsTotalScalars, // Total number of scalars across all HS output control points must not exceed .
  SmPSConsistentInterp, // Interpolation mode for PS input position must be linear_noperspective_centroid or linear_noperspective_sample when outputting oDepthGE or oDepthLE and not running at sample frequency (which is forced by inputting SV_SampleIndex or declaring an input linear_sample or linear_noperspective_sample).
  SmPSCoverageAndInnerCoverage, // InnerCoverage and Coverage are mutually exclusive.
  SmPSMultipleDepthSemantic, // Pixel Shader only allows one type of depth semantic to be declared.
  SmPSOutputSemantic, // Pixel Shader allows output semantics to be SV_Target, SV_Depth, SV_DepthGreaterEqual, SV_DepthLessEqual, SV_Coverage or SV_StencilRef, %0 found.
  SmPSTargetCol0, // SV_Target packed location must start at column 0.
  SmPSTargetIndexMatchesRow, // SV_Target semantic index must match packed row location.
  SmPatchConstantOnlyForHSDS, // patch constant signature only valid in HS and DS.
  SmROVOnlyInPS, // RasterizerOrdered objects are only allowed in 5.0+ pixel shaders.
  SmRayShaderPayloadSize, // For shader '%0', %1 size is smaller than argument's allocation size.
  SmRayShaderSignatures, // Ray tracing shader '%0' should not have any shader signatures.
  SmResourceRangeOverlap, // Resource ranges must not overlap
  SmSampleCountOnlyOn2DMS, // Only Texture2DMS/2DMSArray could has sample count.
  SmSemantic, // Semantic must be defined in target shader model
  SmStreamIndexRange, // Stream index (%0) must between 0 and %1.
  SmTessFactorForDomain, // Required TessFactor for domain not found declared anywhere in Patch Constant data.
  SmTessFactorSizeMatchDomain, // TessFactor rows, columns (%0, %1) invalid for domain %2.  Expected %3 rows and 1 column.
  SmThreadGroupChannelRange, // Declared Thread Group %0 size %1 outside valid range [%2..%3].
  SmTriOutputPrimitiveMismatch, // Hull Shader declared with Tri Domain must specify output primitive point, triangle_cw or triangle_ccw. Line output is not compatible with the Tri domain.
  SmUndefinedOutput, // Not all elements of output %0 were written.
  SmValidDomain, // Invalid Tessellator Domain specified. Must be isoline, tri or quad.
  SmViewIDNeedsSlot, // ViewID requires compatible space in pixel shader input signature
  SmZeroHSInputControlPointWithInput, // When HS input control point count is 0, no input signature should exist.

  // Type system
  TypesDefined, // Type must be defined based on DXIL primitives
  TypesI8, // I8 can only be used as immediate value for intrinsic.
  TypesIntWidth, // Int type must be of valid width
  TypesNoMultiDim, // Only one dimension allowed for array type.
  TypesNoPtrToPtr, // Pointers to pointers, or pointers in structures are not allowed.
  TypesNoVector, // Vector types must not be present
};
// VALRULE-ENUM:END

const char *GetValidationRuleText(ValidationRule value);
void GetValidationVersion(_Out_ unsigned *pMajor, _Out_ unsigned *pMinor);
HRESULT ValidateDxilModule(_In_ llvm::Module *pModule,
                           _In_opt_ llvm::Module *pDebugModule);

// DXIL Container Verification Functions (return false on failure)

bool VerifySignatureMatches(_In_ llvm::Module *pModule,
                            hlsl::DXIL::SignatureKind SigKind,
                            _In_reads_bytes_(SigSize) const void *pSigData,
                            _In_ uint32_t SigSize);

// PSV = data for Pipeline State Validation
bool VerifyPSVMatches(_In_ llvm::Module *pModule,
                      _In_reads_bytes_(PSVSize) const void *pPSVData,
                      _In_ uint32_t PSVSize);

// PSV = data for Pipeline State Validation
bool VerifyRDATMatches(_In_ llvm::Module *pModule,
                       _In_reads_bytes_(RDATSize) const void *pRDATData,
                       _In_ uint32_t RDATSize);

bool VerifyFeatureInfoMatches(_In_ llvm::Module *pModule,
                              _In_reads_bytes_(FeatureInfoSize) const void *pFeatureInfoData,
                              _In_ uint32_t FeatureInfoSize);

// Validate the container parts, assuming supplied module is valid, loaded from the container provided
struct DxilContainerHeader;
HRESULT ValidateDxilContainerParts(_In_ llvm::Module *pModule,
                                   _In_opt_ llvm::Module *pDebugModule,
                                   _In_reads_bytes_(ContainerSize) const DxilContainerHeader *pContainer,
                                   _In_ uint32_t ContainerSize);

// Loads module, validating load, but not module.
HRESULT ValidateLoadModule(_In_reads_bytes_(ILLength) const char *pIL,
                           _In_ uint32_t ILLength,
                           _In_ std::unique_ptr<llvm::Module> &pModule,
                           _In_ llvm::LLVMContext &Ctx,
                           _In_ llvm::raw_ostream &DiagStream,
                           _In_ unsigned bLazyLoad);

// Loads module from container, validating load, but not module.
HRESULT ValidateLoadModuleFromContainer(
    _In_reads_bytes_(ContainerSize) const void *pContainer,
    _In_ uint32_t ContainerSize, _In_ std::unique_ptr<llvm::Module> &pModule,
    _In_ std::unique_ptr<llvm::Module> &pDebugModule,
    _In_ llvm::LLVMContext &Ctx, llvm::LLVMContext &DbgCtx,
    _In_ llvm::raw_ostream &DiagStream);
// Lazy loads module from container, validating load, but not module.
HRESULT ValidateLoadModuleFromContainerLazy(
    _In_reads_bytes_(ContainerSize) const void *pContainer,
    _In_ uint32_t ContainerSize, _In_ std::unique_ptr<llvm::Module> &pModule,
    _In_ std::unique_ptr<llvm::Module> &pDebugModule,
    _In_ llvm::LLVMContext &Ctx, llvm::LLVMContext &DbgCtx,
    _In_ llvm::raw_ostream &DiagStream);

// Load and validate Dxil module from bitcode.
HRESULT ValidateDxilBitcode(_In_reads_bytes_(ILLength) const char *pIL,
                            _In_ uint32_t ILLength,
                            _In_ llvm::raw_ostream &DiagStream);

// Full container validation, including ValidateDxilModule
HRESULT ValidateDxilContainer(_In_reads_bytes_(ContainerSize) const void *pContainer,
                              _In_ uint32_t ContainerSize,
                              _In_ llvm::raw_ostream &DiagStream);

class PrintDiagnosticContext {
private:
  llvm::DiagnosticPrinter &m_Printer;
  bool m_errorsFound;
  bool m_warningsFound;

public:
  PrintDiagnosticContext(llvm::DiagnosticPrinter &printer);

  bool HasErrors() const;
  bool HasWarnings() const;
  void Handle(const llvm::DiagnosticInfo &DI);

  static void PrintDiagnosticHandler(const llvm::DiagnosticInfo &DI,
                                     void *Context);
};
}
