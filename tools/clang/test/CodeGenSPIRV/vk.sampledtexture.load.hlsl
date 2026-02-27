// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// CHECK: [[v2ic:%[0-9]+]] = OpConstantComposite %v2int %int_1 %int_2

// CHECK: [[type_2d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_2d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image]]
// CHECK: [[type_2d_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 1 0 1 Unknown
// CHECK: [[type_2d_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_array]]
// CHECK: [[type_2d_image_ms:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 1 1 Unknown
// CHECK: [[type_2d_sampled_image_ms:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_ms]]
// CHECK: [[type_2d_image_ms_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 1 1 1 Unknown
// CHECK: [[type_2d_sampled_image_ms_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_ms_array]]

vk::SampledTexture2D<float4> tex2d;
vk::SampledTexture2DArray<float4> tex2dArray;
vk::SampledTexture2DMS<float4> tex2dMS;
vk::SampledTexture2DMSArray<float4> tex2dMSArray;

float4 main(int3 location3: A, int4 location4: B) : SV_Target {
    uint status;

// CHECK:        [[loc:%[0-9]+]] = OpLoad %v3int %location3
// CHECK-NEXT: [[coord_0:%[0-9]+]] = OpVectorShuffle %v2int [[loc]] [[loc]] 0 1
// CHECK-NEXT:   [[lod_0:%[0-9]+]] = OpCompositeExtract %int [[loc]] 2
// CHECK-NEXT:    [[tex:%[0-9]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK-NEXT:    [[tex_img:%[0-9]+]] = OpImage %type_2d_image [[tex]]
// CHECK-NEXT:       {{%[0-9]+}} = OpImageFetch %v4float [[tex_img]] [[coord_0]] Lod [[lod_0]]
    float4 val1 = tex2d.Load(location3);

// CHECK:        [[loc:%[0-9]+]] = OpLoad %v3int %location3
// CHECK-NEXT: [[coord_0:%[0-9]+]] = OpVectorShuffle %v2int [[loc]] [[loc]] 0 1
// CHECK-NEXT:   [[lod_0:%[0-9]+]] = OpCompositeExtract %int [[loc]] 2
// CHECK-NEXT:    [[tex:%[0-9]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK-NEXT:    [[tex_img:%[0-9]+]] = OpImage %type_2d_image [[tex]]
// CHECK-NEXT:       {{%[0-9]+}} = OpImageFetch %v4float [[tex_img]] [[coord_0]] Lod|ConstOffset [[lod_0]] [[v2ic]]
    float4 val2 = tex2d.Load(location3, int2(1, 2));

/////////////////////////////////
/// Using the Status argument ///
/////////////////////////////////

// CHECK:        [[loc:%[0-9]+]] = OpLoad %v3int %location3
// CHECK-NEXT: [[coord_0:%[0-9]+]] = OpVectorShuffle %v2int [[loc]] [[loc]] 0 1
// CHECK-NEXT:   [[lod_0:%[0-9]+]] = OpCompositeExtract %int [[loc]] 2
// CHECK-NEXT:    [[tex:%[0-9]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK-NEXT:    [[tex_img:%[0-9]+]] = OpImage %type_2d_image [[tex]]
// CHECK-NEXT:[[structResult:%[0-9]+]] = OpImageSparseFetch %SparseResidencyStruct [[tex_img]] [[coord_0]] Lod|ConstOffset [[lod_0]] [[v2ic]]
// CHECK-NEXT:      [[status:%[0-9]+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                        OpStore %status [[status]]
// CHECK-NEXT:    [[v4result:%[0-9]+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:                        OpStore %val3 [[v4result]]
    float4  val3 = tex2d.Load(location3, int2(1, 2), status);

// CHECK:        [[loc:%[0-9]+]] = OpLoad %v4int %location4
// CHECK-NEXT: [[coord_0:%[0-9]+]] = OpVectorShuffle %v3int [[loc]] [[loc]] 0 1 2
// CHECK-NEXT:   [[lod_0:%[0-9]+]] = OpCompositeExtract %int [[loc]] 3
// CHECK-NEXT:    [[tex:%[0-9]+]] = OpLoad [[type_2d_sampled_image_array]] %tex2dArray
// CHECK-NEXT:    [[tex_img:%[0-9]+]] = OpImage [[type_2d_image_array]] [[tex]]
// CHECK-NEXT:       {{%[0-9]+}} = OpImageFetch %v4float [[tex_img]] [[coord_0]] Lod [[lod_0]]
    float4 val4 = tex2dArray.Load(location4);

// CHECK:        [[loc_ms:%[0-9]+]] = OpLoad %v3int %location3
// CHECK-NEXT:[[coord_ms:%[0-9]+]] = OpVectorShuffle %v2int [[loc_ms]] [[loc_ms]] 0 1
// CHECK-NEXT:[[sample_ms_ptr:%[0-9]+]] = OpAccessChain %_ptr_Function_int %location3 %int_2
// CHECK-NEXT:    [[sample_ms:%[0-9]+]] = OpLoad %int [[sample_ms_ptr]]
// CHECK-NEXT:    [[tex_ms:%[0-9]+]] = OpLoad [[type_2d_sampled_image_ms]] %tex2dMS
// CHECK-NEXT:[[tex_ms_img:%[0-9]+]] = OpImage [[type_2d_image_ms]] [[tex_ms]]
// CHECK-NEXT:       {{%[0-9]+}} = OpImageFetch %v4float [[tex_ms_img]] [[coord_ms]] Sample [[sample_ms]]
    float4 val5 = tex2dMS.Load(location3.xy, location3.z);

// CHECK:         [[loc_msa:%[0-9]+]] = OpLoad %v4int %location4
// CHECK-NEXT:[[coord_msa:%[0-9]+]] = OpVectorShuffle %v3int [[loc_msa]] [[loc_msa]] 0 1 2
// CHECK-NEXT:[[sample_msa_ptr:%[0-9]+]] = OpAccessChain %_ptr_Function_int %location4 %int_3
// CHECK-NEXT:    [[sample_msa:%[0-9]+]] = OpLoad %int [[sample_msa_ptr]]
// CHECK-NEXT:   [[tex_msa:%[0-9]+]] = OpLoad [[type_2d_sampled_image_ms_array]] %tex2dMSArray
// CHECK-NEXT:[[tex_msa_img:%[0-9]+]] = OpImage [[type_2d_image_ms_array]] [[tex_msa]]
// CHECK-NEXT:       {{%[0-9]+}} = OpImageFetch %v4float [[tex_msa_img]] [[coord_msa]] Sample [[sample_msa]]
    float4 val6 = tex2dMSArray.Load(location4.xyz, location4.w);

    return 1.0;
}
