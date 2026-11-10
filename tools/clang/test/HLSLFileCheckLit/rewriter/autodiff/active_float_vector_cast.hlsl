// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Floating vector casts preserve component count and route the complete
// result cotangent back through the source vector type.

// CHECK: float2 f(inout GradientContext<double2> context, Variable<double2> value, float2 __dxc_ad_seed)
// CHECK: float2 __dxc_ad_primal = ((float2)value.value * 2.F);
// CHECK: context.gradients[value.id] += (double2)((__dxc_ad_seed * 2.F));

[[dxc::AutoDiff(bwd)]]
float2 f(double2 value) {
  return (float2)value * 2.0f;
}

float2 main(float2 value : A) : SV_Target {
  return f((double2)value);
}
