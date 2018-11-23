// Run: %dxc -T ps_6_0 -E main

Texture1D       <float4> t1 : register(t1);
Texture2D       <float4> t2 : register(t2);
Texture3D       <float4> t3 : register(t3);
// .Load() does not support TextureCube.

Texture1D        <float> t4 : register(t4);
Texture2D        <int2>  t5 : register(t5);
Texture3D        <uint3> t6 : register(t6);

Texture2DMS     <float>  t7 : register(t7);
Texture2DMSArray<float3> t8 : register(t8);

// CHECK: OpCapability ImageGatherExtended
// CHECK: OpCapability SparseResidency

// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %v4float
// CHECK: %SparseResidencyStruct_0 = OpTypeStruct %uint %v4int
// CHECK: %SparseResidencyStruct_1 = OpTypeStruct %uint %v4uint

// CHECK: [[v2ic:%\d+]] = OpConstantComposite %v2int %int_1 %int_2
// CHECK: [[v4ic:%\d+]] = OpConstantComposite %v4int %int_1 %int_2 %int_3 %int_4
// CHECK: [[v3ic:%\d+]] = OpConstantComposite %v3int %int_3 %int_3 %int_3

float4 main(int3 location: A, int offset: B) : SV_Target {
    uint status;

// CHECK:      [[coord:%\d+]] = OpCompositeExtract %int [[v2ic]] 0
// CHECK-NEXT:   [[lod:%\d+]] = OpCompositeExtract %int [[v2ic]] 1
// CHECK-NEXT:[[offset:%\d+]] = OpLoad %int %offset
// CHECK-NEXT:    [[t1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:       {{%\d+}} = OpImageFetch %v4float [[t1]] [[coord]] Lod|Offset [[lod]] [[offset]]
    float4 val1 = t1.Load(int2(1, 2), offset);

// CHECK:        [[loc:%\d+]] = OpLoad %v3int %location
// CHECK-NEXT: [[coord:%\d+]] = OpVectorShuffle %v2int [[loc]] [[loc]] 0 1
// CHECK-NEXT:   [[lod:%\d+]] = OpCompositeExtract %int [[loc]] 2
// CHECK-NEXT:    [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:       {{%\d+}} = OpImageFetch %v4float [[t2]] [[coord]] Lod|ConstOffset [[lod]] [[v2ic]]
    float4 val2 = t2.Load(location, int2(1, 2));

// CHECK:      [[coord:%\d+]] = OpVectorShuffle %v3int [[v4ic]] [[v4ic]] 0 1 2
// CHECK-NEXT:   [[lod:%\d+]] = OpCompositeExtract %int [[v4ic]] 3
// CHECK-NEXT:    [[t3:%\d+]] = OpLoad %type_3d_image %t3
// CHECK-NEXT:       {{%\d+}} = OpImageFetch %v4float [[t3]] [[coord]] Lod|ConstOffset [[lod]] [[v3ic]]
    float4 val3 = t3.Load(int4(1, 2, 3, 4), 3);

// CHECK:      [[f4:%\d+]] = OpImageFetch %v4float {{%\d+}} {{%\d+}} Lod|Offset {{%\d+}} {{%\d+}}
// CHECK-NEXT:    {{%\d+}} = OpCompositeExtract %float [[f4]] 0
    float val4 = t4.Load(int2(1,2), offset);

// CHECK:      [[f5:%\d+]] = OpImageFetch %v4int {{%\d+}} {{%\d+}} Lod|ConstOffset {{%\d+}} {{%\d+}}
// CHECK-NEXT:    {{%\d+}} = OpVectorShuffle %v2int [[f5]] [[f5]] 0 1
    int2  val5 = t5.Load(location, int2(1,2));

// CHECK:      [[f6:%\d+]] = OpImageFetch %v4uint {{%\d+}} {{%\d+}} Lod|ConstOffset {{%\d+}} {{%\d+}}
// CHECK-NEXT:    {{%\d+}} = OpVectorShuffle %v3uint [[f6]] [[f6]] 0 1 2
    uint3 val6 = t6.Load(int4(1, 2, 3, 4), 3);

    float val7;
    float3 val8;
    int sampleIndex = 7;
    int2 pos2 = int2(2, 3);
    int3 pos3 = int3(2, 3, 4);
    int2 offset2 = int2(1, 2);

// CHECK:     [[pos0:%\d+]] = OpLoad %v2int %pos2
// CHECK-NEXT: [[si0:%\d+]] = OpLoad %int %sampleIndex
// CHECK-NEXT: [[t70:%\d+]] = OpLoad %type_2d_image_1 %t7
// CHECK-NEXT: [[f70:%\d+]] = OpImageFetch %v4float [[t70]] [[pos0]] Sample [[si0]]
// CHECK-NEXT:     {{%\d+}} = OpCompositeExtract %float [[f70]] 0
    val7 = t7.Load(pos2, sampleIndex);

// CHECK:        [[pos1:%\d+]] = OpLoad %v2int %pos2
// CHECK-NEXT:    [[si1:%\d+]] = OpLoad %int %sampleIndex
// CHECK-NEXT:[[offset2:%\d+]] = OpLoad %v2int %offset2
// CHECK-NEXT:    [[t71:%\d+]] = OpLoad %type_2d_image_1 %t7
// CHECK-NEXT:    [[f71:%\d+]] = OpImageFetch %v4float [[t71]] [[pos1]] Offset|Sample [[offset2]] [[si1]]
// CHECK-NEXT:        {{%\d+}} = OpCompositeExtract %float [[f71]] 0
    val7 = t7.Load(pos2, sampleIndex, offset2);

// CHECK:     [[pos2:%\d+]] = OpLoad %v3int %pos3
// CHECK-NEXT: [[si2:%\d+]] = OpLoad %int %sampleIndex
// CHECK-NEXT: [[t80:%\d+]] = OpLoad %type_2d_image_array %t8
// CHECK-NEXT: [[f80:%\d+]] = OpImageFetch %v4float [[t80]] [[pos2]] Sample [[si2]]
// CHECK-NEXT:     {{%\d+}} = OpVectorShuffle %v3float [[f80]] [[f80]] 0 1 2
    val8 = t8.Load(pos3, sampleIndex);

// CHECK:     [[pos3:%\d+]] = OpLoad %v3int %pos3
// CHECK-NEXT: [[si3:%\d+]] = OpLoad %int %sampleIndex
// CHECK-NEXT: [[t81:%\d+]] = OpLoad %type_2d_image_array %t8
// CHECK-NEXT: [[f81:%\d+]] = OpImageFetch %v4float [[t81]] [[pos3]] ConstOffset|Sample [[v2ic]] [[si3]]
// CHECK-NEXT:     {{%\d+}} = OpVectorShuffle %v3float [[f81]] [[f81]] 0 1 2
    val8 = t8.Load(pos3, sampleIndex, int2(1,2));

/////////////////////////////////
/// Using the Status argument ///
/////////////////////////////////

// CHECK:            [[coord:%\d+]] = OpCompositeExtract %int [[v2ic]] 0
// CHECK-NEXT:         [[lod:%\d+]] = OpCompositeExtract %int [[v2ic]] 1
// CHECK-NEXT:      [[offset:%\d+]] = OpLoad %int %offset
// CHECK-NEXT:          [[t4:%\d+]] = OpLoad %type_1d_image %t4
// CHECK-NEXT:[[structResult:%\d+]] = OpImageSparseFetch %SparseResidencyStruct [[t4]] [[coord]] Lod|Offset [[lod]] [[offset]]
// CHECK-NEXT:      [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                        OpStore %status [[status]]
// CHECK-NEXT:    [[v4result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:      [[result:%\d+]] = OpCompositeExtract %float [[v4result]] 0
// CHECK-NEXT:                        OpStore %val14 [[result]]
    float  val14 = t4.Load(int2(1,2), offset, status);

// CHECK:              [[loc:%\d+]] = OpLoad %v3int %location
// CHECK-NEXT:       [[coord:%\d+]] = OpVectorShuffle %v2int [[loc]] [[loc]] 0 1
// CHECK-NEXT:         [[lod:%\d+]] = OpCompositeExtract %int [[loc]] 2
// CHECK-NEXT:          [[t5:%\d+]] = OpLoad %type_2d_image_0 %t5
// CHECK-NEXT:[[structResult:%\d+]] = OpImageSparseFetch %SparseResidencyStruct_0 [[t5]] [[coord]] Lod|ConstOffset [[lod]] [[v2ic]]
// CHECK-NEXT:      [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                        OpStore %status [[status]]
// CHECK-NEXT:    [[v4result:%\d+]] = OpCompositeExtract %v4int [[structResult]] 1
// CHECK-NEXT:      [[result:%\d+]] = OpVectorShuffle %v2int [[v4result]] [[v4result]] 0 1
// CHECK-NEXT:                        OpStore %val15 [[result]]
    int2   val15 = t5.Load(location, int2(1,2), status);

// CHECK:            [[coord:%\d+]] = OpVectorShuffle %v3int [[v4ic]] [[v4ic]] 0 1 2
// CHECK-NEXT:         [[lod:%\d+]] = OpCompositeExtract %int [[v4ic]] 3
// CHECK-NEXT:          [[t6:%\d+]] = OpLoad %type_3d_image_0 %t6
// CHECK-NEXT:[[structResult:%\d+]] = OpImageSparseFetch %SparseResidencyStruct_1 [[t6]] [[coord]] Lod|ConstOffset [[lod]] [[v3ic]]
// CHECK-NEXT:      [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                        OpStore %status [[status]]
// CHECK-NEXT:    [[v4result:%\d+]] = OpCompositeExtract %v4uint [[structResult]] 1
// CHECK-NEXT:      [[result:%\d+]] = OpVectorShuffle %v3uint [[v4result]] [[v4result]] 0 1 2
// CHECK-NEXT:                        OpStore %val16 [[result]]
    uint3  val16 = t6.Load(int4(1, 2, 3, 4), 3, status);

// CHECK:             [[pos1:%\d+]] = OpLoad %v2int %pos2
// CHECK-NEXT:         [[si1:%\d+]] = OpLoad %int %sampleIndex
// CHECK-NEXT:     [[offset2:%\d+]] = OpLoad %v2int %offset2
// CHECK-NEXT:         [[t71:%\d+]] = OpLoad %type_2d_image_1 %t7
// CHECK-NEXT:[[structResult:%\d+]] = OpImageSparseFetch %SparseResidencyStruct [[t71]] [[pos1]] Offset|Sample [[offset2]] [[si1]]
// CHECK-NEXT:      [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                        OpStore %status [[status]]
// CHECK-NEXT:    [[v4result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:      [[result:%\d+]] = OpCompositeExtract %float [[v4result]] 0
// CHECK-NEXT:                        OpStore %val17 [[result]]
    float  val17 = t7.Load(pos2, sampleIndex, offset2, status);

// CHECK:             [[pos3:%\d+]] = OpLoad %v3int %pos3
// CHECK-NEXT:         [[si3:%\d+]] = OpLoad %int %sampleIndex
// CHECK-NEXT:         [[t81:%\d+]] = OpLoad %type_2d_image_array %t8
// CHECK-NEXT:[[structResult:%\d+]] = OpImageSparseFetch %SparseResidencyStruct [[t81]] [[pos3]] ConstOffset|Sample [[v2ic]] [[si3]]
// CHECK-NEXT:      [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                        OpStore %status [[status]]
// CHECK-NEXT:    [[v4result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:      [[result:%\d+]] = OpVectorShuffle %v3float [[v4result]] [[v4result]] 0 1 2
// CHECK-NEXT:                        OpStore %val18 [[result]]
    float3 val18 = t8.Load(pos3, sampleIndex, int2(1,2), status);

    return 1.0;
}
