// Run: %dxc -T ps_6_0 -E main

// CHECK: [[v2float_1_0:%\d+]] = OpConstantComposite %v2float %float_1 %float_0
// CHECK: [[v3float_0_4_n3:%\d+]] = OpConstantComposite %v3float %float_0 %float_4 %float_n3

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    float f;
    bool from1;
    uint from2;
    int from3;

    float1 vf1;
    float2 vf2;
    float3 vf3;
    bool1 vfrom1;
    uint2 vfrom2;
    int3 vfrom3;
// CHECK: %vfc2 = OpVariable %_ptr_Function_v2float Function [[v2float_1_0]]
// CHECK: %vfc3 = OpVariable %_ptr_Function_v3float Function [[v3float_0_4_n3]]

    // From constant (implicit)
// CHECK: OpStore %f %float_1
    f = true;
// CHECK-NEXT: OpStore %f %float_3
    f = 3u;

    // From constant expr
// CHECK-NEXT: OpStore %f %float_n1
    f = 5 - 6;

    // From variable (implicit)
// CHECK-NEXT: [[from1:%\d+]] = OpLoad %bool %from1
// CHECK-NEXT: [[c1:%\d+]] = OpSelect %float [[from1]] %float_1 %float_0
// CHECK-NEXT: OpStore %f [[c1]]
    f = from1;
// CHECK-NEXT: [[from2:%\d+]] = OpLoad %uint %from2
// CHECK-NEXT: [[c2:%\d+]] = OpConvertUToF %float [[from2]]
// CHECK-NEXT: OpStore %f [[c2]]
    f = from2;
// CHECK-NEXT: [[from3:%\d+]] = OpLoad %int %from3
// CHECK-NEXT: [[c3:%\d+]] = OpConvertSToF %float [[from3]]
// CHECK-NEXT: OpStore %f [[c3]]
    f = from3;

    // Vector cases

    // See the beginning for generated code
    float2 vfc2 = {true, false};
    float3 vfc3 = {false, 4u, -3}; // Mixed

// CHECK-NEXT: [[vfrom1:%\d+]] = OpLoad %bool %vfrom1
// CHECK-NEXT: [[vc1:%\d+]] = OpSelect %float [[vfrom1]] %float_1 %float_0
// CHECK-NEXT: OpStore %vf1 [[vc1]]
    vf1 = vfrom1;
// CHECK-NEXT: [[vfrom2:%\d+]] = OpLoad %v2uint %vfrom2
// CHECK-NEXT: [[vc2:%\d+]] = OpConvertUToF %v2float [[vfrom2]]
// CHECK-NEXT: OpStore %vf2 [[vc2]]
    vf2 = vfrom2;
// CHECK-NEXT: [[vfrom3:%\d+]] = OpLoad %v3int %vfrom3
// CHECK-NEXT: [[vc3:%\d+]] = OpConvertSToF %v3float [[vfrom3]]
// CHECK-NEXT: OpStore %vf3 [[vc3]]
    vf3 = vfrom3;
}
