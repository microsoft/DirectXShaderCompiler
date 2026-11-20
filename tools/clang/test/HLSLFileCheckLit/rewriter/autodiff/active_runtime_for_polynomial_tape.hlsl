// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_polynomial_tape_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// A bounded quadratic recurrence replays through the generic expression DAG.

// CHECK: value.value = (((value.value * value.value) + (scale * value.value)) + bias);
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_0_adjoint * scale);

// Starting at 1 with scale 2 and bias 1 gives 4 then 25. Local derivatives
// are 4 and 10, so seed 3 produces gradient 120.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 2.500000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.200000e+02

[[dxc::AutoDiff(bwd)]]
float f(float value, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float scale, [[dxc::NoDiff]] float bias) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration)
    value = value * value + scale * value + bias;
  return value;
}
