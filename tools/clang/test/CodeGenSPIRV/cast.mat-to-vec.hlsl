// RUN: %dxc -T ps_6_0 -E main

float4 main(float4 input : A) : SV_Target {
  float2x2 floatMat;
  int2x2   intMat;
  bool2x2  boolMat;

// CHECK:      [[floatMat:%\d+]] = OpLoad %mat2v2float %floatMat
// CHECK-NEXT:     [[row0:%\d+]] = OpCompositeExtract %v2float [[floatMat]] 0
// CHECK-NEXT:     [[row1:%\d+]] = OpCompositeExtract %v2float [[floatMat]] 1
// CHECK-NEXT:      [[vec:%\d+]] = OpVectorShuffle %v4float [[row0]] [[row1]] 0 1 2 3
// CHECK-NEXT:                     OpStore %c [[vec]]
  float4 c = floatMat;

// CHECK:        [[intMat:%\d+]] = OpLoad %_arr_v2int_uint_2 %intMat
// CHECK-NEXT:     [[row0:%\d+]] = OpCompositeExtract %v2int [[intMat]] 0
// CHECK-NEXT:     [[row1:%\d+]] = OpCompositeExtract %v2int [[intMat]] 1
// CHECK-NEXT:   [[vecInt:%\d+]] = OpVectorShuffle %v4int [[row0]] [[row1]] 0 1 2 3
// CHECK-NEXT: [[vecFloat:%\d+]] = OpConvertSToF %v4float [[vecInt]]
// CHECK-NEXT:                     OpStore %d [[vecFloat]]
  float4 d = intMat;

// CHECK:       [[boolMat:%\d+]] = OpLoad %_arr_v2bool_uint_2 %boolMat
// CHECK-NEXT:     [[row0:%\d+]] = OpCompositeExtract %v2bool [[boolMat]] 0
// CHECK-NEXT:     [[row1:%\d+]] = OpCompositeExtract %v2bool [[boolMat]] 1
// CHECK-NEXT:      [[vec:%\d+]] = OpVectorShuffle %v4bool [[row0]] [[row1]] 0 1 2 3
// CHECK-NEXT: [[vecFloat:%\d+]] = OpSelect %v4float [[vec]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                     OpStore %e [[vecFloat]]
  float4 e = boolMat;

  return 0.xxxx;
}

