// Run: %dxc -T ps_6_0 -E main

// CHECK: [[v2bool_1_0:%\d+]] = OpConstantComposite %v2bool %true %false
// CHECK: [[v3bool_0_1_1:%\d+]] = OpConstantComposite %v3bool %false %true %true
// CHECK: [[v2uint_0_0:%\d+]] = OpConstantComposite %v2uint %uint_0 %uint_0
// CHECK: [[v3float_0_0_0:%\d+]] = OpConstantComposite %v3float %float_0 %float_0 %float_0

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
// CHECK: %vbc2 = OpVariable %_ptr_Function_v2bool Function [[v2bool_1_0]]
// CHECK: %vbc3 = OpVariable %_ptr_Function_v3bool Function [[v3bool_0_1_1]]

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

    // See the beginning for generated code
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
}
