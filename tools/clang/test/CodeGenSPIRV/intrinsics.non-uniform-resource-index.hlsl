// Run: %dxc -T ps_6_0 -E main

// CHECK: OpCapability RuntimeDescriptorArrayEXT
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
// CHECK: OpDecorate [[nu33:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu34:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu35:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu36:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu37:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu38:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu39:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu40:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu41:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu42:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu43:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu44:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu45:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu46:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu47:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu48:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu49:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu50:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu51:%\d+]] NonUniformEXT
// CHECK: OpDecorate [[nu52:%\d+]] NonUniformEXT

Texture2D           gTextures[32];
SamplerState        gSamplers[];
RWTexture2D<float4> gRWTextures[32];
Buffer<float4>      gBuffers[];
RWBuffer<uint>      gRWBuffers[32];
SubpassInput        gSubpassInputs[32];

float4 main(uint index : A, float2 loc : B, int2 offset : C) : SV_Target {
// CHECK: [[nu1]] = OpLoad %uint %index
// CHECK: [[nu2]] = OpAccessChain %_ptr_UniformConstant_type_2d_image %gTextures [[nu1]]
// CHECK: [[nu3]] = OpLoad %type_2d_image
// CHECK: [[nu4]] = OpIAdd %uint {{%\d+}} %uint_1
// CHECK: [[nu5]] = OpAccessChain %_ptr_UniformConstant_type_sampler %gSamplers [[nu4]]
// CHECK: [[nu6]] = OpLoad %type_sampler
// CHECK: [[nu7]] = OpSampledImage %type_sampled_image
// CHECK:           OpImageSampleImplicitLod
    float4 v1 = gTextures[NonUniformResourceIndex(index)].Sample(
        gSamplers[NonUniformResourceIndex(index + 1)], loc);

// CHECK:  [[nu8]] = OpLoad %uint %index
// CHECK:            OpIAdd %uint {{%\d+}} %uint_1
// CHECK:  [[nu9]] = OpAccessChain %_ptr_UniformConstant_type_sampler %gSamplers [[nu8]]
// CHECK: [[nu10]] = OpLoad %type_sampler
// CHECK: [[nu11]] = OpSampledImage %type_sampled_image
// CHECK:            OpImageSampleImplicitLod
    float4 v2 = gTextures[0].Sample(
        gSamplers[NonUniformResourceIndex(index++)], loc);

// CHECK:            OpLoad %uint %index
// CHECK:            OpISub %uint {{%\d+}} %uint_1
// CHECK: [[nu12]] = OpLoad %uint %index
// CHECK: [[nu13]] = OpAccessChain %_ptr_UniformConstant_type_2d_image %gTextures [[nu12]]
// CHECK: [[nu14]] = OpLoad %type_2d_image
// CHECK: [[nu15]] = OpSampledImage %type_sampled_image
// CHECK:            OpImageSampleImplicitLod
    float4 v3 = gTextures[NonUniformResourceIndex(--index)].Sample(
        gSamplers[0], loc);

// CHECK: [[nu16]] = OpIMul %uint
// CHECK: [[nu17]] = OpAccessChain %_ptr_UniformConstant_type_2d_image_0 %gRWTextures [[nu16]]
// CHECK: [[nu18]] = OpLoad %type_2d_image_0
// CHECK:            OpImageRead
    float4 v4 = gRWTextures[NonUniformResourceIndex(index * index)].Load(loc);

// CHECK: [[nu19]] = OpLoad %uint %index
// CHECK: [[nu20]] = OpUMod %uint [[nu19]] %uint_3
// CHECK: [[nu21]] = OpAccessChain %_ptr_UniformConstant_type_2d_image_0 %gRWTextures [[nu20]]
// CHECK: [[nu22]] = OpLoad %type_2d_image_0
// CHECK:            OpImageWrite
    gRWTextures[NonUniformResourceIndex(index) % 3][loc] = 4;

// CHECK: [[nu23]] = OpLoad %uint %index
// CHECK:            OpLoad %uint %index
// CHECK: [[nu24]] = OpIMul %uint [[nu23]] {{%\d+}}
// CHECK: [[nu25]] = OpAccessChain %_ptr_UniformConstant_type_buffer_image %gBuffers [[nu24]]
// CHECK: [[nu26]] = OpLoad %type_buffer_image
// CHECK:            OpImageFetch
    float4 v5 = gBuffers[NonUniformResourceIndex(index) * index][5];

// CHECK: [[nu27]] = OpLoad %uint %index
// CHECK: [[nu28]] = OpAccessChain %_ptr_UniformConstant_type_buffer_image_0 %gRWBuffers [[nu27]]
// CHECK: [[nu29]] = OpLoad %type_buffer_image_0
// CHECK:            OpImageRead
    float4 v6 = gRWBuffers[NonUniformResourceIndex(index)].Load(6);

// CHECK: [[nu30]] = OpLoad %uint %index
// CHECK: [[nu31]] = OpAccessChain %_ptr_UniformConstant_type_buffer_image_0 %gRWBuffers [[nu30]]
// CHECK: [[nu32]] = OpLoad %type_buffer_image_0
// CHECK:            OpImageWrite
    gRWBuffers[NonUniformResourceIndex(index)][8] = 9;

// CHECK: [[nu33]] = OpLoad %uint %index
// CHECK: [[nu34]] = OpLoad %uint %index
// CHECK: [[nu35]] = OpIMul %uint [[nu33]] [[nu34]]
// CHECK: [[nu36]] = OpAccessChain %_ptr_UniformConstant_type_buffer_image_0 %gRWBuffers [[nu35]]
// CHECK: [[nu37]] = OpImageTexelPointer %_ptr_Image_uint {{%\d+}} %uint_10 %uint_0
// CHECK:            OpAtomicIAdd
    uint old = 0;
    InterlockedAdd(gRWBuffers[NonUniformResourceIndex(index) * NonUniformResourceIndex(index)][10], 1, old);

// CHECK: [[nu38]] = OpLoad %uint %index
// CHECK: [[nu39]] = OpAccessChain %_ptr_UniformConstant_type_subpass_image %gSubpassInputs [[nu38]]
// CHECK: [[nu40]] = OpLoad %type_subpass_image
// CHECK:            OpImageRead
    float4 v7 = gSubpassInputs[NonUniformResourceIndex(index)].SubpassLoad();

// CHECK: [[nu41]] = OpLoad %uint %index
// CHECK: [[nu42]] = OpAccessChain %_ptr_UniformConstant_type_2d_image %gTextures [[nu41]]
// CHECK: [[nu43]] = OpLoad %type_2d_image
// CHECK: [[nu44]] = OpSampledImage %type_sampled_image
// CHECK:            OpImageGather
    float4 v8 = gTextures[NonUniformResourceIndex(index)].Gather(gSamplers[0], loc, offset);

// CHECK: [[nu45]] = OpLoad %uint %index
// CHECK: [[nu46]] = OpAccessChain %_ptr_UniformConstant_type_2d_image %gTextures [[nu45]]
// CHECK: [[nu47]] = OpLoad %type_2d_image
// CHECK: [[nu48]] = OpSampledImage %type_sampled_image
// CHECK:            OpImageQueryLod
// CHECK: [[nu49]] = OpLoad %uint %index
// CHECK: [[nu50]] = OpAccessChain %_ptr_UniformConstant_type_2d_image %gTextures [[nu49]]
// CHECK: [[nu51]] = OpLoad %type_2d_image
// CHECK: [[nu52]] = OpSampledImage %type_sampled_image
// CHECK:            OpImageQueryLod
    float  v9 = gTextures[NonUniformResourceIndex(index)].CalculateLevelOfDetail(gSamplers[0], 0.5);

    return v1 + v2 + v3 + v4 + v5 + v6 + v7;
}
