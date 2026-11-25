// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_exp_log_sqrt_chain_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Generic local pullbacks reconstruct intrinsic derivatives from taped
// pre-update values.

// CHECK: x.value = (::exp(x.value) * y.value);
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;

// CHECK: x.value = ((::log(x.value) * y.value) + (::sqrt(x.value) * y.value));
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;

// Both cases return 13. Seed 2 gives exp gradients (4,4,6) and
// log-plus-sqrt gradients (6,4,6).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.300000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 4, i32 0, float 1.300000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 5, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 6, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 7, i32 0, float 6.000000e+00

[[dxc::AutoDiff(bwd)]]
float expRecurrence(float x, float y, float z,
                    [[dxc::NoDiff]] uint count,
                    [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x = exp(x) * y;
    y += z;
    z *= factor;
  }
  return x + y + z;
}

[[dxc::AutoDiff(bwd)]]
float logSqrtRecurrence(float x, float y, float z,
                        [[dxc::NoDiff]] uint count,
                        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x = log(x) * y + sqrt(x) * y;
    y += z;
    z *= factor;
  }
  return x + y + z;
}
