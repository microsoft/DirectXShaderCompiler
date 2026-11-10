// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// [[dxc::NoDiff]] applied to a statement containing a call to a user-defined
// function. The whole assignment statement is copied verbatim into the
// generated forward and backward functions, so the call is not translated
// through the auto-diff expression machinery.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x)
// CHECK: Value<float> a;
// CHECK: a = Value<float>::CreateValue(helper(x.value));
// CHECK: return (Value<float>::CreateValue(a.value) + x);
// CHECK: } } } // namespace user::ad::fwd
// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: VariableExpr<float> x_expr = makeVariableExpr<float>(x);
// CHECK: return compute_gradients_seeded(context, add<float>(helper(x.value), x_expr), __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

float helper(float x) { return x * x; }

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x) {
  float a;
  [[dxc::NoDiff]] a = helper(x);
  return a + x;
}

float main(float x : A) : SV_Target { return f(x); }
