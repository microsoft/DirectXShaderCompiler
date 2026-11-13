// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_generic_pullback_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// The structured loop plan reverses each typed update through its local
// pullback rather than the recurrence-specific linear-chain emitter.

// CHECK: x.value -= y.value;
// CHECK: y.value += z.value;
// CHECK: z.value *= factor;
// CHECK: float __dxc_ad_loop_0_update_2_adjoint = __dxc_ad_loop_0_state_2_adjoint;
// CHECK: __dxc_ad_loop_0_state_2_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_2_adjoint += (__dxc_ad_loop_0_update_2_adjoint * factor);
// CHECK: float __dxc_ad_loop_0_update_1_adjoint = __dxc_ad_loop_0_state_1_adjoint;
// CHECK: __dxc_ad_loop_0_state_1_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_2_adjoint += __dxc_ad_loop_0_update_1_adjoint;
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_1_adjoint += -(__dxc_ad_loop_0_update_0_adjoint);

// Starting at (8,3,1), two iterations produce (1,6,4), returning 11.
// With seed 2, generic reverse replay produces gradients (2,-2,12).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.100000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float -2.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 1.200000e+01

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < count; ++iteration) {
    x -= y;
    y += z;
    z *= factor;
  }
  return x + y + z;
}
