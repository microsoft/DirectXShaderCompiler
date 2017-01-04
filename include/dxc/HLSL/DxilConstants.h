///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilConstants.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Essential DXIL constants.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>

namespace hlsl {

/* <py>
import hctdb_instrhelp
</py> */

// TODO:
//   2. get rid of DXIL namespace.
//   3. use class enum for shader flags.
//   4. use class enum for address spaces.

namespace DXIL {
  // DXIL version.
  const unsigned kDxilMajor = 0;
  const unsigned kDxilMinor = 7;

  inline unsigned MakeDxilVersion(unsigned DxilMajor, unsigned DxilMinor) {
    return 0 | (DxilMajor << 8) | (DxilMinor);
  }
  inline unsigned GetCurrentDxilVersion() { return MakeDxilVersion(kDxilMajor, kDxilMinor); }
  inline unsigned GetDxilVersionMajor(unsigned DxilVersion) { return (DxilVersion >> 8) & 0xFF; }
  inline unsigned GetDxilVersionMinor(unsigned DxilVersion) { return DxilVersion & 0xFF; }

  // Shader flags.
  const unsigned kDisableOptimizations          = 0x00000001; // D3D11_1_SB_GLOBAL_FLAG_SKIP_OPTIMIZATION
  const unsigned kDisableMathRefactoring        = 0x00000002; //~D3D10_SB_GLOBAL_FLAG_REFACTORING_ALLOWED
  const unsigned kEnableDoublePrecision         = 0x00000004; // D3D11_SB_GLOBAL_FLAG_ENABLE_DOUBLE_PRECISION_FLOAT_OPS
  const unsigned kForceEarlyDepthStencil        = 0x00000008; // D3D11_SB_GLOBAL_FLAG_FORCE_EARLY_DEPTH_STENCIL
  const unsigned kEnableRawAndStructuredBuffers = 0x00000010; // D3D11_SB_GLOBAL_FLAG_ENABLE_RAW_AND_STRUCTURED_BUFFERS
  const unsigned kEnableMinPrecision            = 0x00000020; // D3D11_1_SB_GLOBAL_FLAG_ENABLE_MINIMUM_PRECISION
  const unsigned kEnableDoubleExtensions        = 0x00000040; // D3D11_1_SB_GLOBAL_FLAG_ENABLE_DOUBLE_EXTENSIONS
  const unsigned kEnableMSAD                    = 0x00000080; // D3D11_1_SB_GLOBAL_FLAG_ENABLE_SHADER_EXTENSIONS
  const unsigned kAllResourcesBound             = 0x00000100; // D3D12_SB_GLOBAL_FLAG_ALL_RESOURCES_BOUND

  const unsigned kNumOutputStreams = 4;
  const unsigned kNumClipPlanes = 6;

  // TODO: move these to appropriate places (ShaderModel.cpp?)
  const unsigned kMaxTempRegCount = 4096;         // DXBC only
  const unsigned kMaxD3D11ResourceCount = 128;
  const unsigned kMaxD3D11SamplerCount = 16;
  const unsigned kMaxD3D11CBufferCount = 14;
  const unsigned kMaxCBufferSize = 4096;
  const unsigned kMaxStructBufferStride = 2048;
  const unsigned kMaxD3D10UAVCount = 8;
  const unsigned kMaxD3D10CS4UAVCount = 1;
  const unsigned kMaxD3D11UAVCount = 64;
  const unsigned kMaxD3D12TextureCount = 2048;
  const unsigned kMaxD3D12SamplerCount = 128;
  const unsigned kMaxD3D12UAVCount = 512;
  const unsigned kMaxD3D12CBufferCount = 256;
  const unsigned kMaxHSOutputControlPointsTotalScalars = 3968;
  const unsigned kMaxHSOutputPatchConstantTotalScalars = 32*4;
  const unsigned kMaxOutputTotalScalars = 32*4;
  const unsigned kMaxInputTotalScalars = 32*4;
  const unsigned kMaxClipOrCullDistanceElementCount = 2;
  const unsigned kMaxClipOrCullDistanceCount = 2 * 4;
  const unsigned kMaxGSOutputVertexCount = 1024;
  const unsigned kMaxGSInstanceCount = 32;
  const unsigned kMaxIAPatchControlPointCount = 32;
  const float kHSMaxTessFactorLowerBound = 1.0f;
  const float kHSMaxTessFactorUpperBound = 64.0f;
  const unsigned kMaxCSThreadsPerGroup = 1024;
  const unsigned kMaxCSThreadGroupX	= 1024;
  const unsigned kMaxCSThreadGroupY	= 1024;
  const unsigned kMaxCSThreadGroupZ = 64;
  const unsigned kMinCSThreadGroupX = 1;
  const unsigned kMinCSThreadGroupY = 1;
  const unsigned kMinCSThreadGroupZ = 1;
  const unsigned kMaxCS4XThreadsPerGroup = 768;
  const unsigned kMaxCS4XThreadGroupX	= 768;
  const unsigned kMaxCS4XThreadGroupY	= 768;
  const unsigned kMaxTGSMSize = 8192*4;
  const unsigned kMaxGSOutputTotalScalars = 1024;

  const float kMaxMipLodBias = 15.99f;
  const float kMinMipLodBias = -16.0f;

  enum class ComponentType : uint8_t { 
    Invalid = 0,
    I1, I16, U16, I32, U32, I64, U64,
    F16, F32, F64,
    SNormF16, UNormF16, SNormF32, UNormF32, SNormF64, UNormF64,
    LastEntry };

  enum class InterpolationMode : uint8_t {
    Undefined                   = 0,
    Constant                    = 1,
    Linear                      = 2,
    LinearCentroid              = 3,
    LinearNoperspective         = 4,
    LinearNoperspectiveCentroid = 5,
    LinearSample                = 6,
    LinearNoperspectiveSample   = 7,
    Invalid                     = 8
  };

  enum class SignatureKind {
    Invalid = 0,
    Input,
    Output,
    PatchConstant,
  };

  enum class ShaderKind {
    Pixel = 0,
    Vertex,
    Geometry,
    Hull,
    Domain,
    Compute,
    Invalid,
  };

  /* <py::lines('SemanticKind-ENUM')>hctdb_instrhelp.get_enum_decl("SemanticKind", hide_val=True, sort_val=False)</py>*/
  // SemanticKind-ENUM:BEGIN
  // Semantic kind; Arbitrary or specific system value.
  enum class SemanticKind : unsigned {
    Arbitrary,
    VertexID,
    InstanceID,
    Position,
    RenderTargetArrayIndex,
    ViewPortArrayIndex,
    ClipDistance,
    CullDistance,
    OutputControlPointID,
    DomainLocation,
    PrimitiveID,
    GSInstanceID,
    SampleIndex,
    IsFrontFace,
    Coverage,
    InnerCoverage,
    Target,
    Depth,
    DepthLessEqual,
    DepthGreaterEqual,
    StencilRef,
    DispatchThreadID,
    GroupID,
    GroupIndex,
    GroupThreadID,
    TessFactor,
    InsideTessFactor,
    Invalid,
  };
  // SemanticKind-ENUM:END

  /* <py::lines('SigPointKind-ENUM')>hctdb_instrhelp.get_enum_decl("SigPointKind", hide_val=True, sort_val=False)</py>*/
  // SigPointKind-ENUM:BEGIN
  // Signature Point is more specific than shader stage or signature as it is unique in both stage and item dimensionality or frequency.
  enum class SigPointKind : unsigned {
    VSIn, // Ordinary Vertex Shader input from Input Assembler
    VSOut, // Ordinary Vertex Shader output that may feed Rasterizer
    PCIn, // Patch Constant function non-patch inputs
    HSIn, // Hull Shader function non-patch inputs
    HSCPIn, // Hull Shader patch inputs - Control Points
    HSCPOut, // Hull Shader function output - Control Point
    PCOut, // Patch Constant function output - Patch Constant data passed to Domain Shader
    DSIn, // Domain Shader regular input - Patch Constant data plus system values
    DSCPIn, // Domain Shader patch input - Control Points
    DSOut, // Domain Shader output - vertex data that may feed Rasterizer
    GSVIn, // Geometry Shader vertex input - qualified with primitive type
    GSIn, // Geometry Shader non-vertex inputs (system values)
    GSOut, // Geometry Shader output - vertex data that may feed Rasterizer
    PSIn, // Pixel Shader input
    PSOut, // Pixel Shader output
    CSIn, // Compute Shader input
    Invalid,
  };
  // SigPointKind-ENUM:END

  /* <py::lines('SemanticInterpretationKind-ENUM')>hctdb_instrhelp.get_enum_decl("SemanticInterpretationKind", hide_val=True, sort_val=False)</py>*/
  // SemanticInterpretationKind-ENUM:BEGIN
  // Defines how a semantic is interpreted at a particular SignaturePoint
  enum class SemanticInterpretationKind : unsigned {
    NA, // Not Available
    SV, // Normal System Value
    SGV, // System Generated Value (sorted last)
    Arb, // Treated as Arbitrary
    NotInSig, // Not included in signature (intrinsic access)
    NotPacked, // Included in signature, but does not contribute to packing
    Target, // Special handling for SV_Target
    TessFactor, // Special handling for tessellation factors
    Shadow, // Shadow element must be added to a signature for compatibility
    Invalid,
  };
  // SemanticInterpretationKind-ENUM:END

  /* <py::lines('PackingKind-ENUM')>hctdb_instrhelp.get_enum_decl("PackingKind", hide_val=True, sort_val=False)</py>*/
  // PackingKind-ENUM:BEGIN
  // Kind of signature point
  enum class PackingKind : unsigned {
    None, // No packing should be performed
    InputAssembler, // Vertex Shader input from Input Assembler
    Vertex, // Vertex that may feed the Rasterizer
    PatchConstant, // Patch constant signature
    Target, // Render Target (Pixel Shader Output)
    Invalid,
  };
  // PackingKind-ENUM:END

  enum class SamplerKind : unsigned {
    Default = 0,
    Comparison,
    Mono,
    Invalid,
  };

  enum class ResourceClass {
    SRV = 0,
    UAV,
    CBuffer,
    Sampler,
    Invalid
  };

  enum class ResourceKind : unsigned {
    Invalid = 0,
    Texture1D,
    Texture2D,
    Texture2DMS,
    Texture3D,
    TextureCube,
    Texture1DArray,
    Texture2DArray,
    Texture2DMSArray,
    TextureCubeArray,
    TypedBuffer,
    RawBuffer,
    StructuredBuffer,
    CBuffer,
    Sampler,
    TBuffer,
    NumEntries,
  };

  // TODO: change opcodes.
  /* <py::lines('OPCODE-ENUM')>hctdb_instrhelp.get_enum_decl("OpCode")</py>*/
  // OPCODE-ENUM:BEGIN
  // Enumeration for operations specified by DXIL
  enum class OpCode : unsigned {
    // 
    GlobalOrderedCountIncReserved = 129, // reserved
    ToDelete1 = 101, // reserved
    ToDelete2 = 102, // reserved
    ToDelete3 = 104, // reserved
    ToDelete4 = 105, // reserved
    ToDelete5 = 76, // reserved
    ToDelete6 = 77, // reserved
  
    // Binary float
    FMax = 34, // returns the FMax of the input values
    FMin = 35, // returns the FMin of the input values
  
    // Binary int with carry
    IAddc = 43, // returns the IAddc of the input values
    ISubc = 45, // returns the ISubc of the input values
    UAddc = 44, // returns the UAddc of the input values
    USubc = 46, // returns the USubc of the input values
  
    // Binary int with two outputs
    IMul = 40, // returns the IMul of the input values
    UDiv = 42, // returns the UDiv of the input values
    UMul = 41, // returns the UMul of the input values
  
    // Binary int
    IMax = 36, // returns the IMax of the input values
    IMin = 37, // returns the IMin of the input values
    UMax = 38, // returns the UMax of the input values
    UMin = 39, // returns the UMin of the input values
  
    // Bitcasts with different sizes
    BitcastF16toI16 = 133, // bitcast between different sizes
    BitcastF32toI32 = 135, // bitcast between different sizes
    BitcastF64toI64 = 137, // bitcast between different sizes
    BitcastI16toF16 = 132, // bitcast between different sizes
    BitcastI32toF32 = 134, // bitcast between different sizes
    BitcastI64toF64 = 136, // bitcast between different sizes
  
    // Compute shader
    FlattenedThreadIdInGroup = 96, // provides a flattened index for a given thread within a given group (SV_GroupIndex)
    GroupId = 94, // reads the group ID (SV_GroupID)
    ThreadId = 93, // reads the thread ID
    ThreadIdInGroup = 95, // reads the thread ID within the group (SV_GroupThreadID)
  
    // Domain and hull shader
    LoadOutputControlPoint = 106, // LoadOutputControlPoint
    LoadPatchConstant = 107, // LoadPatchConstant
  
    // Domain shader
    DomainLocation = 108, // DomainLocation
  
    // Dot
    Dot2 = 55, // two-dimensional vector dot-product
    Dot3 = 56, // three-dimensional vector dot-product
    Dot4 = 57, // four-dimensional vector dot-product
  
    // Double precision
    LegacyDoubleToFloat = 141, // legacy fuction to convert double to float
    LegacyDoubleToSInt32 = 142, // legacy fuction to convert double to int32
    LegacyDoubleToUInt32 = 143, // legacy fuction to convert double to uint32
    MakeDouble = 100, // creates a double value
    SplitDouble = 103, // splits a double into low and high parts
  
    // GS
    GSInstanceID = 138, // GSInstanceID
  
    // Geometry shader
    CutStream = 98, // completes the current primitive topology at the specified stream
    EmitStream = 97, // emits a vertex to a given stream
    EmitThenCutStream = 99, // equivalent to an EmitStream followed by a CutStream
  
    // Hull shader
    OutputControlPointID = 110, // OutputControlPointID
    PrimitiveID = 111, // PrimitiveID
    StorePatchConstant = 109, // StorePatchConstant
  
    // Legacy floating-point
    LegacyF16ToF32 = 140, // legacy fuction to convert half (f16) to float (f32) (this is not related to min-precision)
    LegacyF32ToF16 = 139, // legacy fuction to convert float (f32) to half (f16) (this is not related to min-precision)
  
    // Other
    CycleCounterLegacy = 112, // CycleCounterLegacy
  
    // Pixel shader
    CalculateLOD = 84, // calculates the level of detail
    Coverage = 147, // returns the coverage mask input in a pixel shader
    DerivCoarseX = 86, // computes the rate of change of components per stamp
    DerivCoarseY = 87, // computes the rate of change of components per stamp
    DerivFineX = 88, // computes the rate of change of components per pixel
    DerivFineY = 89, // computes the rate of change of components per pixel
    Discard = 85, // discard the current pixel
    EvalCentroid = 92, // evaluates an input attribute at pixel center
    EvalSampleIndex = 91, // evaluates an input attribute at a sample location
    EvalSnapped = 90, // evaluates an input attribute at pixel center with an offset
    InnerCoverage = 148, // returns underestimated coverage input from conservative rasterization in a pixel shader
    SampleIndex = 146, // returns the sample index in a sample-frequency pixel shader
  
    // Quaternary
    Bfi = 54, // given a bit range from the LSB of a number, places that number of bits in another number at any offset
  
    // Resources - gather
    TextureGather = 74, // gathers the four texels that would be used in a bi-linear filtering operation
    TextureGatherCmp = 75, // same as TextureGather, except this instrution performs comparison on texels, similar to SampleCmp
  
    // Resources - sample
    RenderTargetGetSampleCount = 80, // gets the number of samples for a render target
    RenderTargetGetSamplePosition = 79, // gets the position of the specified sample
    Sample = 61, // samples a texture
    SampleBias = 62, // samples a texture after applying the input bias to the mipmap level
    SampleCmp = 65, // samples a texture and compares a single component against the specified comparison value
    SampleCmpLevelZero = 66, // samples a texture and compares a single component against the specified comparison value
    SampleGrad = 64, // samples a texture using a gradient to influence the way the sample location is calculated
    SampleLevel = 63, // samples a texture using a mipmap-level offset
    Texture2DMSGetSamplePosition = 78, // gets the position of the specified sample
  
    // Resources
    BufferLoad = 69, // reads from a TypedBuffer
    BufferStore = 70, // writes to a RWTypedBuffer
    BufferUpdateCounter = 71, // atomically increments/decrements the hidden 32-bit counter stored with a Count or Append UAV
    CBufferLoad = 59, // loads a value from a constant buffer resource
    CBufferLoadLegacy = 60, // loads a value from a constant buffer resource
    CheckAccessFullyMapped = 72, // determines whether all values from a Sample, Gather, or Load operation accessed mapped tiles in a tiled resource
    CreateHandle = 58, // creates the handle to a resource
    GetDimensions = 73, // gets texture size information
    TextureLoad = 67, // reads texel data without any filtering or sampling
    TextureStore = 68, // reads texel data without any filtering or sampling
  
    // Synchronization
    AtomicBinOp = 81, // performs an atomic operation on two operands
    AtomicCompareExchange = 82, // atomic compare and exchange to memory
    Barrier = 83, // inserts a memory barrier in the shader
  
    // Temporary, indexable, input, output registers
    LoadInput = 4, // loads the value from shader input
    MinPrecXRegLoad = 2, // helper load operation for minprecision
    MinPrecXRegStore = 3, // helper store operation for minprecision
    StoreOutput = 5, // stores the value to shader output
    TempRegLoad = 0, // helper load operation
    TempRegStore = 1, // helper store operation
  
    // Tertiary float
    FMad = 47, // performs a fused multiply add (FMA) of the form a * b + c
    Fma = 48, // performs a fused multiply add (FMA) of the form a * b + c
  
    // Tertiary int
    IMad = 49, // performs an integral IMad
    Ibfe = 52, // performs an integral Ibfe
    Msad = 51, // performs an integral Msad
    UMad = 50, // performs an integral UMad
    Ubfe = 53, // performs an integral Ubfe
  
    // Unary float - rounding
    Round_ne = 25, // returns the Round_ne
    Round_ni = 26, // returns the Round_ni
    Round_pi = 27, // returns the Round_pi
    Round_z = 28, // returns the Round_z
  
    // Unary float
    Acos = 15, // returns the Acos
    Asin = 16, // returns the Asin
    Atan = 17, // returns the Atan
    Cos = 12, // returns cosine(theta) for theta in radians.
    Exp = 20, // returns the Exp
    FAbs = 6, // returns the absolute value of the input value.
    Frc = 21, // returns the Frc
    Hcos = 18, // returns the Hcos
    Hsin = 19, // returns the Hsin
    Htan = 113, // returns the hyperbolic tangent of the specified value
    IsFinite = 10, // returns the IsFinite
    IsInf = 9, // returns the IsInf
    IsNaN = 8, // returns the IsNaN
    IsNormal = 11, // returns the IsNormal
    Log = 22, // returns the Log
    Rsqrt = 24, // returns the Rsqrt
    Saturate = 7, // clamps the result of a single or double precision floating point value to [0.0f...1.0f]
    Sin = 13, // returns the Sin
    Sqrt = 23, // returns the Sqrt
    Tan = 14, // returns the Tan
  
    // Unary int
    Bfrev = 29, // returns the reverse bit pattern of the input value
    Countbits = 30, // returns the Countbits
    FirstbitHi = 32, // returns src != 0? (BitWidth-1 - FirstbitHi) : -1
    FirstbitLo = 31, // returns the FirstbitLo
    FirstbitSHi = 33, // returns src != 0? (BitWidth-1 - FirstbitSHi) : -1
  
    // Wave
    QuadOp = 131, // returns the result of a quad-level operation
    QuadReadLaneAt = 130, // reads from a lane in the quad
    WaveActiveAllEqual = 121, // returns 1 if all the lanes have the same value
    WaveActiveBallot = 122, // returns a struct with a bit set for each lane where the condition is true
    WaveActiveBit = 126, // returns the result of the operation across all lanes
    WaveActiveOp = 125, // returns the result the operation across waves
    WaveAllBitCount = 144, // returns the count of bits set to 1 across the wave
    WaveAllTrue = 120, // returns 1 if all the lanes evaluate the value to true
    WaveAnyTrue = 119, // returns 1 if any of the lane evaluates the value to true
    WaveCaptureReserved = 114, // reserved
    WaveGetLaneCount = 117, // returns the number of lanes in the wave
    WaveGetLaneIndex = 116, // returns the index of the current lane in the wave
    WaveGetOrderedIndex = 128, // reserved
    WaveIsFirstLane = 115, // returns 1 for the first lane in the wave
    WaveIsHelperLaneReserved = 118, // reserved
    WavePrefixBitCount = 145, // returns the count of bits set to 1 on prior lanes
    WavePrefixOp = 127, // returns the result of the operation on prior lanes
    WaveReadLaneAt = 123, // returns the value from the specified lane
    WaveReadLaneFirst = 124, // returns the value from the first lane
  
    NumOpCodes = 149 // exclusive last value of enumeration
  };
  // OPCODE-ENUM:END

  /* <py::lines('OPCODECLASS-ENUM')>hctdb_instrhelp.get_enum_decl("OpCodeClass")</py>*/
  // OPCODECLASS-ENUM:BEGIN
  // Groups for DXIL operations with equivalent function templates
  enum class OpCodeClass : unsigned {
    // 
    Reserved,
  
    // Binary int with carry
    BinaryWithCarry,
  
    // Binary int with two outputs
    BinaryWithTwoOuts,
  
    // Binary int
    Binary,
  
    // Bitcasts with different sizes
    BitcastF16toI16,
    BitcastF32toI32,
    BitcastF64toI64,
    BitcastI16toF16,
    BitcastI32toF32,
    BitcastI64toF64,
  
    // Compute shader
    FlattenedThreadIdInGroup,
    GroupId,
    ThreadId,
    ThreadIdInGroup,
  
    // Domain and hull shader
    LoadOutputControlPoint,
    LoadPatchConstant,
  
    // Domain shader
    DomainLocation,
  
    // Dot
    Dot2,
    Dot3,
    Dot4,
  
    // Double precision
    LegacyDoubleToFloat,
    LegacyDoubleToSInt32,
    LegacyDoubleToUInt32,
    MakeDouble,
    SplitDouble,
  
    // GS
    GSInstanceID,
  
    // Geometry shader
    CutStream,
    EmitStream,
    EmitThenCutStream,
  
    // Hull shader
    OutputControlPointID,
    PrimitiveID,
    StorePatchConstant,
  
    // LLVM Instructions
    LlvmInst,
  
    // Legacy floating-point
    LegacyF16ToF32,
    LegacyF32ToF16,
  
    // Other
    CycleCounterLegacy,
  
    // Pixel shader
    CalculateLOD,
    Coverage,
    Discard,
    EvalCentroid,
    EvalSampleIndex,
    EvalSnapped,
    InnerCoverage,
    SampleIndex,
  
    // Quaternary
    Quaternary,
  
    // Resources - gather
    TextureGather,
    TextureGatherCmp,
  
    // Resources - sample
    RenderTargetGetSampleCount,
    RenderTargetGetSamplePosition,
    Sample,
    SampleBias,
    SampleCmp,
    SampleCmpLevelZero,
    SampleGrad,
    SampleLevel,
    Texture2DMSGetSamplePosition,
  
    // Resources
    BufferLoad,
    BufferStore,
    BufferUpdateCounter,
    CBufferLoad,
    CBufferLoadLegacy,
    CheckAccessFullyMapped,
    CreateHandle,
    GetDimensions,
    TextureLoad,
    TextureStore,
  
    // Synchronization
    AtomicBinOp,
    AtomicCompareExchange,
    Barrier,
  
    // Temporary, indexable, input, output registers
    LoadInput,
    MinPrecXRegLoad,
    MinPrecXRegStore,
    StoreOutput,
    TempRegLoad,
    TempRegStore,
  
    // Tertiary int
    Tertiary,
  
    // Unary float
    IsSpecialFloat,
    Unary,
  
    // Unary int
    UnaryBits,
  
    // Wave
    QuadOp,
    QuadReadLaneAt,
    WaveActiveAllEqual,
    WaveActiveBallot,
    WaveActiveBit,
    WaveActiveOp,
    WaveAllOp,
    WaveAllTrue,
    WaveAnyTrue,
    WaveGetLaneCount,
    WaveGetLaneIndex,
    WaveIsFirstLane,
    WavePrefixOp,
    WaveReadLaneAt,
    WaveReadLaneFirst,
  
    NumOpClasses = 94 // exclusive last value of enumeration
  };
  // OPCODECLASS-ENUM:END

  // Operand Index for every OpCodeClass.
  namespace OperandIndex {
    // Opcode is always operand 0.
    const unsigned kOpcodeIdx = 0;

    // Unary operators.
    const unsigned kUnarySrc0OpIdx = 1;

    // Binary operators.
    const unsigned kBinarySrc0OpIdx = 1;
    const unsigned kBinarySrc1OpIdx = 2;

    // Trinary operators.
    const unsigned kTrinarySrc0OpIdx = 1;
    const unsigned kTrinarySrc1OpIdx = 2;
    const unsigned kTrinarySrc2OpIdx = 3;

    // LoadInput.
    const unsigned kLoadInputIDOpIdx = 1;
    const unsigned kLoadInputRowOpIdx = 2;
    const unsigned kLoadInputColOpIdx = 3;
    const unsigned kLoadInputVertexIDOpIdx = 4;

    // StoreOutput.
    const unsigned kStoreOutputIDOpIdx = 1;
    const unsigned kStoreOutputRowOpIdx = 2;
    const unsigned kStoreOutputColOpIdx = 3;
    const unsigned kStoreOutputValOpIdx = 4;

    // DomainLocation.
    const unsigned kDomainLocationColOpIdx = 1;

    // BufferStore.
    const unsigned kBufferStoreHandleOpIdx = 1;
    const unsigned kBufferStoreCoord0OpIdx = 2;
    const unsigned kBufferStoreCoord1OpIdx = 3;
    const unsigned kBufferStoreVal0OpIdx = 4;
    const unsigned kBufferStoreVal1OpIdx = 5;
    const unsigned kBufferStoreVal2OpIdx = 6;
    const unsigned kBufferStoreVal3OpIdx = 7;
    const unsigned kBufferStoreMaskOpIdx = 8;

    // TextureStore.
    const unsigned kTextureStoreHandleOpIdx = 1;
    const unsigned kTextureStoreCoord0OpIdx = 2;
    const unsigned kTextureStoreCoord1OpIdx = 3;
    const unsigned kTextureStoreCoord2OpIdx = 4;
    const unsigned kTextureStoreVal0OpIdx = 5;
    const unsigned kTextureStoreVal1OpIdx = 6;
    const unsigned kTextureStoreVal2OpIdx = 7;
    const unsigned kTextureStoreVal3OpIdx = 8;
    const unsigned kTextureStoreMaskOpIdx = 9;

    // TextureGather.
    const unsigned kTextureGatherTexHandleOpIdx = 1;
    const unsigned kTextureGatherSamplerHandleOpIdx = 2;
    const unsigned kTextureGatherCoord0OpIdx = 3;
    const unsigned kTextureGatherCoord1OpIdx = 4;
    const unsigned kTextureGatherCoord2OpIdx = 5;
    const unsigned kTextureGatherCoord3OpIdx = 6;
    const unsigned kTextureGatherOffset0OpIdx = 7;
    const unsigned kTextureGatherOffset1OpIdx = 8;
    const unsigned kTextureGatherOffset2OpIdx = 9;
    const unsigned kTextureGatherChannelOpIdx = 10;
    // TextureGatherCmp.
    const unsigned kTextureGatherCmpCmpValOpIdx = 11;

    // AtomicBinOp.
    const unsigned kAtomicBinOpCoord0OpIdx = 3;
    const unsigned kAtomicBinOpCoord1OpIdx = 4;
    const unsigned kAtomicBinOpCoord2OpIdx = 5;

    // AtomicCmpExchange.
    const unsigned kAtomicCmpExchangeCoord0OpIdx = 2;
    const unsigned kAtomicCmpExchangeCoord1OpIdx = 3;
    const unsigned kAtomicCmpExchangeCoord2OpIdx = 4;

    // CreateHandle
    const unsigned kCreateHandleResClassOpIdx = 1;
    const unsigned kCreateHandleResIDOpIdx = 2;
    const unsigned kCreateHandleResIndexOpIdx = 3;
    const unsigned kCreateHandleIsUniformOpIdx = 4;

    // Emit/Cut
    const unsigned kStreamEmitCutIDOpIdx = 1;
    // TODO: add operand index for all the OpCodeClass.
  }

  // Atomic binary operation kind.
  enum class AtomicBinOpCode : unsigned {
    Add,
    And,
    Or,
    Xor,
    IMin,
    IMax,
    UMin,
    UMax,
    Exchange,
    Invalid           // Must be last.
  };

  // Barrier/fence modes.
  enum class BarrierMode : unsigned {
    SyncThreadGroup       = 0x00000001,
    UAVFenceGlobal        = 0x00000002,
    UAVFenceThreadGroup   = 0x00000004,
    TGSMFence             = 0x00000008,
  };

  // Address space.
  const unsigned kDefaultAddrSpace = 0;
  const unsigned kDeviceMemoryAddrSpace = 1;
  const unsigned kCBufferAddrSpace = 2;
  const unsigned kTGSMAddrSpace = 3;
  const unsigned kGenericPointerAddrSpace = 4;
  const unsigned kImmediateCBufferAddrSpace = 5;

  // Input primitive.
  enum class InputPrimitive : unsigned {
    Undefined = 0,
    Point = 1,
    Line = 2,
    Triangle = 3,
    Reserved4 = 4,
    Reserved5 = 5,
    LineWithAdjacency = 6,
    TriangleWithAdjacency = 7,
    ControlPointPatch1 = 8,
    ControlPointPatch2 = 9,
    ControlPointPatch3 = 10,
    ControlPointPatch4 = 11,
    ControlPointPatch5 = 12,
    ControlPointPatch6 = 13,
    ControlPointPatch7 = 14,
    ControlPointPatch8 = 15,
    ControlPointPatch9 = 16,
    ControlPointPatch10 = 17,
    ControlPointPatch11 = 18,
    ControlPointPatch12 = 19,
    ControlPointPatch13 = 20,
    ControlPointPatch14 = 21,
    ControlPointPatch15 = 22,
    ControlPointPatch16 = 23,
    ControlPointPatch17 = 24,
    ControlPointPatch18 = 25,
    ControlPointPatch19 = 26,
    ControlPointPatch20 = 27,
    ControlPointPatch21 = 28,
    ControlPointPatch22 = 29,
    ControlPointPatch23 = 30,
    ControlPointPatch24 = 31,
    ControlPointPatch25 = 32,
    ControlPointPatch26 = 33,
    ControlPointPatch27 = 34,
    ControlPointPatch28 = 35,
    ControlPointPatch29 = 36,
    ControlPointPatch30 = 37,
    ControlPointPatch31 = 38,
    ControlPointPatch32 = 39,

    LastEntry,
  };

  // Primitive topology.
  enum class PrimitiveTopology : unsigned {
    Undefined = 0,
    PointList = 1,
    LineList = 2,
    LineStrip = 3,
    TriangleList = 4,
    TriangleStrip = 5,

    LastEntry,
  };

  enum class TessellatorDomain
  {
    Undefined = 0,
    IsoLine = 1,
    Tri = 2,
    Quad = 3,

    LastEntry,
  };

  enum class TessellatorOutputPrimitive
  {
    Undefined = 0,
    Point = 1,
    Line = 2,
    TriangleCW = 3,
    TriangleCCW = 4,

    LastEntry,
  };

  // Tessellator partitioning.
  enum class TessellatorPartitioning : unsigned {
    Undefined = 0,
    Integer,
    Pow2,
    FractionalOdd,
    FractionalEven,

    LastEntry,
  };

  // Kind of quad-level operation
  enum class QuadOpKind {
    ReadAcrossX = 0, // returns the value from the other lane in the quad in the horizontal direction
    ReadAcrossY = 1, // returns the value from the other lane in the quad in the vertical direction
    ReadAcrossDiagonal = 2, // returns the value from the lane across the quad in horizontal and vertical direction
  };

  /* <py::lines('WAVEBITOPKIND-ENUM')>hctdb_instrhelp.get_enum_decl("WaveBitOpKind")</py>*/
  // WAVEBITOPKIND-ENUM:BEGIN
  // Kind of bitwise cross-lane operation
  enum class WaveBitOpKind : unsigned {
    And = 0, // bitwise and of values
    Or = 1, // bitwise or of values
    Xor = 2, // bitwise xor of values
  };
  // WAVEBITOPKIND-ENUM:END

  /* <py::lines('WAVEOPKIND-ENUM')>hctdb_instrhelp.get_enum_decl("WaveOpKind")</py>*/
  // WAVEOPKIND-ENUM:BEGIN
  // Kind of cross-lane operation
  enum class WaveOpKind : unsigned {
    Max = 3, // maximum value
    Min = 2, // minimum value
    Product = 1, // product of values
    Sum = 0, // sum of values
  };
  // WAVEOPKIND-ENUM:END

  /* <py::lines('SIGNEDOPKIND-ENUM')>hctdb_instrhelp.get_enum_decl("SignedOpKind")</py>*/
  // SIGNEDOPKIND-ENUM:BEGIN
  // Sign vs. unsigned operands for operation
  enum class SignedOpKind : unsigned {
    Signed = 0, // signed integer or floating-point operands
    Unsigned = 1, // unsigned integer operands
  };
  // SIGNEDOPKIND-ENUM:END

  // Kind of control flow hint
  enum class ControlFlowHint : unsigned {
    Undefined = 0,
    Branch = 1,
    Flatten = 2,
    FastOpt = 3,
    AllowUavCondition = 4,
    ForceCase = 5,
    Call = 6,
    // Loop and Unroll is using llvm.loop.unroll Metadata.

    LastEntry,
  };

  // XYZW component mask.
  const uint8_t kCompMask_X     = 0x1;
  const uint8_t kCompMask_Y     = 0x2;
  const uint8_t kCompMask_Z     = 0x4;
  const uint8_t kCompMask_W     = 0x8;
  const uint8_t kCompMask_All   = 0xF;

} // namespace DXIL

} // namespace hlsl
