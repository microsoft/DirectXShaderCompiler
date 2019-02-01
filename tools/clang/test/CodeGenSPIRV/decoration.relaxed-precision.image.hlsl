// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s5);

// CHECK:               OpDecorate %t2f4 RelaxedPrecision
// CHECK-NEXT:          OpDecorate %t2i4 RelaxedPrecision
// CHECK-NEXT:          OpDecorate [[t2f4_val:%\d+]] RelaxedPrecision
// CHECK-NEXT:          OpDecorate [[image_op_1:%\d+]] RelaxedPrecision
// CHECK-NEXT:          OpDecorate [[t2i4_val:%\d+]] RelaxedPrecision
// CHECK-NEXT:          OpDecorate [[image_op_2:%\d+]] RelaxedPrecision

// CHECK:          %t2f4 = OpVariable %_ptr_UniformConstant_type_2d_image_array UniformConstant
// CHECK:          %t2i4 = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
Texture2DArray<min16float4> t2f4   : register(t1);
Texture2D<min12int4>        t2i4   : register(t2);

float4 main(float3 location: A) : SV_Target {
// CHECK:   [[t2f4_val]] = OpLoad %type_2d_image_array %t2f4
// CHECK: [[image_op_1]] = OpImageGather %v4float
    float4 a = t2f4.GatherBlue(gSampler, location, int2(1, 2));

// CHECK:   [[t2i4_val]] = OpLoad %type_2d_image %t2i4
// CHECK: [[image_op_2]] = OpImageFetch %v4int %52 %50 Lod|ConstOffset %51 %11
    int4 b = t2i4.Load(location, int2(1, 2));

    return 1.0;
}

