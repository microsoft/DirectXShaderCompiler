// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// A canonical runtime loop with a loop-invariant derivative replays the primal
// update and applies that derivative once per iteration in reverse.

// CHECK: for (uint iteration = 0; iteration < count; ++iteration)
// CHECK: value.value *= 2.F;
// CHECK: float __dxc_ad_loop_0_state_0_adjoint = __dxc_ad_seed;
// CHECK: for (uint iteration = count; iteration > 0;)
// CHECK: --iteration;
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_0_adjoint * 2.F);
// CHECK: context.gradients[value.id] += __dxc_ad_loop_0_state_0_adjoint;

// At value=2 and count=3, primal=16. Seed 3 scales 2^3 to gradient 24.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.600000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.400000e+01

[[dxc::AutoDiff(bwd)]]
float f(float value, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < count; ++iteration)
    value *= 2.0f;
  return value;
}
