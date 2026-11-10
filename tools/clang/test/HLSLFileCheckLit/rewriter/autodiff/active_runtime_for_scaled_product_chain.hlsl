// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_scaled_product_chain_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// A nested inactive coefficient replays through the generic expression DAG.

// CHECK: x.value = (((scale * x.value) * y.value) + bias);
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_1_adjoint += (__dxc_ad_loop_0_update_0_adjoint * (scale * __dxc_ad_loop_0_primal_tape[iteration]));

// Starting at (2,3,4), two iterations produce (15,15,16), returning 46.
// With seed 2, reverse Jacobian replay produces gradients (10.5,13,18).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 4.600000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.050000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 1.300000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 1.800000e+01

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float scale, [[dxc::NoDiff]] float bias,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x = scale * x * y + bias;
    y += z;
    z *= factor;
  }
  return x + y + z;
}
