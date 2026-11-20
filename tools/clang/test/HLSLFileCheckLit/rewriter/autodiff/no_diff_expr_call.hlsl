// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// [[dxc::NoDiff]] applied to a sub-expression inside a larger expression.
// Only the marked sub-expression (the floor() call) should be copied
// verbatim; the surrounding subtraction must still be rewritten to the
// backward-mode `subtract<float>` builder.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> frac_NoDiff(Value<float> uv)
// CHECK: return (uv - Value<float>::CreateValue(floor(uv.value)));
// CHECK: } } } // namespace user::ad::fwd
// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float frac_NoDiff(inout GradientContext<float> context, Variable<float> uv, float __dxc_ad_seed)
// CHECK: VariableExpr<float> uv_expr = makeVariableExpr<float>(uv);
// CHECK: return compute_gradients_seeded(context, subtract<float>(uv_expr, floor(uv.value)), __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

[[dxc::AutoDiff(fwd, bwd)]]
float frac_NoDiff(float uv) {
  return uv - [[dxc::NoDiff]] floor(uv);
}

float main(float uv : A) : SV_Target { return frac_NoDiff(uv); }
