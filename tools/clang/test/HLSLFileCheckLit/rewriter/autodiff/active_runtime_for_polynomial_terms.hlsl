// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_polynomial_terms_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// A variable-length polynomial replays through one generic expression DAG.

// CHECK: x.value = ((((x.value * x.value) * y.value) + ((x.value * y.value) * y.value)) + (((x.value * x.value) * y.value) * y.value));
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_1_adjoint += (__dxc_ad_loop_0_update_0_adjoint * (__dxc_ad_loop_0_primal_tape[iteration] * __dxc_ad_loop_0_primal_tape[iteration]));

// Starting at (1,1,1), two iterations produce (66,4,4), returning 74.
// With seed 2, reverse Jacobian replay produces gradients (400,516,128).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 7.400000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 4.000000e+02
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 5.160000e+02
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 1.280000e+02

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x = x * x * y + x * y * y + x * x * y * y;
    y += z;
    z *= factor;
  }
  return x + y + z;
}
