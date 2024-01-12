// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

struct S {
    float2 data[2];
};

StructuredBuffer<S> MySB;

float4 main() : SV_TARGET
{
// CHECK:        [[ac:%[0-9]+]] = OpAccessChain %_ptr_Uniform__arr_v2float_uint_2 %MySB %int_0 %uint_0 %int_0
// CHECK-NEXT: [[mysb:%[0-9]+]] = OpLoad %_arr_v2float_uint_2 [[ac]]
// CHECK-NEXT: [[vec0:%[0-9]+]] = OpCompositeExtract %v2float [[mysb]] 0
// CHECK-NEXT: [[vec1:%[0-9]+]] = OpCompositeExtract %v2float [[mysb]] 1
// CHECK-NEXT:   [[v1:%[0-9]+]] = OpCompositeExtract %float [[vec0]] 0
// CHECK-NEXT:   [[v2:%[0-9]+]] = OpCompositeExtract %float [[vec0]] 1
// CHECK-NEXT:   [[v3:%[0-9]+]] = OpCompositeExtract %float [[vec1]] 0
// CHECK-NEXT:   [[v4:%[0-9]+]] = OpCompositeExtract %float [[vec1]] 1
// CHECK-NEXT:  [[val:%[0-9]+]] = OpCompositeConstruct %_arr_float_uint_4 [[v1]] [[v2]] [[v3]] [[v4]]
// CHECK-NEXT:                 OpStore %data [[val]]
    float data[4] = (float[4])MySB[0].data;
    return data[1];
}
