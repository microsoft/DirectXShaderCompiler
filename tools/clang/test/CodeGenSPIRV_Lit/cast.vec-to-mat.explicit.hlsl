// RUN: %dxc -T ps_6_0 -E main

float4 main(float4 input : A) : SV_Target {
// CHECK:       [[vec:%\d+]] = OpLoad %v4float %input
// CHECK-NEXT: [[vec1:%\d+]] = OpVectorShuffle %v2float [[vec]] [[vec]] 0 1
// CHECK-NEXT: [[vec2:%\d+]] = OpVectorShuffle %v2float [[vec]] [[vec]] 2 3
// CHECK-NEXT:  [[mat:%\d+]] = OpCompositeConstruct %mat2v2float [[vec1]] [[vec2]]
// CHECK-NEXT:                 OpStore %mat1 [[mat]]
    float2x2 mat1 = (             float2x2)input;

// CHECK:       [[vec:%\d+]] = OpLoad %v4float %input
// CHECK-NEXT: [[vec1:%\d+]] = OpVectorShuffle %v2float [[vec]] [[vec]] 0 1
// CHECK-NEXT: [[vec2:%\d+]] = OpVectorShuffle %v2float [[vec]] [[vec]] 2 3
// CHECK-NEXT:  [[mat:%\d+]] = OpCompositeConstruct %mat2v2float [[vec1]] [[vec2]]
// CHECK-NEXT:                 OpStore %mat2 [[mat]]
    float2x2 mat2 = (row_major    float2x2)input;

// CHECK:       [[vec:%\d+]] = OpLoad %v4float %input
// CHECK-NEXT: [[vec1:%\d+]] = OpVectorShuffle %v2float [[vec]] [[vec]] 0 1
// CHECK-NEXT: [[vec2:%\d+]] = OpVectorShuffle %v2float [[vec]] [[vec]] 2 3
// CHECK-NEXT:  [[mat:%\d+]] = OpCompositeConstruct %mat2v2float [[vec1]] [[vec2]]
// CHECK-NEXT:                 OpStore %mat3 [[mat]]
    float2x2 mat3 = (column_major float2x2)input;

// CHECK:         [[a:%\d+]] = OpLoad %v4int %a
// CHECK-NEXT: [[vec1:%\d+]] = OpVectorShuffle %v2int [[a]] [[a]] 0 1
// CHECK-NEXT: [[vec2:%\d+]] = OpVectorShuffle %v2int [[a]] [[a]] 2 3
// CHECK-NEXT:      {{%\d+}} = OpCompositeConstruct %_arr_v2int_uint_2 [[vec1]] [[vec2]]
    int4 a;
    int2x2 b = a;

    return float4(mat1[0][0], mat2[0][1], mat3[1][0], mat1[1][1]);
}
