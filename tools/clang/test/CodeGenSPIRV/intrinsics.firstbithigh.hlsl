// RUN: %dxc -T ps_6_0 -E main

// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  int   sint_1;
  int4  sint_4;
  uint  uint_1;
  uint4 uint_4;

// CHECK: [[sint_1:%\d+]] = OpLoad %int %sint_1
// CHECK:    [[msb:%\d+]] = OpExtInst %uint [[glsl]] FindSMsb [[sint_1]]
// CHECK:    [[res:%\d+]] = OpBitcast %int [[msb]]
// CHECK:                   OpStore %fbh [[res]]
  int fbh = firstbithigh(sint_1);

// CHECK: [[sint_4:%\d+]] = OpLoad %v4int %sint_4
// CHECK:    [[msb:%\d+]] = OpExtInst %v4uint [[glsl]] FindSMsb [[sint_4]]
// CHECK:    [[res:%\d+]] = OpBitcast %v4int [[msb]]
// CHECK:                   OpStore %fbh4 [[res]]
  int4 fbh4 = firstbithigh(sint_4);

// CHECK: [[uint_1:%\d+]] = OpLoad %uint %uint_1
// CHECK:    [[msb:%\d+]] = OpExtInst %uint [[glsl]] FindUMsb [[uint_1]]
// CHECK:                   OpStore %ufbh [[msb]]
  uint ufbh = firstbithigh(uint_1);

// CHECK: [[uint_4:%\d+]] = OpLoad %v4uint %uint_4
// CHECK:    [[msb:%\d+]] = OpExtInst %v4uint [[glsl]] FindUMsb [[uint_4]]
// CHECK:                   OpStore %ufbh4 [[msb]]
  uint4 ufbh4 = firstbithigh(uint_4);
}
