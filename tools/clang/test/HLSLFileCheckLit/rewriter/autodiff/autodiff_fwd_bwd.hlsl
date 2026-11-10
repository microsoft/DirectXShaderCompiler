// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Combined forward and backward autodiff: both variants are emitted in
// their respective namespaces, and the unary math intrinsics sin/cos/exp
// are translated correctly.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x)
// CHECK: return ((sin(x) * cos(x)) + exp(x));
// CHECK: } } } // namespace user::ad::fwd
// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: VariableExpr<float> x_expr = makeVariableExpr<float>(x);
// CHECK: return compute_gradients_seeded(context, add<float>(multiply<float>(sinExpr<float>(x_expr), cosExpr<float>(x_expr)), expExpr<float>(x_expr)), __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x) {
  return sin(x) * cos(x) + exp(x);
}

float main(float x : A) : SV_Target { return f(x); }
