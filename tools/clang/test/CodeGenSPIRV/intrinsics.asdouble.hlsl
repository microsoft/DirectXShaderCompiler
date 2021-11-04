// RUN: %dxc -T vs_6_0 -E main

// Signatures:
// double  asdouble(uint lowbits, uint highbits)
// double2 asdouble(uint2 lowbits, uint2 highbits)

void main() {

// CHECK:      [[arg:%\d+]] = OpCompositeConstruct %v2uint %uint_1 %uint_2
// CHECK-NEXT:     {{%\d+}} = OpBitcast %double [[arg]]
  double a = asdouble(1u, 2u);

  uint low, high;
// CHECK:        [[low:%\d+]] = OpLoad %uint %low
// CHECK-NEXT:  [[high:%\d+]] = OpLoad %uint %high
// CHECK-NEXT:  [[arg2:%\d+]] = OpCompositeConstruct %v2uint [[low]] [[high]]
// CHECK-NEXT:       {{%\d+}} = OpBitcast %double [[arg2]]
  double b = asdouble(low, high);

// CHECK:         [[low2:%\d+]] = OpLoad %v2uint %low2
// CHECK-NEXT:   [[high2:%\d+]] = OpLoad %v2uint %high2
// CHECK-NEXT:    [[arg3:%\d+]] = OpVectorShuffle %v4uint [[low2]] [[high2]] 0 2 1 3
// CHECK-NEXT:         {{%\d+}} = OpBitcast %v2double [[arg3]]
  uint2 low2, high2;
  double2 c = asdouble(low2, high2);
}
