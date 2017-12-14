// Run: %dxc -T vs_6_0 -E main

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main(uint      a : A,     uint2  b : B,     uint3  c : C,     uint4  d : D,
          out float m : M, out float2 n : N, out float3 o : O, out float4 p : P) {
// CHECK:        [[a:%\d+]] = OpLoad %uint %a
// CHECK-NEXT: [[cov:%\d+]] = OpExtInst %v2float [[glsl]] UnpackHalf2x16 [[a]]
// CHECK-NEXT:   [[m:%\d+]] = OpCompositeExtract %float [[cov]] 0
// CHECK-NEXT:                OpStore %m [[m]]
    m = f16tof32(a);

// CHECK:        [[b:%\d+]] = OpLoad %v2uint %b

// CHECK-NEXT:  [[b0:%\d+]] = OpCompositeExtract %uint [[b]] 0
// CHECK-NEXT: [[cov:%\d+]] = OpExtInst %v2float [[glsl]] UnpackHalf2x16 [[b0]]
// CHECK-NEXT:  [[n0:%\d+]] = OpCompositeExtract %float [[cov]] 0

// CHECK-NEXT:  [[b1:%\d+]] = OpCompositeExtract %uint [[b]] 1
// CHECK-NEXT: [[cov:%\d+]] = OpExtInst %v2float [[glsl]] UnpackHalf2x16 [[b1]]
// CHECK-NEXT:  [[n1:%\d+]] = OpCompositeExtract %float [[cov]] 0

// CHECK-NEXT:   [[n:%\d+]] = OpCompositeConstruct %v2float [[n0]] [[n1]]
// CHECK-NEXT:                OpStore %n [[n]]
    n = f16tof32(b);

// CHECK-NEXT:   [[c:%\d+]] = OpLoad %v3uint %c

// CHECK-NEXT:  [[c0:%\d+]] = OpCompositeExtract %uint [[c]] 0
// CHECK-NEXT: [[cov:%\d+]] = OpExtInst %v2float [[glsl]] UnpackHalf2x16 [[c0]]
// CHECK-NEXT:  [[o0:%\d+]] = OpCompositeExtract %float [[cov]] 0

// CHECK-NEXT:  [[c1:%\d+]] = OpCompositeExtract %uint [[c]] 1
// CHECK-NEXT: [[cov:%\d+]] = OpExtInst %v2float [[glsl]] UnpackHalf2x16 [[c1]]
// CHECK-NEXT:  [[o1:%\d+]] = OpCompositeExtract %float [[cov]] 0

// CHECK-NEXT:  [[c2:%\d+]] = OpCompositeExtract %uint [[c]] 2
// CHECK-NEXT: [[cov:%\d+]] = OpExtInst %v2float [[glsl]] UnpackHalf2x16 [[c2]]
// CHECK-NEXT:  [[o2:%\d+]] = OpCompositeExtract %float [[cov]] 0

// CHECK-NEXT:   [[o:%\d+]] = OpCompositeConstruct %v3float [[o0]] [[o1]] [[o2]]
// CHECK-NEXT:                OpStore %o [[o]]
    o = f16tof32(c);

// CHECK-NEXT:   [[d:%\d+]] = OpLoad %v4uint %d

// CHECK-NEXT:  [[d0:%\d+]] = OpCompositeExtract %uint [[d]] 0
// CHECK-NEXT: [[cov:%\d+]] = OpExtInst %v2float [[glsl]] UnpackHalf2x16 [[d0]]
// CHECK-NEXT:  [[p0:%\d+]] = OpCompositeExtract %float [[cov]] 0

// CHECK-NEXT:  [[d1:%\d+]] = OpCompositeExtract %uint [[d]] 1
// CHECK-NEXT: [[cov:%\d+]] = OpExtInst %v2float [[glsl]] UnpackHalf2x16 [[d1]]
// CHECK-NEXT:  [[p1:%\d+]] = OpCompositeExtract %float [[cov]] 0

// CHECK-NEXT:  [[d2:%\d+]] = OpCompositeExtract %uint [[d]] 2
// CHECK-NEXT: [[cov:%\d+]] = OpExtInst %v2float [[glsl]] UnpackHalf2x16 [[d2]]
// CHECK-NEXT:  [[p2:%\d+]] = OpCompositeExtract %float [[cov]] 0

// CHECK-NEXT:  [[d3:%\d+]] = OpCompositeExtract %uint [[d]] 3
// CHECK-NEXT: [[cov:%\d+]] = OpExtInst %v2float [[glsl]] UnpackHalf2x16 [[d3]]
// CHECK-NEXT:  [[p3:%\d+]] = OpCompositeExtract %float [[cov]] 0

// CHECK-NEXT:   [[p:%\d+]] = OpCompositeConstruct %v4float [[p0]] [[p1]] [[p2]] [[p3]]
// CHECK-NEXT:                OpStore %p [[p]]
    p = f16tof32(d);
}
