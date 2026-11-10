// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_multiple_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Independent active values share one primal runtime loop and propagate their
// adjoints independently.

// CHECK: for (uint iteration = 0; iteration < count; ++iteration) {
// CHECK: x.value *= factor;
// CHECK: y.value += 3.F;
// CHECK: float __dxc_ad_loop_0_state_0_adjoint = __dxc_ad_seed;
// CHECK: context.gradients[x.id] += __dxc_ad_loop_0_state_0_adjoint;
// CHECK: float __dxc_ad_loop_1_state_1_adjoint = __dxc_ad_seed;
// CHECK: context.gradients[y.id] += __dxc_ad_loop_1_state_1_adjoint;

// Two iterations map x=1 to 4 and y=5 to 11, so primal=15. Seed 2 produces
// dx=8 and dy=2.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.500000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 8.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 2.000000e+00

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < count; ++iteration) {
    x *= factor;
    y += 3.0f;
  }
  return x + y;
}
