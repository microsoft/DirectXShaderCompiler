// Run: %dxc -T vs_6_0 -E main

// Tests should cover vector swizzling
// * from lvalue/rvalue
// * used as lvalue/rvalue
// * selecting one/two/three/four elements
// * selecting the same element multiple times
// * selecting more elements than the base vector
// * selecting the original vector
// * element selection order
// * assignment/compound assignment
// * continuous selection

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    float4 v4f1, v4f2;
    float3 v3f1, v3f2;
    float2 v2f;

    // Assign to whole vector
// CHECK:      [[v0:%\d+]] = OpLoad %v4float %v4f1
// CHECK-NEXT: OpStore %v4f2 [[v0]]
    v4f2 = v4f1.xyzw; // rhs: all in original order

    // Assign to one element
// CHECK-NEXT: [[ac0:%\d+]] = OpAccessChain %_ptr_Function_float %v4f1 %int_0
// CHECK-NEXT: [[v1:%\d+]] = OpLoad %float [[ac0]]
// CHECK-NEXT: [[ac1:%\d+]] = OpAccessChain %_ptr_Function_float %v4f2 %int_0
// CHECK-NEXT: OpStore [[ac1]] [[v1]]
    v4f2.x = v4f1.r;
    // Assign to two elements
// CHECK-NEXT: [[v2:%\d+]] = OpLoad %v4float %v4f1
// CHECK-NEXT: [[vs0:%\d+]] = OpVectorShuffle %v2float [[v2]] [[v2]] 0 1
// CHECK-NEXT: [[v3:%\d+]] = OpLoad %v4float %v4f2
// CHECK-NEXT: [[vs1:%\d+]] = OpVectorShuffle %v4float [[v3]] [[vs0]] 4 5 2 3
// CHECK-NEXT: OpStore %v4f2 [[vs1]]
    v4f2.xy = v4f1.rg;
    // Assign to three elements
// CHECK-NEXT: [[v4:%\d+]] = OpLoad %v4float %v4f1
// CHECK-NEXT: [[vs2:%\d+]] = OpVectorShuffle %v3float [[v4]] [[v4]] 0 1 2
// CHECK-NEXT: [[v5:%\d+]] = OpLoad %v4float %v4f2
// CHECK-NEXT: [[vs3:%\d+]] = OpVectorShuffle %v4float [[v5]] [[vs2]] 4 5 6 3
// CHECK-NEXT: OpStore %v4f2 [[vs3]]
    v4f2.xyz = v4f1.rgb;
    // Assign to four elements
// CHECK-NEXT: [[v6:%\d+]] = OpLoad %v4float %v4f1
// CHECK-NEXT: OpStore %v4f2 [[v6]]
    v4f2.xyzw = v4f1.rgba; // lhs: all in original order

    // Random order
// CHECK-NEXT: [[v7:%\d+]] = OpLoad %v4float %v4f1
// CHECK-NEXT: [[vs4:%\d+]] = OpVectorShuffle %v4float [[v7]] [[v7]] 3 1 2 0
// CHECK-NEXT: [[v8:%\d+]] = OpLoad %v4float %v4f2
// CHECK-NEXT: [[vs5:%\d+]] = OpVectorShuffle %v4float [[v8]] [[vs4]] 6 7 5 4
// CHECK-NEXT: OpStore %v4f2 [[vs5]]
    v4f2.abrg = v4f1.wyzx;

    // Assign from whole vector
// CHECK-NEXT: [[v9:%\d+]] = OpLoad %v2float %v2f
// CHECK-NEXT: [[v10:%\d+]] = OpLoad %v4float %v4f2
// CHECK-NEXT: [[vs6:%\d+]] = OpVectorShuffle %v4float [[v10]] [[v9]] 0 1 4 5
// CHECK-NEXT: OpStore %v4f2 [[vs6]]
    v4f2.zw = v2f;

    // Select the same element multiple times (can only happen for rhs)
// CHECK-NEXT: [[v11:%\d+]] = OpLoad %v4float %v4f1
// CHECK-NEXT: [[vs7:%\d+]] = OpVectorShuffle %v4float [[v11]] [[v11]] 0 0 1 1
// CHECK-NEXT: OpStore %v4f2 [[vs7]]
    v4f2 = v4f1.xxyy;

    // Select more than original size (can only happen for rhs)
// CHECK-NEXT: [[v13:%\d+]] = OpLoad %v2float %v2f
// CHECK-NEXT: [[vs9:%\d+]] = OpVectorShuffle %v4float [[v13]] [[v13]] 0 1 1 0
// CHECK-NEXT: OpStore %v4f2 [[vs9]]
    v4f2 = v2f.xyyx;

    // Select from rvalue & chained assignment
// CHECK-NEXT: [[v15:%\d+]] = OpLoad %v3float %v3f1
// CHECK-NEXT: [[v16:%\d+]] = OpLoad %v3float %v3f2
// CHECK-NEXT: [[add0:%\d+]] = OpFAdd %v3float [[v15]] [[v16]]
// CHECK-NEXT: [[vs11:%\d+]] = OpVectorShuffle %v2float [[add0]] [[add0]] 1 0
// CHECK-NEXT: OpStore %v2f [[vs11]]
// CHECK-NEXT: [[v17:%\d+]] = OpLoad %v4float %v4f2
// CHECK-NEXT: [[vs12:%\d+]] = OpVectorShuffle %v4float [[v17]] [[vs11]] 0 1 5 4
// CHECK-NEXT: OpStore %v4f2 [[vs12]]
    v4f2.wz = v2f = (v3f1 + v3f2).yx;

// CHECK-NEXT: [[v18:%\d+]] = OpLoad %v3float %v3f1
// CHECK-NEXT: [[v19:%\d+]] = OpLoad %v3float %v3f2
// CHECK-NEXT: [[mul0:%\d+]] = OpFMul %v3float [[v18]] [[v19]]
// CHECK-NEXT: [[ce0:%\d+]] = OpCompositeExtract %float [[mul0]] 1
// CHECK-NEXT: [[ac2:%\d+]] = OpAccessChain %_ptr_Function_float %v2f %int_1
// CHECK-NEXT: OpStore [[ac2]] [[ce0]]
// CHECK-NEXT: [[ac3:%\d+]] = OpAccessChain %_ptr_Function_float %v2f %int_0
// CHECK-NEXT: OpStore [[ac3]] [[ce0]]
    v2f.x = v2f.y = (v3f1 * v3f2).y; // one element

    // Use in binary operations
// CHECK-NEXT: [[v20:%\d+]] = OpLoad %v3float %v3f1
// CHECK-NEXT: [[vs13:%\d+]] = OpVectorShuffle %v2float [[v20]] [[v20]] 0 1
// CHECK-NEXT: [[mul1:%\d+]] = OpVectorTimesScalar %v2float [[vs13]] %float_2
// CHECK-NEXT: [[v21:%\d+]] = OpLoad %v3float %v3f2
// CHECK-NEXT: [[vs14:%\d+]] = OpVectorShuffle %v2float [[v21]] [[v21]] 1 2
// CHECK-NEXT: [[mul2:%\d+]] = OpFMul %v2float [[mul1]] [[vs14]]
// CHECK-NEXT: OpStore %v2f [[mul2]]
    v2f = 2.0 * v3f1.xy * v3f2.yz;

    // Continuous selection

    // v2f.(1, 0).(1, 0, 1) -> v2f.(0, 1, 0)
    // v4f2.(3, 2, 0).(1, 0, 2) -> v4f2.(2, 3, 0)
    // Write rhs.0 (+4 = 4) to lhs.2
    // Write rhs.1 (+4 = 5) to lhs.3
    // Write rhs.2 (+4 = 6) to lhs.0
    // Keep lhs.1
    // So final selectors to write to lhs.(0, 1, 2, 3): 6, 1, 4, 5
// CHECK-NEXT: [[v22:%\d+]] = OpLoad %v2float %v2f
// CHECK-NEXT: [[vs15:%\d+]] = OpVectorShuffle %v3float [[v22]] [[v22]] 0 1 0
// CHECK-NEXT: [[v23:%\d+]] = OpLoad %v4float %v4f2
// CHECK-NEXT: [[vs16:%\d+]] = OpVectorShuffle %v4float [[v23]] [[vs15]] 6 1 4 5
// CHECK-NEXT: OpStore %v4f2 [[vs16]]
    v4f2.wzx.grb = v2f.gr.yxy; // select more than original, write to a part

// CHECK-NEXT: [[v24:%\d+]] = OpLoad %v4float %v4f1
// CHECK-NEXT: OpStore %v4f2 [[v24]]
    v4f2.wzyx.abgr.xywz.rgab = v4f1.xyzw.xyzw.rgab.rgab; // from original vector to original vector

    // Note that we cannot generate OpAccessChain for v4f1 since v4f1.xzyx is
    // already not a lvalue!
// CHECK-NEXT: [[v24:%\d+]] = OpLoad %v4float %v4f1
// CHECK-NEXT: [[ce1:%\d+]] = OpCompositeExtract %float [[v24]] 2
// CHECK-NEXT: [[ac4:%\d+]] = OpAccessChain %_ptr_Function_float %v4f2 %int_1
// CHECK-NEXT: OpStore [[ac4]] [[ce1]]
    v4f2.wzyx.zy.x = v4f1.xzyx.y.x; // from one element (rvalue) to one element (lvalue)

// CHECK-NEXT: [[ac2:%\d+]] = OpAccessChain %_ptr_Function_float %v4f1 %int_1
// CHECK-NEXT: [[e0:%\d+]] = OpLoad %float [[ac2]]
// CHECK-NEXT: [[ac3:%\d+]] = OpAccessChain %_ptr_Function_float %v4f2 %int_3
// CHECK-NEXT: OpStore [[ac3]] [[e0]]
    v4f2.w.x.x.x = v4f1.y.x.x.x; // continuously selecting one element
}