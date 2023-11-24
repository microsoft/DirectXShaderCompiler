// RUN: %dxc -T ps_6_2 -E main -enable-16bit-types

// CHECK: [[ext:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
// CHECK:      [[a:%\d+]] = OpLoad %bool %a
// CHECK-NEXT: [[b:%\d+]] = OpSelect %half [[a]] %half_0x1p_0 %half_0x0p_0
// CHECK-NEXT:              OpStore %b [[b]]
  bool a;
  half b = a;

// CHECK:      [[c:%\d+]] = OpLoad %v2bool %c
// CHECK-NEXT: [[d:%\d+]] = OpSelect %v2half [[c]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:              OpStore %d [[d]]
  bool2 c;
  half2 d = c;

// CHECK:      [[d:%\d+]] = OpLoad %v2half %d
// CHECK-NEXT: [[e:%\d+]] = OpExtInst %v2half [[ext]] FClamp [[d]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:              OpStore %e [[e]]
  half2 e = saturate(d);

// CHECK:      [[b:%\d+]] = OpLoad %half %b
// CHECK-NEXT: [[f:%\d+]] = OpExtInst %half [[ext]] FClamp [[b]] %half_0x0p_0 %half_0x1p_0
// CHECK-NEXT:              OpStore %f [[f]]
  half f = saturate(b);

// CHECK:      [[a:%\d+]] = OpLoad %bool %a
// CHECK-NEXT: [[x:%\d+]] = OpSelect %float [[a]] %float_1 %float_0
// CHECK-NEXT: [[y:%\d+]] = OpExtInst %float [[ext]] FClamp [[x]] %float_0 %float_1
// CHECK-NEXT: [[g:%\d+]] = OpFConvert %half [[y]]
// CHECK-NEXT:              OpStore %g [[g]]
  half g = (half)saturate(a);

// CHECK:      [[h:%\d+]] = OpLoad %v2int %h
// CHECK-NEXT: [[x:%\d+]] = OpConvertSToF %v2float [[h]]
// CHECK-NEXT: [[y:%\d+]] = OpExtInst %v2float [[ext]] FClamp [[x]] {{%\d+}} {{%\d+}}
// CHECK-NEXT: [[i:%\d+]] = OpFConvert %v2half [[y]]
// CHECK-NEXT:              OpStore %i [[i]]
  int2 h;
  half2 i = (half2)saturate(h);

// CHECK:      [[j:%\d+]] = OpLoad %v2float %j
// CHECK-NEXT: [[x:%\d+]] = OpExtInst %v2float [[ext]] FClamp [[j]] {{%\d+}} {{%\d+}}
// CHECK-NEXT: [[k:%\d+]] = OpFConvert %v2half [[x]]
// CHECK-NEXT:              OpStore %k [[k]]
  float2 j;
  half2 k = (half2)saturate(j);
}
