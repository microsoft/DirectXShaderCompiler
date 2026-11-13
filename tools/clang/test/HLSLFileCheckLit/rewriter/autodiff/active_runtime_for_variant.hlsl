// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_variant_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Factors that depend only on inactive state and the induction variable can be
// recomputed while iterating the counter in reverse.

// CHECK: for (uint iteration = 0; iteration < count; ++iteration)
// CHECK: value.value *= (float)(iteration + 1);
// CHECK: for (uint iteration = count; iteration > 0;)
// CHECK: --iteration;
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_0_adjoint * (float)(iteration + 1));

// At value=2 and count=3, factors are 1,2,3: primal=12. Seed 4 produces
// gradient 4*1*2*3=24.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.200000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.400000e+01

[[dxc::AutoDiff(bwd)]]
float f(float value, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < count; ++iteration)
    value *= (float)(iteration + 1);
  return value;
}
