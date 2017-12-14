// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s5);

// Note: The front end forbids sampling from non-floating-point texture formats.

Texture1D   <float4> t1 : register(t1);
Texture2D   <float4> t2 : register(t2);
Texture3D   <float4> t3 : register(t3);
TextureCube <float4> t4 : register(t4);

// CHECK: OpCapability ImageGatherExtended
// CHECK: OpCapability MinLod
// CHECK: OpCapability SparseResidency

// CHECK: %type_sampled_image = OpTypeSampledImage %type_1d_image
// CHECK: %type_sampled_image_0 = OpTypeSampledImage %type_2d_image
// CHECK: %type_sampled_image_1 = OpTypeSampledImage %type_3d_image
// CHECK: %type_sampled_image_2 = OpTypeSampledImage %type_cube_image

// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %v4float

// CHECK: [[v2f_0_1:%\d+]] = OpConstantComposite %v2float %float_0_1 %float_0_1
// CHECK: [[v2f_0_2:%\d+]] = OpConstantComposite %v2float %float_0_2 %float_0_2
// CHECK: [[v2f_0_3:%\d+]] = OpConstantComposite %v2float %float_0_3 %float_0_3
// CHECK: [[v3f_0_1:%\d+]] = OpConstantComposite %v3float %float_0_1 %float_0_1 %float_0_1
// CHECK: [[v3f_0_2:%\d+]] = OpConstantComposite %v3float %float_0_2 %float_0_2 %float_0_2
// CHECK: [[v3f_0_3:%\d+]] = OpConstantComposite %v3float %float_0_3 %float_0_3 %float_0_3
// CHECK: [[v3i_3:%\d+]] = OpConstantComposite %v3int %int_3 %int_3 %int_3

float4 main(int2 offset : A) : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] %float_0_1 Grad %float_0_2 %float_0_3
    float4 val1 = t1.SampleGrad(gSampler, 0.1, 0.2, 0.3);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v2f_0_1]] Grad|Offset [[v2f_0_2]] [[v2f_0_3]] [[offset]]
    float4 val2 = t2.SampleGrad(gSampler, float2(0.1, 0.1), float2(0.2, 0.2), float2(0.3, 0.3), offset);

// CHECK:              [[t3:%\d+]] = OpLoad %type_3d_image %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v3f_0_1]] Grad|ConstOffset [[v3f_0_2]] [[v3f_0_3]] [[v3i_3]]
    float4 val3 = t3.SampleGrad(gSampler, float3(0.1, 0.1, 0.1), float3(0.2, 0.2, 0.2), float3(0.3, 0.3, 0.3), 3);

// CHECK:              [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_2 [[t4]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v3f_0_1]] Grad [[v3f_0_2]] [[v3f_0_3]]
    float4 val4 = t4.SampleGrad(gSampler, float3(0.1, 0.1, 0.1), float3(0.2, 0.2, 0.2), float3(0.3, 0.3, 0.3));

    float clamp;
// CHECK:           [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:         [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v2f_0_1]] Grad|Offset|MinLod [[v2f_0_2]] [[v2f_0_3]] [[offset]] [[clamp]]
    float4 val5 = t2.SampleGrad(gSampler, float2(0.1, 0.1), float2(0.2, 0.2), float2(0.3, 0.3), offset, clamp);

// CHECK:              [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_2 [[t4]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v3f_0_1]] Grad|MinLod [[v3f_0_2]] [[v3f_0_3]] %float_3_5
    float4 val6 = t4.SampleGrad(gSampler, float3(0.1, 0.1, 0.1), float3(0.2, 0.2, 0.2), float3(0.3, 0.3, 0.3), /*clamp*/3.5);

    uint status;
// CHECK:             [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:           [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:       [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleExplicitLod %SparseResidencyStruct [[sampledImg]] [[v2f_0_1]] Grad|Offset|MinLod [[v2f_0_2]] [[v2f_0_3]] [[offset]] [[clamp]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val7 [[result]]
    float4 val7 = t2.SampleGrad(gSampler, float2(0.1, 0.1), float2(0.2, 0.2), float2(0.3, 0.3), offset, clamp, status);

// CHECK:                [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_2 [[t4]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleExplicitLod %SparseResidencyStruct [[sampledImg]] [[v3f_0_1]] Grad|MinLod [[v3f_0_2]] [[v3f_0_3]] %float_3_5
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val8 [[result]]
    float4 val8 = t4.SampleGrad(gSampler, float3(0.1, 0.1, 0.1), float3(0.2, 0.2, 0.2), float3(0.3, 0.3, 0.3), /*clamp*/3.5, status);

    return 1.0;
}
