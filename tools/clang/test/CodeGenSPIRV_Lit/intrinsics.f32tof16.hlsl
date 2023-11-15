// RUN: %dxc -T vs_6_0 -E main

// CHECK:      [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main(out uint  a : A, out uint2  b : B, out uint3  c : C, out uint4  d : D,
              float m : M,     float2 n : N,     float3 o : O,     float4 p : P) {
// CHECK:        [[m:%\d+]] = OpLoad %float %m
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeConstruct %v2float [[m]] %float_0
// CHECK-NEXT:   [[a:%\d+]] = OpExtInst %uint [[glsl]] PackHalf2x16 [[vec]]
// CHECK-NEXT:                OpStore %a [[a]]
    a = f32tof16(m);

// CHECK-NEXT:   [[n:%\d+]] = OpLoad %v2float %n

// CHECK-NEXT:  [[n0:%\d+]] = OpCompositeExtract %float [[n]] 0
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeConstruct %v2float [[n0]] %float_0
// CHECK-NEXT:  [[b0:%\d+]] = OpExtInst %uint [[glsl]] PackHalf2x16 [[vec]]

// CHECK-NEXT:  [[n1:%\d+]] = OpCompositeExtract %float [[n]] 1
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeConstruct %v2float [[n1]] %float_0
// CHECK-NEXT:  [[b1:%\d+]] = OpExtInst %uint [[glsl]] PackHalf2x16 [[vec]]

// CHECK-NEXT:   [[b:%\d+]] = OpCompositeConstruct %v2uint [[b0]] [[b1]]
// CHECK-NEXT:                OpStore %b [[b]]
    b = f32tof16(n);

// CHECK-NEXT:   [[o:%\d+]] = OpLoad %v3float %o

// CHECK-NEXT:  [[o0:%\d+]] = OpCompositeExtract %float [[o]] 0
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeConstruct %v2float [[o0]] %float_0
// CHECK-NEXT:  [[c0:%\d+]] = OpExtInst %uint [[glsl]] PackHalf2x16 [[vec]]

// CHECK-NEXT:  [[o1:%\d+]] = OpCompositeExtract %float [[o]] 1
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeConstruct %v2float [[o1]] %float_0
// CHECK-NEXT:  [[c1:%\d+]] = OpExtInst %uint [[glsl]] PackHalf2x16 [[vec]]

// CHECK-NEXT:  [[o2:%\d+]] = OpCompositeExtract %float [[o]] 2
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeConstruct %v2float [[o2]] %float_0
// CHECK-NEXT:  [[c2:%\d+]] = OpExtInst %uint [[glsl]] PackHalf2x16 [[vec]]

// CHECK-NEXT:   [[c:%\d+]] = OpCompositeConstruct %v3uint [[c0]] [[c1]] [[c2]]
// CHECK-NEXT:                OpStore %c [[c]]
    c = f32tof16(o);

// CHECK-NEXT:   [[p:%\d+]] = OpLoad %v4float %p

// CHECK-NEXT:  [[p0:%\d+]] = OpCompositeExtract %float [[p]] 0
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeConstruct %v2float [[p0]] %float_0
// CHECK-NEXT:  [[d0:%\d+]] = OpExtInst %uint [[glsl]] PackHalf2x16 [[vec]]

// CHECK-NEXT:  [[p1:%\d+]] = OpCompositeExtract %float [[p]] 1
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeConstruct %v2float [[p1]] %float_0
// CHECK-NEXT:  [[d1:%\d+]] = OpExtInst %uint [[glsl]] PackHalf2x16 [[vec]]

// CHECK-NEXT:  [[p2:%\d+]] = OpCompositeExtract %float [[p]] 2
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeConstruct %v2float [[p2]] %float_0
// CHECK-NEXT:  [[d2:%\d+]] = OpExtInst %uint [[glsl]] PackHalf2x16 [[vec]]

// CHECK-NEXT:  [[p3:%\d+]] = OpCompositeExtract %float [[p]] 3
// CHECK-NEXT: [[vec:%\d+]] = OpCompositeConstruct %v2float [[p3]] %float_0
// CHECK-NEXT:  [[d3:%\d+]] = OpExtInst %uint [[glsl]] PackHalf2x16 [[vec]]

// CHECK-NEXT:   [[d:%\d+]] = OpCompositeConstruct %v4uint [[d0]] [[d1]] [[d2]] [[d3]]
// CHECK-NEXT:                OpStore %d [[d]]
    d = f32tof16(p);
}
