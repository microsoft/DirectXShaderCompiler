// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/no_diff_parameter_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Parameter-level NoDiff keeps x in the primal computation but prevents an
// x_expr reference from entering the backward expression graph.

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, float x, Variable<float> y, float __dxc_ad_seed)
// CHECK-NOT: VariableExpr<float> x_expr
// CHECK: VariableExpr<float> y_expr = makeVariableExpr<float>(y);
// CHECK: return compute_gradients_seeded(context, multiply<float>(x, y_expr), __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

// At x=2, y=3, and seed=2: f=6 and active dy=seed*x=4. The inactive x
// has no Variable or gradient slot in the generated ABI.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 4.000000e+00

[[dxc::AutoDiff(bwd)]]
float f([[dxc::NoDiff]] float x, float y) {
  return x * y;
}
