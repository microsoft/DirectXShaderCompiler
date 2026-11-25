// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/custom_backward_derivative_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// CHECK: float __dxc_ad_custom_call_0_arg_0_adjoint = (float)0;
// CHECK: ::bwd_square(x.value, __dxc_ad_seed, __dxc_ad_custom_call_0_arg_0_adjoint);
// CHECK: context.gradients[x.id] += __dxc_ad_custom_call_0_arg_0_adjoint;
// CHECK: __dxc_ad_loop_0_primal_tape[3]
// CHECK: ::bwd_square(__dxc_ad_loop_0_primal_tape[iteration], __dxc_ad_loop_0_update_0_adjoint, __dxc_ad_custom_call_0_arg_0_adjoint);
// CHECK: float2 __dxc_ad_custom_call_0_arg_0_adjoint = (float2)0;
// CHECK: ::bwd_twist(x.value, __dxc_ad_seed, __dxc_ad_custom_call_0_arg_0_adjoint);
// CHECK: ::bwd_late(x.value, __dxc_ad_seed, __dxc_ad_custom_call_0_arg_0_adjoint);
// CHECK: ::bwd_overloaded(x.value, __dxc_ad_seed, __dxc_ad_custom_call_0_arg_0_adjoint);
// CHECK: ::bwd_pair(x.value, y.value, __dxc_ad_seed, __dxc_ad_custom_call_0_arg_0_adjoint, __dxc_ad_custom_call_0_arg_1_adjoint);

// The custom pullback returns 5*x*seed. At x=2 and seed=3, the gradient is 30.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 3.000000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 2.560000e+02
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 4.800000e+04
// EXEC: rawBufferStore.f32{{.*}}i32 4, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 5, i32 0, float 9.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 6, i32 0, float 1.400000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 7, i32 0, float 1.500000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 8, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 9, i32 0, float 3.300000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 10, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 11, i32 0, float 3.900000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 12, i32 0, float 5.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 13, i32 0, float 5.100000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 14, i32 0, float 5.700000e+01

void bwd_square(float x, float dResult, out float dX) {
  dX = 5.0f * x * dResult;
}

[[dxc::BackwardDerivative(bwd_square)]]
float square(float x) { return x * x; }

[[dxc::AutoDiff(bwd)]]
float f(float x) { return square(x); }

[[dxc::AutoDiff(bwd)]]
float loop(float x, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < min(count, 3); ++iteration)
    x = square(x);
  return x;
}

void bwd_twist(float2 x, float2 dResult, out float2 dX) {
  dX = float2(2.0f * dResult.y, 3.0f * dResult.x);
}

[[dxc::BackwardDerivative(bwd_twist)]]
float2 twist(float2 x) { return x * x; }

[[dxc::AutoDiff(bwd)]]
float2 vector_f(float2 x) { return twist(x); }

[[dxc::BackwardDerivative(bwd_late)]]
float late(float x) { return 2.0f * x; }

void bwd_late(float x, float dResult, out float dX) {
  dX = 11.0f * dResult;
}

[[dxc::AutoDiff(bwd)]]
float late_f(float x) { return late(x); }

void bwd_overloaded(float2 x, float2 dResult, out float2 dX) {
  dX = dResult;
}

void bwd_overloaded(float x, float dResult, out float dX) {
  dX = 13.0f * dResult;
}

[[dxc::BackwardDerivative(bwd_overloaded)]]
float overloaded(float x) { return x * x; }

[[dxc::AutoDiff(bwd)]]
float overloaded_f(float x) { return overloaded(x); }

void bwd_pair(float x, float y, float dResult, out float dX, out float dY) {
  dX = 17.0f * dResult;
  dY = 19.0f * dResult;
}

[[dxc::BackwardDerivative(bwd_pair)]]
float pair(float x, float y) { return x + y; }

[[dxc::AutoDiff(bwd)]]
float pair_f(float x, float y) { return pair(x, y); }
