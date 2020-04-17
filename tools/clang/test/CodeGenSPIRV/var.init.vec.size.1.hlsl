// Run: %dxc -T cs_6_0 -E main

// CHECK:       [[ext:%\d+]] = OpExtInstImport "GLSL.std.450"

struct S1 {
  float value;
};

struct S2 {
  float1 value;
};

struct S3 {
  int value;
};

struct S4 {
  int1 value;
};

struct S5 {
  bool value;
};

struct S6 {
  bool1 value;
};

[numthreads(1,1,1)]
void main() {
  int1 vi;
  float1 vf;
  bool1 vb;

// CHECK:      [[vi:%\d+]] = OpLoad %int %vi
// CHECK-NEXT:  [[x:%\d+]] = OpConvertSToF %float [[vi]]
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S1 [[x]]
// CHECK-NEXT:               OpStore %a1 [[s]]
  S1 a1 = { vi };

// CHECK-NEXT: [[vf:%\d+]] = OpLoad %float %vf
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S1 [[vf]]
// CHECK-NEXT:               OpStore %b1 [[s]]
  S1 b1 = { vf };

// CHECK-NEXT: [[vb:%\d+]] = OpLoad %bool %vb
// CHECK-NEXT:  [[x:%\d+]] = OpSelect %float [[vb]] %float_1 %float_0
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S1 [[x]]
// CHECK-NEXT:               OpStore %c1 [[s]]
  S1 c1 = { vb };

// CHECK-NEXT:   [[vi:%\d+]] = OpLoad %int %vi
// CHECK-NEXT:    [[u:%\d+]] = OpBitcast %uint [[vi]]
// CHECK-NEXT: [[half:%\d+]] = OpExtInst %v2float [[ext]] UnpackHalf2x16 [[u]]
// CHECK-NEXT:    [[x:%\d+]] = OpCompositeExtract %float [[half]] 0
// CHECK-NEXT:    [[s:%\d+]] = OpCompositeConstruct %S1 [[x]]
// CHECK-NEXT:                 OpStore %d1 [[s]]
  S1 d1 = { half1(f16tof32(vi)) };

// CHECK-NEXT: [[vi:%\d+]] = OpLoad %int %vi
// CHECK-NEXT:  [[x:%\d+]] = OpConvertSToF %float [[vi]]
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S2 [[x]]
// CHECK-NEXT:               OpStore %a2 [[s]]
  S2 a2 = { vi };

// CHECK-NEXT: [[vf:%\d+]] = OpLoad %float %vf
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S2 [[vf]]
// CHECK-NEXT:               OpStore %b2 [[s]]
  S2 b2 = { vf };

// CHECK-NEXT: [[vb:%\d+]] = OpLoad %bool %vb
// CHECK-NEXT:  [[x:%\d+]] = OpSelect %float [[vb]] %float_1 %float_0
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S2 [[x]]
// CHECK-NEXT:               OpStore %c2 [[s]]
  S2 c2 = { vb };

// CHECK-NEXT:   [[vi:%\d+]] = OpLoad %int %vi
// CHECK-NEXT:    [[u:%\d+]] = OpBitcast %uint [[vi]]
// CHECK-NEXT: [[half:%\d+]] = OpExtInst %v2float [[ext]] UnpackHalf2x16 [[u]]
// CHECK-NEXT:    [[x:%\d+]] = OpCompositeExtract %float [[half]] 0
// CHECK-NEXT:    [[s:%\d+]] = OpCompositeConstruct %S2 [[x]]
// CHECK-NEXT:                 OpStore %d2 [[s]]
  S2 d2 = { half1(f16tof32(vi)) };

// CHECK-NEXT: [[vi:%\d+]] = OpLoad %int %vi
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S3 [[vi]]
// CHECK-NEXT:               OpStore %a3 [[s]]
  S3 a3 = { vi };

// CHECK-NEXT: [[vf:%\d+]] = OpLoad %float %vf
// CHECK-NEXT:  [[x:%\d+]] = OpConvertFToS %int [[vf]]
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S3 [[x]]
// CHECK-NEXT:               OpStore %b3 [[s]]
  S3 b3 = { vf };

// CHECK-NEXT: [[vb:%\d+]] = OpLoad %bool %vb
// CHECK-NEXT:  [[x:%\d+]] = OpSelect %int [[vb]] %int_1 %int_0
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S3 [[x]]
// CHECK-NEXT:               OpStore %c3 [[s]]
  S3 c3 = { vb };

// CHECK-NEXT:   [[vi:%\d+]] = OpLoad %int %vi
// CHECK-NEXT:    [[u:%\d+]] = OpBitcast %uint [[vi]]
// CHECK-NEXT: [[half:%\d+]] = OpExtInst %v2float [[ext]] UnpackHalf2x16 [[u]]
// CHECK-NEXT:    [[f:%\d+]] = OpCompositeExtract %float [[half]] 0
// CHECK-NEXT:    [[x:%\d+]] = OpConvertFToS %int [[f]]
// CHECK-NEXT:    [[s:%\d+]] = OpCompositeConstruct %S3 [[x]]
// CHECK-NEXT:                 OpStore %d3 [[s]]
  S3 d3 = { half1(f16tof32(vi)) };

// CHECK-NEXT: [[vi:%\d+]] = OpLoad %int %vi
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S4 [[vi]]
// CHECK-NEXT:               OpStore %a4 [[s]]
  S4 a4 = { vi };

// CHECK-NEXT: [[vf:%\d+]] = OpLoad %float %vf
// CHECK-NEXT:  [[x:%\d+]] = OpConvertFToS %int [[vf]]
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S4 [[x]]
// CHECK-NEXT:               OpStore %b4 [[s]]
  S4 b4 = { vf };

// CHECK-NEXT: [[vb:%\d+]] = OpLoad %bool %vb
// CHECK-NEXT:  [[x:%\d+]] = OpSelect %int [[vb]] %int_1 %int_0
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S4 [[x]]
// CHECK-NEXT:               OpStore %c4 [[s]]
  S4 c4 = { vb };

// CHECK-NEXT:   [[vi:%\d+]] = OpLoad %int %vi
// CHECK-NEXT:    [[u:%\d+]] = OpBitcast %uint [[vi]]
// CHECK-NEXT: [[half:%\d+]] = OpExtInst %v2float [[ext]] UnpackHalf2x16 [[u]]
// CHECK-NEXT:    [[f:%\d+]] = OpCompositeExtract %float [[half]] 0
// CHECK-NEXT:    [[x:%\d+]] = OpConvertFToS %int [[f]]
// CHECK-NEXT:    [[s:%\d+]] = OpCompositeConstruct %S4 [[x]]
// CHECK-NEXT:                 OpStore %d4 [[s]]
  S4 d4 = { half1(f16tof32(vi)) };

// CHECK-NEXT: [[vi:%\d+]] = OpLoad %int %vi
// CHECK-NEXT:  [[x:%\d+]] = OpINotEqual %bool [[vi]] %int_0
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S5 [[x]]
// CHECK-NEXT:               OpStore %a5 [[s]]
  S5 a5 = { vi };

// CHECK-NEXT: [[vf:%\d+]] = OpLoad %float %vf
// CHECK-NEXT:  [[x:%\d+]] = OpFOrdNotEqual %bool [[vf]] %float_0
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S5 [[x]]
// CHECK-NEXT:               OpStore %b5 [[s]]
  S5 b5 = { vf };

// CHECK-NEXT: [[vb:%\d+]] = OpLoad %bool %vb
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S5 [[vb]]
// CHECK-NEXT:               OpStore %c5 [[s]]
  S5 c5 = { vb };

// CHECK-NEXT:   [[vi:%\d+]] = OpLoad %int %vi
// CHECK-NEXT:    [[u:%\d+]] = OpBitcast %uint [[vi]]
// CHECK-NEXT: [[half:%\d+]] = OpExtInst %v2float [[ext]] UnpackHalf2x16 [[u]]
// CHECK-NEXT:    [[f:%\d+]] = OpCompositeExtract %float [[half]] 0
// CHECK-NEXT:    [[x:%\d+]] = OpFOrdNotEqual %bool [[f]] %float_0
// CHECK-NEXT:    [[s:%\d+]] = OpCompositeConstruct %S5 [[x]]
// CHECK-NEXT:                 OpStore %d5 [[s]]
  S5 d5 = { half1(f16tof32(vi)) };

// CHECK-NEXT: [[vi:%\d+]] = OpLoad %int %vi
// CHECK-NEXT:  [[x:%\d+]] = OpINotEqual %bool [[vi]] %int_0
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S6 [[x]]
// CHECK-NEXT:               OpStore %a6 [[s]]
  S6 a6 = { vi };

// CHECK-NEXT: [[vf:%\d+]] = OpLoad %float %vf
// CHECK-NEXT:  [[x:%\d+]] = OpFOrdNotEqual %bool [[vf]] %float_0
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S6 [[x]]
// CHECK-NEXT:               OpStore %b6 [[s]]
  S6 b6 = { vf };

// CHECK-NEXT: [[vb:%\d+]] = OpLoad %bool %vb
// CHECK-NEXT:  [[s:%\d+]] = OpCompositeConstruct %S6 [[vb]]
// CHECK-NEXT:               OpStore %c6 [[s]]
  S6 c6 = { vb };

// CHECK-NEXT:   [[vi:%\d+]] = OpLoad %int %vi
// CHECK-NEXT:    [[u:%\d+]] = OpBitcast %uint [[vi]]
// CHECK-NEXT: [[half:%\d+]] = OpExtInst %v2float [[ext]] UnpackHalf2x16 [[u]]
// CHECK-NEXT:    [[f:%\d+]] = OpCompositeExtract %float [[half]] 0
// CHECK-NEXT:    [[x:%\d+]] = OpFOrdNotEqual %bool [[f]] %float_0
// CHECK-NEXT:    [[s:%\d+]] = OpCompositeConstruct %S6 [[x]]
// CHECK-NEXT:                 OpStore %d6 [[s]]
  S6 d6 = { half1(f16tof32(vi)) };
}
