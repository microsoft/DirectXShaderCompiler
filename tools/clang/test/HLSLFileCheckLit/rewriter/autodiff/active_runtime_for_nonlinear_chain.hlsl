// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_nonlinear_chain_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// A bounded nonlinear final state is taped while the preceding sequential
// updates replay their adjoints in reverse statement order.

// CHECK: float __dxc_ad_loop_2_primal_tape[8];
// CHECK: __dxc_ad_loop_2_primal_tape[iteration] = z.value;
// CHECK: z.value *= z.value;
// CHECK: float __dxc_ad_loop_2_update_2_adjoint = __dxc_ad_loop_2_state_2_adjoint;
// CHECK: __dxc_ad_loop_2_state_2_adjoint = (float)0;
// CHECK: __dxc_ad_loop_2_state_2_adjoint += (__dxc_ad_loop_2_update_2_adjoint * __dxc_ad_loop_2_primal_tape[iteration]);
// CHECK: __dxc_ad_loop_2_state_2_adjoint += __dxc_ad_loop_2_update_1_adjoint;
// CHECK: __dxc_ad_loop_2_state_1_adjoint += __dxc_ad_loop_2_update_0_adjoint;

// Starting at (1,2,2), two iterations produce (7,8,16), returning 31.
// With seed 2, reverse Jacobian replay produces gradients (2,6,76).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 3.100000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 7.600000e+01

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y += z;
    z *= z;
  }
  return x + y + z;
}
