// Run: %dxc -T ps_6_0 -E main

// CHECK: [[v3float_1_1_1:%\d+]] = OpConstantComposite %v3float %float_1 %float_1 %float_1

RWTexture2D<float>  MyTexture : register(u1);
RWBuffer<int> intbuf;

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    int a, b;
// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[a1:%\d+]] = OpIAdd %int [[a0]] %int_1
// CHECK-NEXT: OpStore %a [[a1]]
// CHECK-NEXT: [[a2:%\d+]] = OpLoad %int %a
// CHECK-NEXT: OpStore %b [[a2]]
    b = ++a;
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[a3:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[a4:%\d+]] = OpIAdd %int [[a3]] %int_1
// CHECK-NEXT: OpStore %a [[a4]]
// CHECK-NEXT: OpStore %a [[b0]]
    ++a = b;

// Spot check a complicated usage case. No need to duplicate it for all types.

// CHECK-NEXT: [[b1:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[b2:%\d+]] = OpIAdd %int [[b1]] %int_1
// CHECK-NEXT: OpStore %b [[b2]]
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[b4:%\d+]] = OpIAdd %int [[b3]] %int_1
// CHECK-NEXT: OpStore %b [[b4]]
// CHECK-NEXT: [[b5:%\d+]] = OpLoad %int %b

// CHECK-NEXT: [[a5:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[a6:%\d+]] = OpIAdd %int [[a5]] %int_1
// CHECK-NEXT: OpStore %a [[a6]]
// CHECK-NEXT: [[a7:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[a8:%\d+]] = OpIAdd %int [[a7]] %int_1
// CHECK-NEXT: OpStore %a [[a8]]
// CHECK-NEXT: OpStore %a [[b5]]
    ++(++a) = ++(++b);

    uint i, j;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[i1:%\d+]] = OpIAdd %uint [[i0]] %uint_1
// CHECK-NEXT: OpStore %i [[i1]]
// CHECK-NEXT: [[i2:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: OpStore %j [[i2]]
    j = ++i;
// CHECK-NEXT: [[j0:%\d+]] = OpLoad %uint %j
// CHECK-NEXT: [[i3:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[i4:%\d+]] = OpIAdd %uint [[i3]] %uint_1
// CHECK-NEXT: OpStore %i [[i4]]
// CHECK-NEXT: OpStore %i [[j0]]
    ++i = j;

    float o, p;
// CHECK-NEXT: [[o0:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[o1:%\d+]] = OpFAdd %float [[o0]] %float_1
// CHECK-NEXT: OpStore %o [[o1]]
// CHECK-NEXT: [[o2:%\d+]] = OpLoad %float %o
// CHECK-NEXT: OpStore %p [[o2]]
    p = ++o;
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %float %p
// CHECK-NEXT: [[o3:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[o4:%\d+]] = OpFAdd %float [[o3]] %float_1
// CHECK-NEXT: OpStore %o [[o4]]
// CHECK-NEXT: OpStore %o [[p0]]
    ++o = p;

    float3 x, y;
// CHECK-NEXT: [[x0:%\d+]] = OpLoad %v3float %x
// CHECK-NEXT: [[x1:%\d+]] = OpFAdd %v3float [[x0]] [[v3float_1_1_1]]
// CHECK-NEXT: OpStore %x [[x1]]
// CHECK-NEXT: [[x2:%\d+]] = OpLoad %v3float %x
// CHECK-NEXT: OpStore %y [[x2]]
    y = ++x;
// CHECK-NEXT: [[y0:%\d+]] = OpLoad %v3float %y
// CHECK-NEXT: [[x3:%\d+]] = OpLoad %v3float %x
// CHECK-NEXT: [[x4:%\d+]] = OpFAdd %v3float [[x3]] [[v3float_1_1_1]]
// CHECK-NEXT: OpStore %x [[x4]]
// CHECK-NEXT: OpStore %x [[y0]]
    ++x = y;

  uint2 index;
// CHECK:      [[index:%\d+]] = OpLoad %v2uint %index
// CHECK-NEXT:   [[img:%\d+]] = OpLoad %type_2d_image %MyTexture
// CHECK-NEXT:   [[vec:%\d+]] = OpImageRead %v4float [[img]] [[index]] None
// CHECK-NEXT:   [[val:%\d+]] = OpCompositeExtract %float [[vec]] 0
// CHECK-NEXT:   [[inc:%\d+]] = OpFAdd %float [[val]] %float_1
// CHECK:      [[index:%\d+]] = OpLoad %v2uint %index
// CHECK-NEXT:   [[img:%\d+]] = OpLoad %type_2d_image %MyTexture
// CHECK-NEXT:                  OpImageWrite [[img]] [[index]] [[inc]]
// CHECK-NEXT:                  OpStore %s [[inc]]
  float s = ++MyTexture[index];

// CHECK:      [[img:%\d+]] = OpLoad %type_buffer_image %intbuf
// CHECK-NEXT: [[vec:%\d+]] = OpImageRead %v4int [[img]] %uint_1 None
// CHECK-NEXT: [[val:%\d+]] = OpCompositeExtract %int [[vec]] 0
// CHECK-NEXT: [[inc:%\d+]] = OpIAdd %int [[val]] %int_1
// CHECK-NEXT: [[img:%\d+]] = OpLoad %type_buffer_image %intbuf
// CHECK-NEXT:       OpImageWrite [[img]] %uint_1 [[inc]]
// CHECK-NEXT:       OpStore %t [[inc]]
  int t = ++intbuf[1];
}
