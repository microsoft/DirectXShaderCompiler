// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/compound_assign_bwd_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC
//
// [[dxc::NoDiff]] applied to a parenthesised sub-expression. The attribute
// must propagate through ParenExpr / ImplicitCastExpr wrappers so the
// rewriter still recognises the marked node as no-diff. Here the entire
// `(sin(x) * cos(x))` is preserved verbatim instead of being rewritten to
// the backward-mode builder chain.

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: return compute_gradients_seeded(context, add<float>(x_expr, (sin(x.value) * cos(x.value))), __dxc_ad_seed);
// CHECK-NOT: multiply<float>(sinExpr
// CHECK: } } } // namespace user::ad::bwd

// The trigonometric product contributes to the primal but not the gradient.
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.000000e+00

[[dxc::AutoDiff(bwd)]]
float f(float x) {
  return x + [[dxc::NoDiff]] (sin(x) * cos(x));
}

float main(float x : A) : SV_Target { return f(x); }
