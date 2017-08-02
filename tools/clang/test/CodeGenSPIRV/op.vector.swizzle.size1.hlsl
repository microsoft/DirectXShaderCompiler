// Run: %dxc -T vs_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    float2 v2f;
    float1 v1f1, v1f2;
    float sf;

    // Assign to whole vector
// CHECK:      [[v0:%\d+]] = OpLoad %float %v1f1
// CHECK-NEXT: OpStore %v1f2 [[v0]]
    v1f2 = v1f1.x; // rhs: all in original order

    // Assign from whole vector, to one element
// CHECK-NEXT: [[v1:%\d+]] = OpLoad %float %v1f1
// CHECK-NEXT: OpStore %v1f2 [[v1]]
    v1f2.x = v1f1;

    // Select one element multiple times & select more than size
// CHECK-NEXT: [[v2:%\d+]] = OpLoad %float %v1f1
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v2float [[v2]] [[v2]]
// CHECK-NEXT: OpStore %v2f [[cc0]]
    v2f = v1f1.xx;

    // Select from rvalue & chained assignment
// CHECK-NEXT: [[v3:%\d+]] = OpLoad %float %v1f1
// CHECK-NEXT: [[v4:%\d+]] = OpLoad %float %v1f2
// CHECK-NEXT: [[mul0:%\d+]] = OpFMul %float [[v3]] [[v4]]
// CHECK-NEXT: [[ac0:%\d+]] = OpAccessChain %_ptr_Function_float %v2f %int_1
// CHECK-NEXT: OpStore [[ac0]] [[mul0]]
// CHECK-NEXT: OpStore %v1f2 [[mul0]]
// CHECK-NEXT: OpStore %sf [[mul0]]
    sf = v1f2 = v2f.y = (v1f1 * v1f2).r;

    // Continuous selection
// CHECK-NEXT: [[v5:%\d+]] = OpLoad %float %v1f1
// CHECK-NEXT: OpStore %v1f2 [[v5]]
    v1f2.x.r.x = v1f1.r.x.r;
}
