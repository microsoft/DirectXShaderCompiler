// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_quadratic_tape_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// General quadratic and linear terms replay through the generic expression DAG.

// CHECK: value.value = (((quadratic * (value.value * value.value)) + (slope * value.value)) + bias);
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_0_adjoint * slope);

// For a=2, b=3, c=1 and x=1: x1=6, x2=91. Local derivatives are 7 and 27,
// so seed 2 produces gradient 2*7*27=378.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 9.100000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 3.780000e+02

[[dxc::AutoDiff(bwd)]]
float f(float value, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float quadratic,
        [[dxc::NoDiff]] float slope, [[dxc::NoDiff]] float bias) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration)
    value = quadratic * (value * value) + slope * value + bias;
  return value;
}
