// Run: %dxc -T ps_6_0 -E main

struct S {
    float2 data[2];
};

StructuredBuffer<S> MySB;

float4 main() : SV_TARGET
{
// CHECK:      [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %MySB %int_0 %uint_0 %int_0 %uint_0
// CHECK-NEXT: [[vec:%\d+]] = OpLoad %v2float [[ptr]]
// CHECK-NEXT:  [[v1:%\d+]] = OpCompositeExtract %float [[vec]] 0
// CHECK-NEXT:  [[v2:%\d+]] = OpCompositeExtract %float [[vec]] 1
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %MySB %int_0 %uint_0 %int_0 %uint_1
// CHECK-NEXT: [[vec:%\d+]] = OpLoad %v2float [[ptr]]
// CHECK-NEXT:  [[v3:%\d+]] = OpCompositeExtract %float [[vec]] 0
// CHECK-NEXT:  [[v4:%\d+]] = OpCompositeExtract %float [[vec]] 1
// CHECK-NEXT: [[val:%\d+]] = OpCompositeConstruct %_arr_float_uint_4 [[v1]] [[v2]] [[v3]] [[v4]]
// CHECK-NEXT:                OpStore %data [[val]]
    float data[4] = (float[4])MySB[0].data;
    return data[1];
}
