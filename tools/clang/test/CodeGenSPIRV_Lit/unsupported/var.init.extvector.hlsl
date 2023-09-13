// RUN: %dxc -T cs_6_0 -E main

// CHECK:       [[ext:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
// CHECK:       [[foo:%\d+]] = OpLoad %v4uint %foo
// CHECK-NEXT:    [[i:%\d+]] = OpCompositeExtract %uint [[foo]] 0
// CHECK-NEXT: [[half:%\d+]] = OpExtInst %v2float [[ext]] UnpackHalf2x16 [[i]]
// CHECK-NEXT:    [[x:%\d+]] = OpCompositeExtract %float [[half]] 0
// CHECK-NEXT:    [[i:%\d+]] = OpCompositeExtract %uint [[foo]] 1
// CHECK-NEXT: [[half:%\d+]] = OpExtInst %v2float [[ext]] UnpackHalf2x16 [[i]]
// CHECK-NEXT:    [[y:%\d+]] = OpCompositeExtract %float [[half]] 0
// CHECK-NEXT:    [[i:%\d+]] = OpCompositeExtract %uint [[foo]] 2
// CHECK-NEXT: [[half:%\d+]] = OpExtInst %v2float [[ext]] UnpackHalf2x16 [[i]]
// CHECK-NEXT:    [[z:%\d+]] = OpCompositeExtract %float [[half]] 0
// CHECK-NEXT:    [[i:%\d+]] = OpCompositeExtract %uint [[foo]] 3
// CHECK-NEXT: [[half:%\d+]] = OpExtInst %v2float [[ext]] UnpackHalf2x16 [[i]]
// CHECK-NEXT:    [[w:%\d+]] = OpCompositeExtract %float [[half]] 0
// CHECK-NEXT:  [[bar:%\d+]] = OpCompositeConstruct %v4float [[x]] [[y]] [[z]] [[w]]
// CHECK-NEXT:                 OpStore %bar [[bar]]
  uint4 foo;
  half4 bar = half4(f16tof32(foo));
}
