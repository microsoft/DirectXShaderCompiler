// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/local_decls_bwd_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// A local reference captures the current immutable binding. Reassigning the
// source local later must not change the expression held by `snapshot`.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x, Value<float> y)
// CHECK: Value<float> a = x;
// CHECK: Value<float> snapshot = a;
// CHECK: a = y;
// CHECK: return (snapshot * a);
// CHECK: } } } // namespace user::ad::fwd

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, Variable<float> y, float __dxc_ad_seed)
// CHECK: return compute_gradients_seeded(context, multiply<float>(x_expr, y_expr), __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

// At x=2 and y=3: f=xy, df/dx=y, and df/dy=x.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 3.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 2.000000e+00

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x, float y) {
  float a = x;
  float snapshot = a;
  a = y;
  return snapshot * a;
}
