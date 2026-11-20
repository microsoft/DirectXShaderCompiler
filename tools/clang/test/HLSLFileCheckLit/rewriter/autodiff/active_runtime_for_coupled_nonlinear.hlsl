// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_coupled_nonlinear_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Multiplying carried values replays through the generic expression DAG.

// CHECK: float __dxc_ad_loop_0_primal_tape[8];
// CHECK: float __dxc_ad_loop_1_primal_tape[8];
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_1_adjoint += (__dxc_ad_loop_0_update_0_adjoint * __dxc_ad_loop_0_primal_tape[iteration]);

// Starting from (2,3), two iterations produce (24,5), so the result is 29.
// The total Jacobian is [[12,14],[0,1]], and seed 2 gives (24,30).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 2.900000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.400000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 3.000000e+01

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x *= y;
    y += 1.0f;
  }
  return x + y;
}
