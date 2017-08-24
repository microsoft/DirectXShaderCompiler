// Run: %dxc -T ps_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

// CHECK:      [[cc00:%\d+]] = OpCompositeConstruct %v3float %float_1 %float_2 %float_3
// CHECK-NEXT: OpStore %mat1 [[cc00]]
    float1x3 mat1 = {1., 2., 3.};
// CHECK-NEXT: [[cc01:%\d+]] = OpCompositeConstruct %v3float %float_1 %float_2 %float_3
// CHECK-NEXT: OpStore %mat2 [[cc01]]
    float1x3 mat2 = {1., {2., {{3.}}}};
// CHECK-NEXT: [[cc02:%\d+]] = OpCompositeConstruct %v3float %float_1 %float_2 %float_3
// CHECK-NEXT: OpStore %mat3 [[cc02]]
    float1x3 mat3 = float1x3(1., 2., 3.);
// CHECK-NEXT: [[mat3:%\d+]] = OpLoad %v3float %mat3
// CHECK-NEXT: OpStore %mat4 [[mat3]]
    float1x3 mat4 = float1x3(mat3);

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
    float1x4 mat5 = {scalar, vec2, vec1};

    float1x2 mat6;
// CHECK-NEXT: [[mat6:%\d+]] = OpLoad %v2float %mat6
// CHECK-NEXT: [[ce2:%\d+]] = OpCompositeExtract %float [[mat6]] 0
// CHECK-NEXT: [[ce3:%\d+]] = OpCompositeExtract %float [[mat6]] 1
// CHECK-NEXT: [[mat6:%\d+]] = OpLoad %v2float %mat6
// CHECK-NEXT: [[ce4:%\d+]] = OpCompositeExtract %float [[mat6]] 0
// CHECK-NEXT: [[ce5:%\d+]] = OpCompositeExtract %float [[mat6]] 1
// CHECK-NEXT: [[cc1:%\d+]] = OpCompositeConstruct %v4float [[ce2]] [[ce3]] [[ce4]] [[ce5]]
// CHECK-NEXT: OpStore %mat7 [[cc1]]
    float1x4 mat7 = {mat6, mat6};
}
