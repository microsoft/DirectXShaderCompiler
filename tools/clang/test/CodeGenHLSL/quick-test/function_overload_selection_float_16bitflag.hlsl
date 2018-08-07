// RUN: %dxc /Tps_6_2 -enable-16bit-types /Emain > %s | FileCheck %s
// CHECK: define void @main()
// CHECK: entry
// CHECK: %frem = frem fast float %{{[0-9]+}}, 1.000000e+01
// CHECK: %div.i = fdiv fast float %{{[0-9]+}}, %frem

float foo(float v0, float v1) { return v0 / v1; }
half foo(half v0, half v1) { return v0 * v1; }
min16float foo(min16float v0, min16float v1) { return v0 + v1; }
min10float foo(min10float v0, min10float v1) { return v0 - v1; }

[RootSignature("")]
float main(float vf
  : A, half vh
  : B, min16float vm16
  : C, min10float vm10
  : D) : SV_Target{
    return foo(vf, vf % 10.0);
}