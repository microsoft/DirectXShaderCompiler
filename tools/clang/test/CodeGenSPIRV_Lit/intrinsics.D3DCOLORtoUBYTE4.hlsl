// RUN: %dxc -T vs_6_0 -E main

// CHECK: %float_255_001999 = OpConstant %float 255.001999

void main() {
  float4 input;

// CHECK:         [[input:%\d+]] = OpLoad %v4float %input
// CHECK-NEXT: [[swizzled:%\d+]] = OpVectorShuffle %v4float [[input]] [[input]] 2 1 0 3
// CHECK-NEXT:   [[scaled:%\d+]] = OpVectorTimesScalar %v4float [[swizzled]] %float_255_001999
// CHECK-NEXT:          {{%\d+}} = OpConvertFToS %v4int [[scaled]]
  int4 result = D3DCOLORtoUBYTE4(input);
}
