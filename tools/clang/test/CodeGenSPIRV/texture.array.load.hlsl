// Run: %dxc -T ps_6_0 -E main

Texture1DArray <float4> t1 : register(t1);
Texture2DArray <float4> t2 : register(t2);
// .Load() does not support TextureCubeArray.

// CHECK: [[v3ic:%\d+]] = OpConstantComposite %v3int %int_1 %int_2 %int_3

float4 main(int4 location: A) : SV_Target {

// CHECK:         [[t1:%\d+]] = OpLoad %type_1d_image_array %t1
// CHECK-NEXT: [[coord:%\d+]] = OpVectorShuffle %v2int [[v3ic]] [[v3ic]] 0 1
// CHECK-NEXT:   [[lod:%\d+]] = OpCompositeExtract %int [[v3ic]] 2
// CHECK-NEXT:       {{%\d+}} = OpImageFetch %v4float [[t1]] [[coord]] Lod|ConstOffset [[lod]] %int_10
    float4 val1 = t1.Load(int3(1, 2, 3), 10);

// CHECK:         [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:   [[loc:%\d+]] = OpLoad %v4int %location
// CHECK-NEXT: [[coord:%\d+]] = OpVectorShuffle %v3int [[loc]] [[loc]] 0 1 2
// CHECK-NEXT:   [[lod:%\d+]] = OpCompositeExtract %int [[loc]] 3
// CHECK-NEXT:       {{%\d+}} = OpImageFetch %v4float [[t2]] [[coord]] Lod [[lod]]
    float4 val2 = t2.Load(location);

    return 1.0;
}




