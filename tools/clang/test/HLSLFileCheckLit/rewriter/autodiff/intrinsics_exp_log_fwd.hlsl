// RUN: %dxr -generate-differentials %s | FileCheck %s
//
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Exponential / logarithm / power family in forward mode: the calls remain
// untransformed, since the Value<T> overloads supply the differential
// arithmetic.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x, Value<float> y)
// CHECK: exp(x)
// CHECK: exp2(x)
// CHECK: log(x)
// CHECK: log2(x)
// CHECK: log10(x)
// CHECK: pow(x, y)
// CHECK: } } } // namespace user::ad::fwd

[[dxc::AutoDiff(fwd)]]
float f(float x, float y) {
  return exp(x) + exp2(x)
       + log(x) + log2(x) + log10(x)
       + pow(x, y);
}

float main(float x : A) : SV_Target { return f(x, x); }
