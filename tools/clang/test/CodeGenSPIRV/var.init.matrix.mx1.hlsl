// Run: %dxc -T ps_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

// CHECK:      [[cc00:%\d+]] = OpCompositeConstruct %v3float %float_1 %float_2 %float_3
// CHECK-NEXT: OpStore %mat1 [[cc00]]
    float3x1 mat1 = {1., 2., 3.};
// CHECK-NEXT: [[cc01:%\d+]] = OpCompositeConstruct %v3float %float_1 %float_2 %float_3
// CHECK-NEXT: OpStore %mat2 [[cc01]]
    float3x1 mat2 = {1., {2., {{3.}}}};
// CHECK-NEXT: [[cc02:%\d+]] = OpCompositeConstruct %v3float %float_1 %float_2 %float_3
// CHECK-NEXT: OpStore %mat3 [[cc02]]
    float3x1 mat3 = float3x1(1., 2., 3.);
// CHECK-NEXT: [[mat3:%\d+]] = OpLoad %v3float %mat3
// CHECK-NEXT: OpStore %mat4 [[mat3]]
    float3x1 mat4 = float3x1(mat3);

    int scalar;
    bool1 vec1;
    uint2 vec2;
// CHECK-NEXT: [[scalar:%\d+]] = OpLoad %int %scalar
// CHECK-NEXT: [[cv0:%\d+]] = OpConvertSToF %float [[scalar]]
// CHECK-NEXT: [[vec2:%\d+]] = OpLoad %v2uint %vec2
// CHECK-NEXT: [[ce0:%\d+]] = OpCompositeExtract %uint [[vec2]] 0
// CHECK-NEXT: [[ce1:%\d+]] = OpCompositeExtract %uint [[vec2]] 1
// CHECK-NEXT: [[cv1:%\d+]] = OpConvertUToF %float [[ce0]]
// CHECK-NEXT: [[cv2:%\d+]] = OpConvertUToF %float [[ce1]]
// CHECK-NEXT: [[vec1:%\d+]] = OpLoad %bool %vec1
// CHECK-NEXT: [[cv3:%\d+]] = OpSelect %float [[vec1]] %float_1 %float_0
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v4float [[cv0]] [[cv1]] [[cv2]] [[cv3]]
// CHECK-NEXT: OpStore %mat5 [[cc0]]
    float4x1 mat5 = {scalar, vec2, vec1};
}
