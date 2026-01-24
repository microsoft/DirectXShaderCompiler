// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s


vk::SampledTexture2D<float4> tex2D_F4 : register(t1);

// CHECK: OpCapability SparseResidency

// CHECK: [[v2ic:%[0-9]+]] = OpConstantComposite %v2int %int_1 %int_2

// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %v4float

float4 main(int3 location: A) : SV_Target {
    uint status;

// CHECK:        [[loc:%[0-9]+]] = OpLoad %v3int %location
// CHECK-NEXT: [[coord_0:%[0-9]+]] = OpVectorShuffle %v2int [[loc]] [[loc]] 0 1
// CHECK-NEXT:   [[lod_0:%[0-9]+]] = OpCompositeExtract %int [[loc]] 2
// CHECK-NEXT:    [[tex:%[0-9]+]] = OpLoad %type_sampled_image %tex2D_F4
// CHECK-NEXT:    [[tex_img:%[0-9]+]] = OpImage %type_2d_image [[tex]]
// CHECK-NEXT:       {{%[0-9]+}} = OpImageFetch %v4float [[tex_img]] [[coord_0]] Lod [[lod_0]]
    float4 val1 = tex2D_F4.Load(location);

// CHECK:        [[loc:%[0-9]+]] = OpLoad %v3int %location
// CHECK-NEXT: [[coord_0:%[0-9]+]] = OpVectorShuffle %v2int [[loc]] [[loc]] 0 1
// CHECK-NEXT:   [[lod_0:%[0-9]+]] = OpCompositeExtract %int [[loc]] 2
// CHECK-NEXT:    [[tex:%[0-9]+]] = OpLoad %type_sampled_image %tex2D_F4
// CHECK-NEXT:    [[tex_img:%[0-9]+]] = OpImage %type_2d_image [[tex]]
// CHECK-NEXT:       {{%[0-9]+}} = OpImageFetch %v4float [[tex_img]] [[coord_0]] Lod|ConstOffset [[lod_0]] [[v2ic]]
    float4 val2 = tex2D_F4.Load(location, int2(1, 2));

/////////////////////////////////
/// Using the Status argument ///
/////////////////////////////////

// CHECK:        [[loc:%[0-9]+]] = OpLoad %v3int %location
// CHECK-NEXT: [[coord_0:%[0-9]+]] = OpVectorShuffle %v2int [[loc]] [[loc]] 0 1
// CHECK-NEXT:   [[lod_0:%[0-9]+]] = OpCompositeExtract %int [[loc]] 2
// CHECK-NEXT:    [[tex:%[0-9]+]] = OpLoad %type_sampled_image %tex2D_F4
// CHECK-NEXT:    [[tex_img:%[0-9]+]] = OpImage %type_2d_image [[tex]]
// CHECK-NEXT:[[structResult:%[0-9]+]] = OpImageSparseFetch %SparseResidencyStruct [[tex_img]] [[coord_0]] Lod|ConstOffset [[lod_0]] [[v2ic]]
// CHECK-NEXT:      [[status:%[0-9]+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                        OpStore %status [[status]]
// CHECK-NEXT:    [[v4result:%[0-9]+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:                        OpStore %val3 [[v4result]]
    float4  val3 = tex2D_F4.Load(location, int2(1, 2), status);

    return 1.0;
}
