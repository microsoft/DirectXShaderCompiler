// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_product_bilinear_chain_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Independent linear terms replay through the same generic expression DAG.

// CHECK: x.value = (((((scale * x.value) * y.value) + (targetSlope * x.value)) + (peerSlope * y.value)) + bias);
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_1_adjoint += (__dxc_ad_loop_0_update_0_adjoint * (scale * __dxc_ad_loop_0_primal_tape[iteration]));
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_0_adjoint * targetSlope);

// Starting at (2,3,4), two iterations produce (25.75,15,16), returning 56.75.
// With seed 2, reverse Jacobian replay produces (16,18.25,20.25).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 5.675000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.600000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 1.825000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 2.025000e+01

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float scale, [[dxc::NoDiff]] float targetSlope,
        [[dxc::NoDiff]] float peerSlope, [[dxc::NoDiff]] float bias,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x = scale * x * y + targetSlope * x + peerSlope * y + bias;
    y += z;
    z *= factor;
  }
  return x + y + z;
}
