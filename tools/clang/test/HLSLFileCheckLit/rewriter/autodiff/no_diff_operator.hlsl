// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// [[dxc::NoDiff]] applied to an expression statement built from arithmetic
// operators. The operator expression is copied verbatim instead of being
// translated to add/multiply/etc. builders.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x, Value<float> y)
// CHECK: Value<float> a;
// CHECK: a = Value<float>::CreateValue(((x.value * y.value) + x.value));
// CHECK: return (Value<float>::CreateValue(a.value) + x);
// CHECK: } } } // namespace user::ad::fwd
// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, Variable<float> y, float __dxc_ad_seed)
// CHECK: VariableExpr<float> x_expr = makeVariableExpr<float>(x);
// CHECK: VariableExpr<float> y_expr = makeVariableExpr<float>(y);
// CHECK: return compute_gradients_seeded(context, add<float>(((x.value * y.value) + x.value), x_expr), __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x, float y) {
  float a;
  [[dxc::NoDiff]] a = x * y + x;
  return a + x;
}

float main(float x : A) : SV_Target { return f(x, x); }
