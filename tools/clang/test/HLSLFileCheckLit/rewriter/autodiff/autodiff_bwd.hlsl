// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/autodiff_bwd_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E main -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Backward-only autodiff evaluates its expression graph, returns the primal,
// and leaves parameter gradients in the caller-provided GradientContext.

// CHECK: float f(float x)
// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: VariableExpr<float> x_expr = makeVariableExpr<float>(x);
// CHECK: return compute_gradients_seeded(context, add<float>(multiply<float>(x_expr, x_expr), x_expr), __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

// f(2) = 6 and df/dx = 2x + 1 = 5.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 5.000000e+00

[[dxc::AutoDiff(bwd)]]
float f(float x) {
  return x * x + x;
}
