// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_product_chain_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// An interior product edge tapes both pre-update operands and applies its
// changing two-input Jacobian after reversing the later chain updates.

// CHECK: float __dxc_ad_loop_0_primal_tape[8];
// CHECK: float __dxc_ad_loop_1_primal_tape[8];
// CHECK: __dxc_ad_loop_0_primal_tape[iteration] = x.value;
// CHECK: x.value *= y.value;
// CHECK: __dxc_ad_loop_1_primal_tape[iteration] = y.value;
// CHECK: z.value *= factor;
// CHECK: float __dxc_ad_loop_0_update_2_adjoint = __dxc_ad_loop_0_state_2_adjoint;
// CHECK: __dxc_ad_loop_0_state_2_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_2_adjoint += (__dxc_ad_loop_0_update_2_adjoint * factor);
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_0_adjoint * __dxc_ad_loop_1_primal_tape[iteration]);
// CHECK: __dxc_ad_loop_0_state_1_adjoint += (__dxc_ad_loop_0_update_0_adjoint * __dxc_ad_loop_0_primal_tape[iteration]);

// Starting at (2,3,4), two iterations produce (42,15,16), returning 73.
// With seed 2, reverse Jacobian replay produces gradients (42,42,26).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 7.300000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 4.200000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 4.200000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 2.600000e+01

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x *= y;
    y += z;
    z *= factor;
  }
  return x + y + z;
}
