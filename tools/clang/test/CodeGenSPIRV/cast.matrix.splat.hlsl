// Run: %dxc -T vs_6_0 -E main

// CHECK:      [[v2f10_3:%\d+]] = OpConstantComposite %v2float %float_10_3 %float_10_3
// CHECK:      [[v3f10_4:%\d+]] = OpConstantComposite %v3float %float_10_4 %float_10_4 %float_10_4
// CHECK:      [[v2f10_5:%\d+]] = OpConstantComposite %v2float %float_10_5 %float_10_5
// CHECK:    [[m3v2f10_5:%\d+]] = OpConstantComposite %mat3v2float [[v2f10_5]] [[v2f10_5]] [[v2f10_5]]
// CHECK:        [[v2i10:%\d+]] = OpConstantComposite %v2int %int_10 %int_10
// CHECK:   [[int3x2_i10:%\d+]] = OpConstantComposite %_arr_v2int_uint_3 [[v2i10]] [[v2i10]] [[v2i10]]
// CHECK:       [[v2true:%\d+]] = OpConstantComposite %v2bool %true %true
// CHECK: [[bool3x2_true:%\d+]] = OpConstantComposite %_arr_v2bool_uint_3 [[v2true]] [[v2true]] [[v2true]]

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    // TODO: Optimally the following literals can be attached to variable
    // definitions instead of OpStore. Constant evaluation in the front
    // end doesn't really support it for now.

// CHECK:      OpStore %a %float_10_2
    float1x1 a = 10.2;
// CHECK-NEXT: OpStore %b [[v2f10_3]]
    float1x2 b = 10.3;
// CHECK-NEXT: OpStore %c [[v3f10_4]]
    float3x1 c = 10.4;
// CHECK-NEXT: OpStore %d [[m3v2f10_5]]
    float3x2 d = 10.5;
// CHECK-NEXT: OpStore %e [[int3x2_i10]]
      int3x2 e = 10;
// CHECK-NEXT: OpStore %f [[bool3x2_true]]
     bool3x2 f = true;

    float val;
// CHECK-NEXT: [[val0:%\d+]] = OpLoad %float %val
// CHECK-NEXT: OpStore %h [[val0]]
    float1x1 h = val;
// CHECK-NEXT: [[val1:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[cc0:%\d+]] = OpCompositeConstruct %v3float [[val1]] [[val1]] [[val1]]
// CHECK-NEXT: OpStore %i [[cc0]]
    float1x3 i = val;
    float2x1 j;
    float2x3 k;

// CHECK-NEXT: [[val2:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[cc1:%\d+]] = OpCompositeConstruct %v2float [[val2]] [[val2]]
// CHECK-NEXT: OpStore %j [[cc1]]
    j = val;
// CHECK-NEXT: [[val3:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[cc2:%\d+]] = OpCompositeConstruct %v3float [[val3]] [[val3]] [[val3]]
// CHECK-NEXT: [[cc3:%\d+]] = OpCompositeConstruct %mat2v3float [[cc2]] [[cc2]]
// CHECK-NEXT: OpStore %k [[cc3]]
    k = val;

    int intVal;
// CHECK:      [[intVal:%\d+]] = OpLoad %int %intVal
// CHECK-NEXT:    [[cc4:%\d+]] = OpCompositeConstruct %v3int [[intVal]] [[intVal]] [[intVal]]
// CHECK-NEXT: OpStore %m [[cc4]]
    int1x3 m = intVal;
    int2x1 n;
    int2x3 o;
// CHECK:      [[intVal:%\d+]] = OpLoad %int %intVal
// CHECK-NEXT:    [[cc5:%\d+]] = OpCompositeConstruct %v2int [[intVal]] [[intVal]]
// CHECK-NEXT: OpStore %n [[cc5]]
    n = intVal;
// CHECK:        [[intVal:%\d+]] = OpLoad %int %intVal
// CHECK-NEXT: [[v3intVal:%\d+]] = OpCompositeConstruct %v3int [[intVal]] [[intVal]] [[intVal]]
// CHECK-NEXT:      [[cc6:%\d+]] = OpCompositeConstruct %_arr_v3int_uint_2 [[v3intVal]] [[v3intVal]]
// CHECK-NEXT: OpStore %o [[cc6]]
    o = intVal;

    bool boolVal;
// CHECK:      [[boolVal:%\d+]] = OpLoad %bool %boolVal
// CHECK-NEXT:     [[cc7:%\d+]] = OpCompositeConstruct %v3bool [[boolVal]] [[boolVal]] [[boolVal]]
// CHECK-NEXT: OpStore %p [[cc7]]
    bool1x3 p = boolVal;
    bool2x1 q;
    bool2x3 r;
// CHECK:      [[boolVal:%\d+]] = OpLoad %bool %boolVal
// CHECK-NEXT:     [[cc8:%\d+]] = OpCompositeConstruct %v2bool [[boolVal]] [[boolVal]]
// CHECK-NEXT: OpStore %q [[cc8]]
    q = boolVal;
// CHECK:        [[boolVal:%\d+]] = OpLoad %bool %boolVal
// CHECK-NEXT: [[v3boolVal:%\d+]] = OpCompositeConstruct %v3bool [[boolVal]] [[boolVal]] [[boolVal]]
// CHECK-NEXT:       [[cc9:%\d+]] = OpCompositeConstruct %_arr_v3bool_uint_2 [[v3boolVal]] [[v3boolVal]]
// CHECK-NEXT: OpStore %r [[cc9]]
    r = boolVal;
}
