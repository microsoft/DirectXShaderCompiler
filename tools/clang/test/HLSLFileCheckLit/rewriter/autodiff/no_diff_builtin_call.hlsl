// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// [[dxc::NoDiff]] applied to a statement containing a call to a builtin HLSL
// intrinsic. The call is copied verbatim instead of being mapped to the
// backward-mode '*Expr' builder; this lets a user opt out of differentiating
// a known intrinsic on a per-statement basis.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x)
// CHECK: Value<float> a;
// CHECK: a = Value<float>::CreateValue(sin(x.value));
// CHECK: return (Value<float>::CreateValue(a.value) + x);
// CHECK: } } } // namespace user::ad::fwd
// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: VariableExpr<float> x_expr = makeVariableExpr<float>(x);
// CHECK: return compute_gradients_seeded(context, add<float>(sin(x.value), x_expr), __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x) {
  float a;
  [[dxc::NoDiff]] a = sin(x);
  return a + x;
}

float main(float x : A) : SV_Target { return f(x); }
