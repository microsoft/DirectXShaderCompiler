// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_affine_tape_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// An inactive affine term changes the primal recurrence but not its derivative
// with respect to the carried active value.

// CHECK: __dxc_ad_loop_0_primal_tape[iteration] = value.value;
// CHECK: value.value = ((value.value * value.value) + bias);
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_0_adjoint * __dxc_ad_loop_0_primal_tape[iteration]);

// Starting at 1 with bias 1 gives 2 then 5. The derivative is
// (2*1)*(2*2)=8, so seed 3 produces gradient 24.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 5.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.400000e+01

[[dxc::AutoDiff(bwd)]]
float f(float value, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float bias) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration)
    value = value * value + bias;
  return value;
}
