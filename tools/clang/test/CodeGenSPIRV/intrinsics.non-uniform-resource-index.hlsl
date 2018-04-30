// Run: %dxc -T ps_6_0 -E main

// CHECK: OpCapability ShaderNonUniformEXT
// CHECK: OpCapability SampledImageArrayNonUniformIndexingEXT
// CHECK: OpCapability StorageImageArrayNonUniformIndexingEXT
// CHECK: OpCapability UniformTexelBufferArrayNonUniformIndexingEXT
// CHECK: OpCapability StorageTexelBufferArrayNonUniformIndexingEXT
// CHECK: OpCapability InputAttachmentArrayNonUniformIndexingEXT

// CHECK: OpExtension "SPV_EXT_descriptor_indexing"

// CHECK: OpDecorate [[nu1:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu2:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu3:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu4:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu5:%\d+]] NonUniformEXT

// CHECK: OpDecorate [[nu6:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu7:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu8:%\d+]] NonUniformEXT

// CHECK: OpDecorate [[nu9:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu10:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu11:%\d+]] NonUniformEXT

// CHECK: OpDecorate [[nu12:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu13:%\d+]] NonUniformEXT

// CHECK: OpDecorate [[nu14:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu15:%\d+]] NonUniformEXT

// CHECK: OpDecorate [[nu16:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu17:%\d+]] NonUniformEXT

// CHECK: OpDecorate [[nu18:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu19:%\d+]] NonUniformEXT

// CHECK: OpDecorate [[nu20:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu21:%\d+]] NonUniformEXT

// CHECK: OpDecorate [[nu22:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu23:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu24:%\d+]] NonUniformEXT

// CHECK: OpDecorate [[nu25:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu26:%\d+]] NonUniformEXT

// CHECK: OpDecorate [[nu27:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu28:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu29:%\d+]] NonUniformEXT

// CHECK: OpDecorate [[nu30:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu31:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu32:%\d+]] NonUniformEXT

Texture2D           gTextures[32];
SamplerState        gSamplers[];
RWTexture2D<float4> gRWTextures[32];
Buffer<float4>      gBuffers[];
RWBuffer<uint>      gRWBuffers[32];
SubpassInput        gSubpassInputs[32];

float4 main(uint index : A, float2 loc : B, int2 offset : C) : SV_Target {
// CHECK: [[nu1]] = OpLoad %uint %index
// CHECK: [[nu2]] = OpLoad %type_2d_image
// CHECK: [[nu3]] = OpIAdd %uint {{%\d+}} %uint_1
// CHECK: [[nu4]] = OpLoad %type_sampler
// CHECK: [[nu5]] = OpSampledImage %type_sampled_image
// CHECK:           OpImageSampleImplicitLod
    float4 v1 = gTextures[NonUniformResourceIndex(index)].Sample(
        gSamplers[NonUniformResourceIndex(index + 1)], loc);

// CHECK: [[nu6]] = OpLoad %uint %index
// CHECK:           OpIAdd %uint {{%\d+}} %uint_1
// CHECK: [[nu7]] = OpLoad %type_sampler
// CHECK: [[nu8]] = OpSampledImage %type_sampled_image
// CHECK:           OpImageSampleImplicitLod
    float4 v2 = gTextures[0].Sample(
        gSamplers[NonUniformResourceIndex(index++)], loc);

// CHECK:            OpLoad %uint %index
// CHECK:            OpISub %uint {{%\d+}} %uint_1
// CHECK:  [[nu9]] = OpLoad %uint %index
// CHECK: [[nu10]] = OpLoad %type_2d_image
// CHECK: [[nu11]] = OpSampledImage %type_sampled_image
// CHECK:            OpImageSampleImplicitLod
    float4 v3 = gTextures[NonUniformResourceIndex(--index)].Sample(
        gSamplers[0], loc);

// CHECK: [[nu12]] = OpIMul %uint
// CHECK: [[nu13]] = OpLoad %type_2d_image_0
// CHECK:            OpImageRead
    float4 v4 = gRWTextures[NonUniformResourceIndex(index * index)].Load(loc);

// CHECK: [[nu14]] = OpLoad %uint %index
// CHECK:            OpUMod %uint {{%\d+}} %uint_3
// CHECK: [[nu15]] = OpLoad %type_2d_image_0
// CHECK:            OpImageWrite
    gRWTextures[NonUniformResourceIndex(index) % 3][loc] = 4;

// CHECK: [[nu16]] = OpLoad %uint %index
// CHECK:            OpLoad %uint %index
// CHECK: [[nu17]] = OpLoad %type_buffer_image
// CHECK:            OpImageFetch
    float4 v5 = gBuffers[NonUniformResourceIndex(index) * index][5];

// CHECK: [[nu18]] = OpLoad %uint %index
// CHECK: [[nu19]] = OpLoad %type_buffer_image_0
// CHECK:            OpImageRead
    float4 v6 = gRWBuffers[NonUniformResourceIndex(index)].Load(6);

// CHECK: [[nu20]] = OpLoad %uint %index
// CHECK: [[nu21]] = OpLoad %type_buffer_image_0
// CHECK:            OpImageWrite
    gRWBuffers[NonUniformResourceIndex(index)][8] = 9;

// CHECK: [[nu22]] = OpLoad %uint %index
// CHECK: [[nu23]] = OpLoad %uint %index
// CHECK: [[nu24]] = OpImageTexelPointer %_ptr_Image_uint {{%\d+}} %uint_10 %uint_0
// CHECK:            OpAtomicIAdd
    uint old = 0;
    InterlockedAdd(gRWBuffers[NonUniformResourceIndex(index) * NonUniformResourceIndex(index)][10], 1, old);

// CHECK: [[nu25]] = OpLoad %uint %index
// CHECK: [[nu26]] = OpLoad %type_subpass_image
// CHECK:            OpImageRead
    float4 v7 = gSubpassInputs[NonUniformResourceIndex(index)].SubpassLoad();

// CHECK: [[nu27]] = OpLoad %uint %index
// CHECK: [[nu28]] = OpLoad %type_2d_image
// CHECK: [[nu29]] = OpSampledImage %type_sampled_image
// CHECK:            OpImageGather
    float4 v8 = gTextures[NonUniformResourceIndex(index)].Gather(gSamplers[0], loc, offset);

// CHECK: [[nu30]] = OpLoad %uint %index
// CHECK: [[nu31]] = OpLoad %type_2d_image
// CHECK: [[nu32]] = OpSampledImage %type_sampled_image
// CHECK:            OpImageQueryLod
    float  v9 = gTextures[NonUniformResourceIndex(index)].CalculateLevelOfDetail(gSamplers[0], 0.5);

    return v1 + v2 + v3 + v4 + v5 + v6 + v7;
}
