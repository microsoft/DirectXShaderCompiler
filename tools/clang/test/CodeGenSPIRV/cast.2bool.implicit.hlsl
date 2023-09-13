// RUN: %dxc -T ps_6_0 -E main

// CHECK: [[v2bool_1_0:%\d+]] = OpConstantComposite %v2bool %true %false
// CHECK: [[v3bool_0_1_1:%\d+]] = OpConstantComposite %v3bool %false %true %true
// CHECK: [[v2uint_0_0:%\d+]] = OpConstantComposite %v2uint %uint_0 %uint_0
// CHECK: [[v3float_0_0_0:%\d+]] = OpConstantComposite %v3float %float_0 %float_0 %float_0
// CHECK: [[v3i0:%\d+]] = OpConstantComposite %v3int %int_0 %int_0 %int_0
// CHECK: [[v3u0:%\d+]] = OpConstantComposite %v3uint %uint_0 %uint_0 %uint_0

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    bool b;
    int from1;
    uint from2;
    float from3;

    bool1 vb1;
    bool2 vb2;
    bool3 vb3;
    int1 vfrom1;
    uint2 vfrom2;
    float3 vfrom3;

    // From constant (implicit)
// CHECK: OpStore %b %true
    b = 42;
// CHECK-NEXT: OpStore %b %false
    b = 0.0;

    // From constant expr
// CHECK-NEXT: OpStore %b %false
    b = 35 - 35;

    // From variable (implicit)
// CHECK-NEXT: [[from1:%\d+]] = OpLoad %int %from1
// CHECK-NEXT: [[c1:%\d+]] = OpINotEqual %bool [[from1]] %int_0
// CHECK-NEXT: OpStore %b [[c1]]
    b = from1;
// CHECK-NEXT: [[from2:%\d+]] = OpLoad %uint %from2
// CHECK-NEXT: [[c2:%\d+]] = OpINotEqual %bool [[from2]] %uint_0
// CHECK-NEXT: OpStore %b [[c2]]
    b = from2;
// CHECK-NEXT: [[from3:%\d+]] = OpLoad %float %from3
// CHECK-NEXT: [[c3:%\d+]] = OpFOrdNotEqual %bool [[from3]] %float_0
// CHECK-NEXT: OpStore %b [[c3]]
    b = from3;

    // Vector cases

// CHECK: OpStore %vbc2 [[v2bool_1_0]]
// CHECK: OpStore %vbc3 [[v3bool_0_1_1]]
    bool2 vbc2 = {1, 15 - 15};
    bool3 vbc3 = {0.0, 1.2 + 1.1, 3}; // Mixed

// CHECK-NEXT: [[vfrom1:%\d+]] = OpLoad %int %vfrom1
// CHECK-NEXT: [[vc1:%\d+]] = OpINotEqual %bool [[vfrom1]] %int_0
// CHECK-NEXT: OpStore %vb1 [[vc1]]
    vb1 = vfrom1;
// CHECK-NEXT: [[vfrom2:%\d+]] = OpLoad %v2uint %vfrom2
// CHECK-NEXT: [[vc2:%\d+]] = OpINotEqual %v2bool [[vfrom2]] [[v2uint_0_0]]
// CHECK-NEXT: OpStore %vb2 [[vc2]]
    vb2 = vfrom2;
// CHECK-NEXT: [[vfrom3:%\d+]] = OpLoad %v3float %vfrom3
// CHECK-NEXT: [[vc3:%\d+]] = OpFOrdNotEqual %v3bool [[vfrom3]] [[v3float_0_0_0]]
// CHECK-NEXT: OpStore %vb3 [[vc3]]
    vb3 = vfrom3;

    float2x3 floatMat;
    int2x3   intMat;
    uint2x3  uintMat;
    bool2x3 boolMat;

// CHECK:       [[floatMat:%\d+]] = OpLoad %mat2v3float %floatMat
// CHECK-NEXT: [[floatMat0:%\d+]] = OpCompositeExtract %v3float [[floatMat]] 0
// CHECK-NEXT:  [[boolMat0:%\d+]] = OpFOrdNotEqual %v3bool [[floatMat0]] [[v3float_0_0_0]]
// CHECK-NEXT: [[floatMat1:%\d+]] = OpCompositeExtract %v3float [[floatMat]] 1
// CHECK-NEXT:  [[boolMat1:%\d+]] = OpFOrdNotEqual %v3bool [[floatMat1]] [[v3float_0_0_0]]
// CHECK-NEXT:           {{%\d+}} = OpCompositeConstruct %_arr_v3bool_uint_2 [[boolMat0]] [[boolMat1]]
    boolMat = floatMat;

// CHECK:        [[intMat:%\d+]] = OpLoad %_arr_v3int_uint_2 %intMat
// CHECK-NEXT:  [[intMat0:%\d+]] = OpCompositeExtract %v3int [[intMat]] 0
// CHECK-NEXT: [[boolMat0:%\d+]] = OpINotEqual %v3bool [[intMat0]] [[v3i0]]
// CHECK-NEXT:  [[intMat1:%\d+]] = OpCompositeExtract %v3int [[intMat]] 1
// CHECK-NEXT: [[boolMat1:%\d+]] = OpINotEqual %v3bool [[intMat1]] [[v3i0]]
// CHECK-NEXT:          {{%\d+}} = OpCompositeConstruct %_arr_v3bool_uint_2 [[boolMat0]] [[boolMat1]]
    boolMat = intMat;

// CHECK:      [[uintMat:%\d+]] = OpLoad %_arr_v3uint_uint_2 %uintMat
// CHECK-NEXT: [[uintMat0:%\d+]] = OpCompositeExtract %v3uint [[uintMat]] 0
// CHECK-NEXT: [[boolMat0:%\d+]] = OpINotEqual %v3bool [[uintMat0]] [[v3u0]]
// CHECK-NEXT: [[uintMat1:%\d+]] = OpCompositeExtract %v3uint [[uintMat]] 1
// CHECK-NEXT: [[boolMat1:%\d+]] = OpINotEqual %v3bool [[uintMat1]] [[v3u0]]
// CHECK-NEXT:  {{%\d+}} = OpCompositeConstruct %_arr_v3bool_uint_2 [[boolMat0]] [[boolMat1]]
    boolMat = uintMat;
}
