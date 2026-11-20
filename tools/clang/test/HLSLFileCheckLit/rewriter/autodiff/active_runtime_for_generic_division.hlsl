// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_generic_division_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Active division records both changing primal operands. Reverse replay
// applies the quotient rule to their versioned loop-state values.

// CHECK: float __dxc_ad_loop_0_version_1_primal_tape[8];
// CHECK: y.value = (x.value / z.value);
// CHECK: __dxc_ad_loop_2_primal_tape[iteration] = z.value;
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_1_adjoint / __dxc_ad_loop_2_primal_tape[iteration]);
// CHECK: __dxc_ad_loop_0_state_2_adjoint += (-(__dxc_ad_loop_0_update_1_adjoint) * __dxc_ad_loop_0_version_1_primal_tape[iteration] / (__dxc_ad_loop_2_primal_tape[iteration] * __dxc_ad_loop_2_primal_tape[iteration]));

// One iteration returns 14.25. Seed 2 produces gradients (2.5,2.5,3.375).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.425000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.500000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 2.500000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 3.375000e+00

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y = x / z;
    z *= factor;
  }
  return x + y + z;
}
