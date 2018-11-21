// Run: %dxc -T ps_6_0 -E main

// CHECK: [[v3float_1_1_1:%\d+]] = OpConstantComposite %v3float %float_1 %float_1 %float_1

RWTexture2D<float>  MyTexture : register(u1);
RWBuffer<int> intbuf;

void main() {
// CHECK-LABEL: %bb_entry = OpLabel
    int a, b;
// CHECK:      [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: [[a1:%\d+]] = OpISub %int [[a0]] %int_1
// CHECK-NEXT: OpStore %a [[a1]]
// CHECK-NEXT: OpStore %b [[a0]]
    b = a--;

    uint i, j;
// CHECK-NEXT: [[i0:%\d+]] = OpLoad %uint %i
// CHECK-NEXT: [[i1:%\d+]] = OpISub %uint [[i0]] %uint_1
// CHECK-NEXT: OpStore %i [[i1]]
// CHECK-NEXT: OpStore %j [[i0]]
    j = i--;

    float o, p;
// CHECK-NEXT: [[o0:%\d+]] = OpLoad %float %o
// CHECK-NEXT: [[o1:%\d+]] = OpFSub %float [[o0]] %float_1
// CHECK-NEXT: OpStore %o [[o1]]
// CHECK-NEXT: OpStore %p [[o0]]
    p = o--;

    float3 x, y;
// CHECK-NEXT: [[x0:%\d+]] = OpLoad %v3float %x
// CHECK-NEXT: [[x1:%\d+]] = OpFSub %v3float [[x0]] [[v3float_1_1_1]]
// CHECK-NEXT: OpStore %x [[x1]]
// CHECK-NEXT: OpStore %y [[x0]]
    y = x--;

  uint2 index;
// CHECK:      [[index:%\d+]] = OpLoad %v2uint %index
// CHECK-NEXT:   [[img:%\d+]] = OpLoad %type_2d_image %MyTexture
// CHECK-NEXT:   [[vec:%\d+]] = OpImageRead %v4float [[img]] [[index]] None
// CHECK-NEXT:   [[val:%\d+]] = OpCompositeExtract %float [[vec]] 0
// CHECK-NEXT:   [[dec:%\d+]] = OpFSub %float [[val]] %float_1
// CHECK:      [[index:%\d+]] = OpLoad %v2uint %index
// CHECK-NEXT:   [[img:%\d+]] = OpLoad %type_2d_image %MyTexture
// CHECK-NEXT:                  OpImageWrite [[img]] [[index]] [[dec]]
// CHECK-NEXT:                  OpStore %s [[val]]
  float s = MyTexture[index]--;

// CHECK:      [[img:%\d+]] = OpLoad %type_buffer_image %intbuf
// CHECK-NEXT: [[vec:%\d+]] = OpImageRead %v4int [[img]] %uint_1 None
// CHECK-NEXT: [[val:%\d+]] = OpCompositeExtract %int [[vec]] 0
// CHECK-NEXT: [[dec:%\d+]] = OpISub %int [[val]] %int_1
// CHECK-NEXT: [[img:%\d+]] = OpLoad %type_buffer_image %intbuf
// CHECK-NEXT:       OpImageWrite [[img]] %uint_1 [[dec]]
// CHECK-NEXT:       OpStore %t [[val]]
  int t = intbuf[1]--;
}
