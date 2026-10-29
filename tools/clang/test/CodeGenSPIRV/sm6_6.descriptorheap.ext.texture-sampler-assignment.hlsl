// RUN: %dxc -T ps_6_6 -E PSMain -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s | FileCheck %s

// Verifies: reassigning both a Texture2D and a SamplerState across a 
//  dynamic index, a +2 index, and inside a conditional branch reloads 
//  the latest texture+sampler from BOTH the resource and sampler 
//  heaps per reassignment, producing a fresh 
//  OpSampledImage/OpImageSampleImplicitLod each time.

// CHECK-DAG:       %[[UntypedPtr:[a-zA-Z0-9_]+]] = OpTypeUntypedPointerKHR UniformConstant
// CHECK-DAG:            %[[Tex2D:[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 2 0 0 1 Unknown
// CHECK-DAG:          %[[Sampler:[a-zA-Z0-9_]+]] = OpTypeSampler
// CHECK-DAG:         %[[TexArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Tex2D]]
// CHECK-DAG:     %[[SamplerArray:[a-zA-Z0-9_]+]] = OpTypeRuntimeArray %[[Sampler]]
// CHECK-DAG:     %[[SampledImage:[a-zA-Z0-9_]+]] = OpTypeSampledImage %[[Tex2D]]

// CHECK:         %[[ResourceHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant
// CHECK:          %[[SamplerHeap:[a-zA-Z0-9_]+]] = OpUntypedVariableKHR %[[UntypedPtr]] UniformConstant

struct PSInput
{
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

float4 PSMain(PSInput input) : SV_Target0
{
    uint texIdx = uint(input.color.x);
    uint sampIdx = uint(input.color.y);
    uint cond = uint(input.color.z);

    Texture2D<float4> myTexture = ResourceDescriptorHeap[texIdx];
    SamplerState samp = SamplerDescriptorHeap[sampIdx];

    // CHECK:           %[[TexIdx:[a-zA-Z0-9_]+]] = OpConvertFToU %uint
    // CHECK:          %[[SampIdx:[a-zA-Z0-9_]+]] = OpConvertFToU %uint
    // CHECK:             %[[Cond:[a-zA-Z0-9_]+]] = OpConvertFToU %uint
    // CHECK:         %[[TexDesc0:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[TexArray]] %[[ResourceHeap]] %[[TexIdx]]
    // CHECK:             %[[Tex0:[a-zA-Z0-9_]+]] = OpLoad %[[Tex2D]] %[[TexDesc0]]
    // CHECK:        %[[SampDesc0:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[SamplerArray]] %[[SamplerHeap]] %[[SampIdx]]
    // CHECK:            %[[Samp0:[a-zA-Z0-9_]+]] = OpLoad %[[Sampler]] %[[SampDesc0]]
    // CHECK:         %[[Sampled0:[a-zA-Z0-9_]+]] = OpSampledImage %[[SampledImage]] %[[Tex0]] %[[Samp0]]
    // CHECK:           %[[Color0:[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float %[[Sampled0]]
    float4 color = myTexture.Sample(samp, float2(1,1));

    myTexture = ResourceDescriptorHeap[texIdx + 2];
    samp = SamplerDescriptorHeap[sampIdx];

    // CHECK:      %[[TexIdxPlus2:[a-zA-Z0-9_]+]] = OpIAdd %uint %[[TexIdx]] %uint_2
    // CHECK:         %[[TexDesc1:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[TexArray]] %[[ResourceHeap]] %[[TexIdxPlus2]]
    // CHECK:             %[[Tex1:[a-zA-Z0-9_]+]] = OpLoad %[[Tex2D]] %[[TexDesc1]]
    // CHECK:        %[[SampDesc1:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[SamplerArray]] %[[SamplerHeap]] %[[SampIdx]]
    // CHECK:            %[[Samp1:[a-zA-Z0-9_]+]] = OpLoad %[[Sampler]] %[[SampDesc1]]
    // CHECK:         %[[Sampled1:[a-zA-Z0-9_]+]] = OpSampledImage %[[SampledImage]] %[[Tex1]] %[[Samp1]]
    // CHECK:           %[[Color1:[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float %[[Sampled1]]
    // CHECK:         %[[ColorSum:[a-zA-Z0-9_]+]] = OpFAdd %v4float %[[Color0]] %[[Color1]]
    color += myTexture.Sample(samp, float2(2,2));

    if (cond > 4) {
        myTexture = ResourceDescriptorHeap[texIdx - 2];
        samp = SamplerDescriptorHeap[sampIdx + 2];

        // CHECK:      %[[CondCmp:[a-zA-Z0-9_]+]] = OpUGreaterThan %bool %[[Cond]] %uint_4
        // CHECK:                                   OpBranchConditional %[[CondCmp]]
        // CHECK: %[[TexIdxMinus2:[a-zA-Z0-9_]+]] = OpISub %uint %[[TexIdx]] %uint_2
        // CHECK:     %[[TexDesc2:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[TexArray]] %[[ResourceHeap]] %[[TexIdxMinus2]]
        // CHECK:         %[[Tex2:[a-zA-Z0-9_]+]] = OpLoad %[[Tex2D]] %[[TexDesc2]]
        // CHECK: %[[SampIdxPlus2:[a-zA-Z0-9_]+]] = OpIAdd %uint %[[SampIdx]] %uint_2
        // CHECK:    %[[SampDesc2:[a-zA-Z0-9_]+]] = OpUntypedAccessChainKHR %[[UntypedPtr]] %[[SamplerArray]] %[[SamplerHeap]] %[[SampIdxPlus2]]
        // CHECK:        %[[Samp2:[a-zA-Z0-9_]+]] = OpLoad %[[Sampler]] %[[SampDesc2]]
        // CHECK:     %[[Sampled2:[a-zA-Z0-9_]+]] = OpSampledImage %[[SampledImage]] %[[Tex2]] %[[Samp2]]
        // CHECK:       %[[Color2:[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float %[[Sampled2]]
        // CHECK:                                   OpFAdd %v4float %[[ColorSum]] %[[Color2]]
        color += myTexture.Sample(samp, float2(2,2));
    }

    return color;
}
