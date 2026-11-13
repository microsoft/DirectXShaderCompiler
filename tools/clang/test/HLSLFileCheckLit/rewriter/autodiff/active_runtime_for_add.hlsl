// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Additive recurrence has unit derivative with respect to carried active state.

// CHECK: for (uint iteration = 0; iteration < count; ++iteration)
// CHECK: value.value += (float)(iteration + 1);
// CHECK: float __dxc_ad_loop_0_state_0_adjoint = __dxc_ad_seed;
// CHECK-NOT: _reverse
// CHECK: context.gradients[value.id] += __dxc_ad_loop_0_state_0_adjoint;

[[dxc::AutoDiff(bwd)]]
float f(float value, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < count; ++iteration)
    value += (float)(iteration + 1);
  return value;
}

float main(float value : A) : SV_Target {
  return f(value, 3);
}
