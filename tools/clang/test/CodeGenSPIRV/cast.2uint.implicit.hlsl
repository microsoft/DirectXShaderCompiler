// Run: %dxc -T ps_6_0 -E main

// CHECK: [[v2uint_1_0:%\d+]] = OpConstantComposite %v2uint %uint_1 %uint_0
// CHECK: [[v3uint_0_2_3:%\d+]] = OpConstantComposite %v3uint %uint_0 %uint_2 %uint_3
// CHECK: [[v3u1:%\d+]] = OpConstantComposite %v3uint %uint_1 %uint_1 %uint_1
// CHECK: [[v3u0:%\d+]] = OpConstantComposite %v3uint %uint_0 %uint_0 %uint_0

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    uint i;
    bool from1;
    int from2;
    float from3;

    uint1 vi1;
    uint2 vi2;
    uint3 vi3;
    bool1 vfrom1;
    int2 vfrom2;
    float3 vfrom3;

    // From constant (implicit)
// CHECK: OpStore %i %uint_1
    i = true;
// CHECK-NEXT: OpStore %i %uint_0
    i = 0.0;

    // From constant expr
// CHECK-NEXT: OpStore %i %uint_3
    i = 4.3 - 1.1;

    // From variable (implicit)
// CHECK-NEXT: [[from1:%\d+]] = OpLoad %bool %from1
// CHECK-NEXT: [[c1:%\d+]] = OpSelect %uint [[from1]] %uint_1 %uint_0
// CHECK-NEXT: OpStore %i [[c1]]
    i = from1;
// CHECK-NEXT: [[from2:%\d+]] = OpLoad %int %from2
// CHECK-NEXT: [[c2:%\d+]] = OpBitcast %uint [[from2]]
// CHECK-NEXT: OpStore %i [[c2]]
    i = from2;
// CHECK-NEXT: [[from3:%\d+]] = OpLoad %float %from3
// CHECK-NEXT: [[c3:%\d+]] = OpConvertFToU %uint [[from3]]
// CHECK-NEXT: OpStore %i [[c3]]
    i = from3;

    // Vector cases

// CHECK: OpStore %vic2 [[v2uint_1_0]]
// CHECK: OpStore %vic3 [[v3uint_0_2_3]]
    uint2 vic2 = {true, false};
    uint3 vic3 = {false, 1.1 + 1.2, 3}; // Mixed

// CHECK-NEXT: [[vfrom1:%\d+]] = OpLoad %bool %vfrom1
// CHECK-NEXT: [[vc1:%\d+]] = OpSelect %uint [[vfrom1]] %uint_1 %uint_0
// CHECK-NEXT: OpStore %vi1 [[vc1]]
    vi1 = vfrom1;
// CHECK-NEXT: [[vfrom2:%\d+]] = OpLoad %v2int %vfrom2
// CHECK-NEXT: [[vc2:%\d+]] = OpBitcast %v2uint [[vfrom2]]
// CHECK-NEXT: OpStore %vi2 [[vc2]]
    vi2 = vfrom2;
// CHECK-NEXT: [[vfrom3:%\d+]] = OpLoad %v3float %vfrom3
// CHECK-NEXT: [[vc3:%\d+]] = OpConvertFToU %v3uint [[vfrom3]]
// CHECK-NEXT: OpStore %vi3 [[vc3]]
    vi3 = vfrom3;

    int2x3   intMat;
    float2x3 floatMat;
    uint2x3  uintMat;
    bool2x3  boolMat;

// CHECK:       [[boolMat:%\d+]] = OpLoad %_arr_v3bool_uint_2 %boolMat
// CHECK-NEXT: [[boolMat0:%\d+]] = OpCompositeExtract %v3bool [[boolMat]] 0
// CHECK-NEXT: [[uintMat0:%\d+]] = OpSelect %v3uint [[boolMat0]] [[v3u1]] [[v3u0]]
// CHECK-NEXT: [[boolMat1:%\d+]] = OpCompositeExtract %v3bool [[boolMat]] 1
// CHECK-NEXT: [[uintMat1:%\d+]] = OpSelect %v3uint [[boolMat1]] [[v3u1]] [[v3u0]]
// CHECK-NEXT:          {{%\d+}} = OpCompositeConstruct %_arr_v3uint_uint_2 [[uintMat0]] [[uintMat1]]
    uintMat = boolMat;
// CHECK:        [[intMat:%\d+]] = OpLoad %_arr_v3int_uint_2 %intMat
// CHECK-NEXT:  [[intMat0:%\d+]] = OpCompositeExtract %v3int [[intMat]] 0
// CHECK-NEXT: [[uintMat0:%\d+]] = OpBitcast %v3uint [[intMat0]]
// CHECK-NEXT:  [[intMat1:%\d+]] = OpCompositeExtract %v3int [[intMat]] 1
// CHECK-NEXT: [[uintMat1:%\d+]] = OpBitcast %v3uint [[intMat1]]
// CHECK-NEXT:          {{%\d+}} = OpCompositeConstruct %_arr_v3uint_uint_2 [[uintMat0]] [[uintMat1]]
    uintMat = intMat;
// CHECK:       [[floatMat:%\d+]] = OpLoad %mat2v3float %floatMat
// CHECK-NEXT: [[floatMat0:%\d+]] = OpCompositeExtract %v3float [[floatMat]] 0
// CHECK-NEXT:  [[uintMat0:%\d+]] = OpConvertFToU %v3uint [[floatMat0]]
// CHECK-NEXT: [[floatMat1:%\d+]] = OpCompositeExtract %v3float [[floatMat]] 1
// CHECK-NEXT:  [[uintMat1:%\d+]] = OpConvertFToU %v3uint [[floatMat1]]
// CHECK-NEXT:           {{%\d+}} = OpCompositeConstruct %_arr_v3uint_uint_2 [[uintMat0]] [[uintMat1]]
    uintMat = floatMat;
}
