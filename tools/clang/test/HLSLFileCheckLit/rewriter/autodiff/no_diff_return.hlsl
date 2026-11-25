// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// The [[dxc::NoDiff]] statement attribute marks a statement that should NOT be
// translated by the auto-diff rewriter. Its expression is evaluated on primal
// values and contributes zero derivative.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x)
// CHECK: return Value<float>::CreateValue(((x.value * x.value) + x.value));
// CHECK: } } } // namespace user::ad::fwd
// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: context.zeroGradients();
// CHECK: return ((x.value * x.value) + x.value);
// CHECK: } } } // namespace user::ad::bwd

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x) {
  [[dxc::NoDiff]] return x * x + x;
}

float main(float x : A) : SV_Target { return f(x); }
