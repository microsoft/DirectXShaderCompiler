// Run: %dxc -T ps_6_0 -E main

// CHECK: [[v2uint_1_0:%\d+]] = OpConstantComposite %v2uint %uint_1 %uint_0
// CHECK: [[v3uint_0_2_3:%\d+]] = OpConstantComposite %v3uint %uint_0 %uint_2 %uint_3

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
// CHECK: %vic2 = OpVariable %_ptr_Function_v2uint Function [[v2uint_1_0]]
// CHECK: %vic3 = OpVariable %_ptr_Function_v3uint Function [[v3uint_0_2_3]]

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

    // See the beginning for generated code
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
}
