// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Compound assignments are normalized to explicit immutable binding updates.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x)
// CHECK: Value<float> a = x;
// CHECK: a = (a + x);
// CHECK: a = (a - x);
// CHECK: a = (a * x);
// CHECK: a = (a / x);
// CHECK: return a;
// CHECK: } } } // namespace user::ad::fwd

[[dxc::AutoDiff(fwd)]]
float f(float x) {
  float a = x;
  a += x;
  a -= x;
  a *= x;
  a /= x;
  return a;
}

float main(float x : A) : SV_Target { return f(x); }
