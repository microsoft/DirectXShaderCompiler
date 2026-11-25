// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_coupled_affine_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Assignment-form coupled products replay through the generic expression DAG.

// CHECK: x.value = ((x.value * y.value) + bias);
// CHECK: y.value *= factor;
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_1_adjoint += (__dxc_ad_loop_0_update_0_adjoint * __dxc_ad_loop_0_primal_tape[iteration]);

// Starting from (2,3), bias=1, and factor=2 gives (7,6) then (43,12),
// returning 55. Seed 2 produces gradients (36,60).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 5.500000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 3.600000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 6.000000e+01

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float bias, [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x = x * y + bias;
    y *= factor;
  }
  return x + y;
}
