// RUN: %dxc -T ps_6_0 -E main

struct S {
    float2 data[2];
};

StructuredBuffer<S> MySB;

float4 main() : SV_TARGET
{
// CHECK:        [[ac:%\d+]] = OpAccessChain %_ptr_Uniform__arr_v2float_uint_2 %MySB %int_0 %uint_0 %int_0
// CHECK-NEXT: [[mysb:%\d+]] = OpLoad %_arr_v2float_uint_2 [[ac]]
// CHECK-NEXT: [[vec0:%\d+]] = OpCompositeExtract %v2float [[mysb]] 0
// CHECK-NEXT: [[vec1:%\d+]] = OpCompositeExtract %v2float [[mysb]] 1
// CHECK-NEXT:   [[v1:%\d+]] = OpCompositeExtract %float [[vec0]] 0
// CHECK-NEXT:   [[v2:%\d+]] = OpCompositeExtract %float [[vec0]] 1
// CHECK-NEXT:   [[v3:%\d+]] = OpCompositeExtract %float [[vec1]] 0
// CHECK-NEXT:   [[v4:%\d+]] = OpCompositeExtract %float [[vec1]] 1
// CHECK-NEXT:  [[val:%\d+]] = OpCompositeConstruct %_arr_float_uint_4 [[v1]] [[v2]] [[v3]] [[v4]]
// CHECK-NEXT:                 OpStore %data [[val]]
    float data[4] = (float[4])MySB[0].data;
    return data[1];
}
