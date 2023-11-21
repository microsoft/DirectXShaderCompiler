// RUN: %dxc -T ps_6_0 -E main

void main() {
  float2x2 a;
  int4 b;

// CHECK:        [[a:%\d+]] = OpLoad %mat2v2float %a
// CHECK-NEXT: [[a_0:%\d+]] = OpCompositeExtract %v2float [[a]] 0
// CHECK-NEXT: [[a_0:%\d+]] = OpConvertFToS %v2int [[a_0]]
// CHECK-NEXT: [[a_1:%\d+]] = OpCompositeExtract %v2float [[a]] 1
// CHECK-NEXT: [[a_1:%\d+]] = OpConvertFToS %v2int [[a_1]]
// CHECK-NEXT:   [[a:%\d+]] = OpCompositeConstruct %_arr_v2int_uint_2 [[a_0]] [[a_1]]
  b.zw = mul(int2x2(a), b.yx);
}
