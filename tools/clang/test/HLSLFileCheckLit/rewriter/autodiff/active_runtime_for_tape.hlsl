// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_tape_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// A statically capped nonlinear recurrence saves pre-update primal values and
// consumes them in reverse.

// CHECK: float __dxc_ad_loop_0_primal_tape[8];
// CHECK: __dxc_ad_loop_0_primal_tape[iteration] = value.value;
// CHECK: value.value *= value.value;
// CHECK: --iteration;
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_0_adjoint * __dxc_ad_loop_0_primal_tape[iteration]);
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_0_adjoint * __dxc_ad_loop_0_primal_tape[iteration]);

// With value=2 and two iterations, primal=16 and df/dvalue=32. Seed 3 gives 96.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.600000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 9.600000e+01

[[dxc::AutoDiff(bwd)]]
float f(float value, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration)
    value *= value;
  return value;
}
