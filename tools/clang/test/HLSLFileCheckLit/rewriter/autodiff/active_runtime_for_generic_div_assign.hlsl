// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_generic_div_assign_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Active compound division snapshots its target input and denominator. Reverse
// replay replaces the target cotangent before applying the quotient rule.

// CHECK: float __dxc_ad_loop_0_version_1_primal_tape[8];
// CHECK: __dxc_ad_loop_1_primal_tape[iteration] = y.value;
// CHECK: y.value /= x.value;
// CHECK: float __dxc_ad_loop_0_update_1_adjoint = __dxc_ad_loop_0_state_1_adjoint;
// CHECK: __dxc_ad_loop_0_state_1_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_1_adjoint += (__dxc_ad_loop_0_update_1_adjoint / __dxc_ad_loop_0_version_1_primal_tape[iteration]);
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (-(__dxc_ad_loop_0_update_1_adjoint) * __dxc_ad_loop_1_primal_tape[iteration] / (__dxc_ad_loop_0_version_1_primal_tape[iteration] * __dxc_ad_loop_0_version_1_primal_tape[iteration]));

// One iteration returns 18.8. Seed 2 produces gradients (1.84,2.04,4).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 0x4032CCCCC0000000
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 0x3FFD70A3E0000000
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 0x400051EB80000000
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 4.000000e+00

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y /= x;
    z *= factor;
  }
  return x + y + z;
}
