// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/inactive_if_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Control flow depending only on an inactive local is folded into a conditional
// primal binding. The condition and branch mutation contribute no derivative.

// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: subtract<float>(x_expr, ((floor(x.value) < 0.F) ? (floor(x.value) + 2.F) : floor(x.value)))
// CHECK-NOT: _Static_assert

// At x=-0.25, wrapped floor(-0.25) becomes 1, so f=-1.25. Seed 3 passes
// unchanged through the active x term.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float -1.250000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 3.000000e+00

[[dxc::AutoDiff(bwd)]]
float f(float x) {
  float wrapped = [[dxc::NoDiff]] floor(x);
  if (wrapped < 0.0f)
    wrapped += 2.0f;
  return x - wrapped;
}
