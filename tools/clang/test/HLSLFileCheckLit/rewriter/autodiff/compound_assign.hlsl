// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/compound_assign_bwd_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Compound assignments create immutable local binding versions. Backward
// lowering expands those versions into the final expression graph.

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK-NOT: Variable<float> a
// CHECK-NOT: Assign<float>
// CHECK: return compute_gradients_seeded(context, divide<float>(multiply<float>(subtract<float>(add<float>(x_expr, x_expr), x_expr), x_expr), x_expr), __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

// At x=2, the sequence returns x and has derivative 1.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 2.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.000000e+00

[[dxc::AutoDiff(bwd)]]
float f(float x) {
  float a = x;
  a += x;
  a -= x;
  a *= x;
  a /= x;
  return a;
}

float main(float x : A) : SV_Target { return f(x); }
