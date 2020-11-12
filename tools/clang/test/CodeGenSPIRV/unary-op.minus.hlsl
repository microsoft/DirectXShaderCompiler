// Run: %dxc -T ps_6_0 -E main

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    int a, b;
// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[b0:%\d+]] = OpSNegate %int [[a0]]
// CHECK-NEXT: OpStore %b [[b0]]
    b = -a;

    uint c, d;
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %uint %c
// CHECK-NEXT: [[d0:%\d+]] = OpSNegate %uint [[c0]]
// CHECK-NEXT: OpStore %d [[d0]]
    d = -c;

    float i, j;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %float %i
// CHECK-NEXT: [[j0:%\d+]] = OpFNegate %float [[i0]]
// CHECK-NEXT: OpStore %j [[j0]]
    j = -i;

    float1 m, n;
// CHECK-NEXT: [[m0:%\d+]] = OpLoad %float %m
// CHECK-NEXT: [[n0:%\d+]] = OpFNegate %float [[m0]]
// CHECK-NEXT: OpStore %n [[n0]]
    n = -m;

    int3 x, y;
// CHECK-NEXT: [[x0:%\d+]] = OpLoad %v3int %x
// CHECK-NEXT: [[y0:%\d+]] = OpSNegate %v3int [[x0]]
// CHECK-NEXT: OpStore %y [[y0]]
    y = -x;

// CHECK-NEXT:       [[s:%\d+]] = OpLoad %_arr_v4int_uint_4 %s
// CHECK-NEXT:    [[row0:%\d+]] = OpCompositeExtract %v4int [[s]] 0
// CHECK-NEXT: [[result0:%\d+]] = OpSNegate %v4int [[row0]]
// CHECK-NEXT:    [[row1:%\d+]] = OpCompositeExtract %v4int [[s]] 1
// CHECK-NEXT: [[result1:%\d+]] = OpSNegate %v4int [[row1]]
// CHECK-NEXT:    [[row2:%\d+]] = OpCompositeExtract %v4int [[s]] 2
// CHECK-NEXT: [[result2:%\d+]] = OpSNegate %v4int [[row2]]
// CHECK-NEXT:    [[row3:%\d+]] = OpCompositeExtract %v4int [[s]] 3
// CHECK-NEXT: [[result3:%\d+]] = OpSNegate %v4int [[row3]]
// CHECK-NEXT:  [[result:%\d+]] = OpCompositeConstruct %_arr_v4int_uint_4 [[result0]] [[result1]] [[result2]] [[result3]]
// CHECK-NEXT:                    OpStore %r [[result]]
    int4x4 r, s;
    r = -s;

// CHECK-NEXT:       [[u:%\d+]] = OpLoad %mat4v4float %u
// CHECK-NEXT:    [[row0:%\d+]] = OpCompositeExtract %v4float [[u]] 0
// CHECK-NEXT: [[result0:%\d+]] = OpFNegate %v4float [[row0]]
// CHECK-NEXT:    [[row1:%\d+]] = OpCompositeExtract %v4float [[u]] 1
// CHECK-NEXT: [[result1:%\d+]] = OpFNegate %v4float [[row1]]
// CHECK-NEXT:    [[row2:%\d+]] = OpCompositeExtract %v4float [[u]] 2
// CHECK-NEXT: [[result2:%\d+]] = OpFNegate %v4float [[row2]]
// CHECK-NEXT:    [[row3:%\d+]] = OpCompositeExtract %v4float [[u]] 3
// CHECK-NEXT: [[result3:%\d+]] = OpFNegate %v4float [[row3]]
// CHECK-NEXT:  [[result:%\d+]] = OpCompositeConstruct %mat4v4float [[result0]] [[result1]] [[result2]] [[result3]]
// CHECK-NEXT:                    OpStore %t [[result]]
    float4x4 t, u;
    t = -u;
}
