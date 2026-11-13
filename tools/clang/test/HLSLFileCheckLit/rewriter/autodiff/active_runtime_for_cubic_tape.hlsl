// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_cubic_tape_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Cubic recurrences replay through the generic expression DAG.

// CHECK: value.value = (((value.value * value.value) * value.value) + (slope * value.value));
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_0_adjoint * (__dxc_ad_loop_0_primal_tape[iteration] * __dxc_ad_loop_0_primal_tape[iteration]));

// Starting at 1 with slope 1 gives 2 then 10. Local derivatives are 4 and 13,
// so seed 2 produces gradient 104.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.000000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.040000e+02

[[dxc::AutoDiff(bwd)]]
float f(float value, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float slope) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration)
    value = value * value * value + slope * value;
  return value;
}
