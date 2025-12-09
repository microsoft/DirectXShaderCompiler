// RUN: %dxc -T vs_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// Signatures:
// double  asdouble(uint lowbits, uint highbits)
// double2 asdouble(uint2 lowbits, uint2 highbits)

void main() {

// CHECK:      [[arg:%[0-9]+]] = OpCompositeConstruct %v2uint %uint_1 %uint_2
// CHECK-NEXT:     {{%[0-9]+}} = OpBitcast %double [[arg]]
  double a = asdouble(1u, 2u);

  uint low, high;
// CHECK:        [[low:%[0-9]+]] = OpLoad %uint %low
// CHECK-NEXT:  [[high:%[0-9]+]] = OpLoad %uint %high
// CHECK-NEXT:  [[arg2:%[0-9]+]] = OpCompositeConstruct %v2uint [[low]] [[high]]
// CHECK-NEXT:       {{%[0-9]+}} = OpBitcast %double [[arg2]]
  double b = asdouble(low, high);

// CHECK:         [[low2:%[0-9]+]] = OpLoad %v2uint %low2
// CHECK-NEXT:   [[high2:%[0-9]+]] = OpLoad %v2uint %high2
// CHECK-NEXT:   [[lowbit1:%[0-9]+]] = OpCompositeExtract %uint [[low2]] 0
// CHECK-NEXT:   [[highbit1:%[0-9]+]] = OpCompositeExtract %uint [[high2]] 0
// CHECK-NEXT:   [[comp1:%[0-9]+]] = OpCompositeConstruct %v2uint [[lowbit1]] [[highbit1]]
// CHECK-NEXT:      [[double1:%[0-9]+]] = OpBitcast %double [[comp1]]
// CHECK-NEXT:   [[lowbit2:%[0-9]+]] = OpCompositeExtract %uint [[low2]] 1
// CHECK-NEXT:   [[highbit2:%[0-9]+]] = OpCompositeExtract %uint [[high2]] 1
// CHECK-NEXT:   [[comp2:%[0-9]+]] = OpCompositeConstruct %v2uint [[lowbit2]] [[highbit2]]
// CHECK-NEXT:      [[double2:%[0-9]+]] = OpBitcast %double [[comp2]]
// CHECK-NEXT:   {{%[0-9]+}} = OpCompositeConstruct %v2double [[double1]] [[double2]]
  uint2 low2, high2;
  double2 c = asdouble(low2, high2);

// CHECK:         [[low3:%[0-9]+]] = OpLoad %v3uint %low3
// CHECK-NEXT:   [[high3:%[0-9]+]] = OpLoad %v3uint %high3
// CHECK-NEXT:   [[lowbit1:%[0-9]+]] = OpCompositeExtract %uint [[low3]] 0
// CHECK-NEXT:   [[highbit1:%[0-9]+]] = OpCompositeExtract %uint [[high3]] 0
// CHECK-NEXT:   [[comp1:%[0-9]+]] = OpCompositeConstruct %v2uint [[lowbit1]] [[highbit1]]
// CHECK-NEXT:      [[double1:%[0-9]+]] = OpBitcast %double [[comp1]]
// CHECK-NEXT:   [[lowbit2:%[0-9]+]] = OpCompositeExtract %uint [[low3]] 1
// CHECK-NEXT:   [[highbit2:%[0-9]+]] = OpCompositeExtract %uint [[high3]] 1
// CHECK-NEXT:   [[comp2:%[0-9]+]] = OpCompositeConstruct %v2uint [[lowbit2]] [[highbit2]]
// CHECK-NEXT:      [[double2:%[0-9]+]] = OpBitcast %double [[comp2]]
// CHECK-NEXT:   [[lowbit3:%[0-9]+]] = OpCompositeExtract %uint [[low3]] 2
// CHECK-NEXT:   [[highbit3:%[0-9]+]] = OpCompositeExtract %uint [[high3]] 2
// CHECK-NEXT:   [[comp3:%[0-9]+]] = OpCompositeConstruct %v2uint [[lowbit3]] [[highbit3]]
// CHECK-NEXT:      [[double3:%[0-9]+]] = OpBitcast %double [[comp3]]
// CHECK-NEXT:   {{%[0-9]+}} = OpCompositeConstruct %v3double [[double1]] [[double2]] [[double3]]
  uint3 low3, high3;
  double3 d = asdouble(low3, high3);
}
