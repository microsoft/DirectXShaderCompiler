// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_polynomial_chain_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// A signed sum of mixed monomials replays through the generic expression DAG.

// CHECK: x.value = (((x.value * x.value) * y.value) + ((x.value * y.value) * y.value));
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_1_adjoint += (__dxc_ad_loop_0_update_0_adjoint * (__dxc_ad_loop_0_primal_tape[iteration] * __dxc_ad_loop_0_primal_tape[iteration]));

// Starting at (1,1,1), two iterations produce (16,4,4), returning 24.
// With seed 2, reverse Jacobian replay produces gradients (72,98,38).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 2.400000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 7.200000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 9.800000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 3.800000e+01

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x = x * x * y + x * y * y;
    y += z;
    z *= factor;
  }
  return x + y + z;
}
