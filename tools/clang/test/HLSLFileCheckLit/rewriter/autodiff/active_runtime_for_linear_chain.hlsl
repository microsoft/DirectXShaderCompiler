// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_linear_chain_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// A sequential active-state chain applies update adjoints in reverse statement
// order for every reverse iteration.

// CHECK: x.value += y.value;
// CHECK: y.value += z.value;
// CHECK: z.value *= factor;
// CHECK: float __dxc_ad_loop_0_update_2_adjoint = __dxc_ad_loop_0_state_2_adjoint;
// CHECK: __dxc_ad_loop_0_state_2_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_2_adjoint += (__dxc_ad_loop_0_update_2_adjoint * factor);
// CHECK: __dxc_ad_loop_0_state_2_adjoint += __dxc_ad_loop_0_update_1_adjoint;
// CHECK: __dxc_ad_loop_0_state_1_adjoint += __dxc_ad_loop_0_update_0_adjoint;

// Starting at (1,2,3), two iterations produce (8,11,12), returning 31.
// With seed 2, reverse Jacobian replay produces gradients (2,6,16).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 3.100000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 1.600000e+01

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < count; ++iteration) {
    x += y;
    y += z;
    z *= factor;
  }
  return x + y + z;
}
