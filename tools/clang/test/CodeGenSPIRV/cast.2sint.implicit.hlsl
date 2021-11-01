// RUN: %dxc -T ps_6_0 -E main

// CHECK: [[v2int_1_0:%\d+]] = OpConstantComposite %v2int %int_1 %int_0
// CHECK: [[v3int_0_2_n3:%\d+]] = OpConstantComposite %v3int %int_0 %int_2 %int_n3
// CHECK: [[v3i1:%\d+]] = OpConstantComposite %v3int %int_1 %int_1 %int_1
// CHECK: [[v3i0:%\d+]] = OpConstantComposite %v3int %int_0 %int_0 %int_0

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    int i;
    bool from1;
    uint from2;
    float from3;

    int1 vi1;
    int2 vi2;
    int3 vi3;
    bool1 vfrom1;
    uint2 vfrom2;
    float3 vfrom3;

    // From constant (implicit)
// CHECK: OpStore %i %int_1
    i = true;
// CHECK-NEXT: OpStore %i %int_0
    i = 0.0;

    // From constant expr
// CHECK-NEXT: OpStore %i %int_n3
    i = 1.1 - 4.3;

    // From variable (implicit)
// CHECK-NEXT: [[from1:%\d+]] = OpLoad %bool %from1
// CHECK-NEXT: [[c1:%\d+]] = OpSelect %int [[from1]] %int_1 %int_0
// CHECK-NEXT: OpStore %i [[c1]]
    i = from1;
// CHECK-NEXT: [[from2:%\d+]] = OpLoad %uint %from2
// CHECK-NEXT: [[c2:%\d+]] = OpBitcast %int [[from2]]
// CHECK-NEXT: OpStore %i [[c2]]
    i = from2;
// CHECK-NEXT: [[from3:%\d+]] = OpLoad %float %from3
// CHECK-NEXT: [[c3:%\d+]] = OpConvertFToS %int [[from3]]
// CHECK-NEXT: OpStore %i [[c3]]
    i = from3;

    // Vector cases

// CHECK: OpStore %vic2 [[v2int_1_0]]
// CHECK: OpStore %vic3 [[v3int_0_2_n3]]
    int2 vic2 = {true, false};
    int3 vic3 = {false, 1.1 + 1.2, -3}; // Mixed

// CHECK-NEXT: [[vfrom1:%\d+]] = OpLoad %bool %vfrom1
// CHECK-NEXT: [[vc1:%\d+]] = OpSelect %int [[vfrom1]] %int_1 %int_0
// CHECK-NEXT: OpStore %vi1 [[vc1]]
    vi1 = vfrom1;
// CHECK-NEXT: [[vfrom2:%\d+]] = OpLoad %v2uint %vfrom2
// CHECK-NEXT: [[vc2:%\d+]] = OpBitcast %v2int [[vfrom2]]
// CHECK-NEXT: OpStore %vi2 [[vc2]]
    vi2 = vfrom2;
// CHECK-NEXT: [[vfrom3:%\d+]] = OpLoad %v3float %vfrom3
// CHECK-NEXT: [[vc3:%\d+]] = OpConvertFToS %v3int [[vfrom3]]
// CHECK-NEXT: OpStore %vi3 [[vc3]]
    vi3 = vfrom3;

    int2x3   intMat;
    float2x3 floatMat;
    uint2x3  uintMat;
    bool2x3  boolMat;

// CHECK:       [[boolMat:%\d+]] = OpLoad %_arr_v3bool_uint_2 %boolMat
// CHECK-NEXT: [[boolMat0:%\d+]] = OpCompositeExtract %v3bool [[boolMat]] 0
// CHECK-NEXT:  [[intMat0:%\d+]] = OpSelect %v3int [[boolMat0]] [[v3i1]] [[v3i0]]
// CHECK-NEXT: [[boolMat1:%\d+]] = OpCompositeExtract %v3bool [[boolMat]] 1
// CHECK-NEXT:  [[intMat1:%\d+]] = OpSelect %v3int [[boolMat1]] [[v3i1]] [[v3i0]]
// CHECK-NEXT:          {{%\d+}} = OpCompositeConstruct %_arr_v3int_uint_2 [[intMat0]] [[intMat1]]
    intMat = boolMat;
// CHECK:       [[uintMat:%\d+]] = OpLoad %_arr_v3uint_uint_2 %uintMat
// CHECK-NEXT: [[uintMat0:%\d+]] = OpCompositeExtract %v3uint [[uintMat]] 0
// CHECK-NEXT:  [[intMat0:%\d+]] = OpBitcast %v3int [[uintMat0]]
// CHECK-NEXT: [[uintMat1:%\d+]] = OpCompositeExtract %v3uint [[uintMat]] 1
// CHECK-NEXT:  [[intMat1:%\d+]] = OpBitcast %v3int [[uintMat1]]
// CHECK-NEXT:          {{%\d+}} = OpCompositeConstruct %_arr_v3int_uint_2 [[intMat0]] [[intMat1]]
    intMat = uintMat;
// CHECK:       [[floatMat:%\d+]] = OpLoad %mat2v3float %floatMat
// CHECK-NEXT: [[floatMat0:%\d+]] = OpCompositeExtract %v3float [[floatMat]] 0
// CHECK-NEXT:   [[intMat0:%\d+]] = OpConvertFToS %v3int [[floatMat0]]
// CHECK-NEXT: [[floatMat1:%\d+]] = OpCompositeExtract %v3float [[floatMat]] 1
// CHECK-NEXT:   [[intMat1:%\d+]] = OpConvertFToS %v3int [[floatMat1]]
// CHECK-NEXT:           {{%\d+}} = OpCompositeConstruct %_arr_v3int_uint_2 [[intMat0]] [[intMat1]]
    intMat = floatMat;
}
