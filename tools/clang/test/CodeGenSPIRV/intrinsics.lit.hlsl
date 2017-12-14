// Run: %dxc -T vs_6_0 -E main

// The signature for 'lit' intrinsic function is as follows:
// float4 lit(float n_dot_l, float n_dot_h, float m)

// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  float n_dot_l, n_dot_h, m;
  
// CHECK:      [[n_dot_l:%\d+]] = OpLoad %float %n_dot_l
// CHECK-NEXT: [[n_dot_h:%\d+]] = OpLoad %float %n_dot_h
// CHECK-NEXT:       [[m:%\d+]] = OpLoad %float %m
// CHECK-NEXT: [[diffuse:%\d+]] = OpExtInst %float [[glsl]] FMax %float_0 [[n_dot_l]]
// CHECK-NEXT:     [[min:%\d+]] = OpExtInst %float [[glsl]] FMin [[n_dot_l]] [[n_dot_h]]
// CHECK-NEXT:  [[is_neg:%\d+]] = OpFOrdLessThan %bool [[min]] %float_0
// CHECK-NEXT:     [[mul:%\d+]] = OpFMul %float [[n_dot_h]] [[m]]
// CHECK-NEXT:[[specular:%\d+]] = OpSelect %float [[is_neg]] %float_0 [[mul]]
// CHECK-NEXT:         {{%\d+}} = OpCompositeConstruct %v4float %float_1 [[diffuse]] [[specular]] %float_1
  float4 result = lit(n_dot_l, n_dot_h, m);
}
