// Run: %dxc -T ps_6_0 -E main

Texture1D <float4> t1 : register(t1);
Texture2D <float4> t2 : register(t2);
Texture3D <float4> t3 : register(t3);
// .Load() does not support TextureCube.

// CHECK: OpCapability ImageGatherExtended

// CHECK: [[v2ic:%\d+]] = OpConstantComposite %v2int %int_1 %int_2
// CHECK: [[v4ic:%\d+]] = OpConstantComposite %v4int %int_1 %int_2 %int_3 %int_4
// CHECK: [[v3ic:%\d+]] = OpConstantComposite %v3int %int_3 %int_3 %int_3

float4 main(int3 location: A, int offset: B) : SV_Target {

// CHECK:         [[t1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT: [[coord:%\d+]] = OpCompositeExtract %int [[v2ic]] 0
// CHECK-NEXT:   [[lod:%\d+]] = OpCompositeExtract %int [[v2ic]] 1
// CHECK-NEXT:[[offset:%\d+]] = OpLoad %int %offset
// CHECK-NEXT:       {{%\d+}} = OpImageFetch %v4float [[t1]] [[coord]] Lod|Offset [[lod]] [[offset]]
    float4 val1 = t1.Load(int2(1, 2), offset);

// CHECK:         [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:   [[loc:%\d+]] = OpLoad %v3int %location
// CHECK-NEXT: [[coord:%\d+]] = OpVectorShuffle %v2int [[loc]] [[loc]] 0 1
// CHECK-NEXT:   [[lod:%\d+]] = OpCompositeExtract %int [[loc]] 2
// CHECK-NEXT:       {{%\d+}} = OpImageFetch %v4float [[t2]] [[coord]] Lod|ConstOffset [[lod]] [[v2ic]]
    float4 val2 = t2.Load(location, int2(1, 2));

// CHECK:         [[t3:%\d+]] = OpLoad %type_3d_image %t3
// CHECK-NEXT: [[coord:%\d+]] = OpVectorShuffle %v3int [[v4ic]] [[v4ic]] 0 1 2
// CHECK-NEXT:   [[lod:%\d+]] = OpCompositeExtract %int [[v4ic]] 3
// CHECK-NEXT:       {{%\d+}} = OpImageFetch %v4float [[t3]] [[coord]] Lod|ConstOffset [[lod]] [[v3ic]]
    float4 val3 = t3.Load(int4(1, 2, 3, 4), 3);

    return 1.0;
}
