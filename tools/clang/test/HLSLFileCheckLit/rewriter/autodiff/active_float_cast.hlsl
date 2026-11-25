// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Equal-width floating casts are straight-through differentiable. Direct
// reverse lowering casts the result seed back to the active operand type.

// CHECK: float f(inout GradientContext<double> context, Variable<double> value, float __dxc_ad_seed)
// CHECK: float __dxc_ad_primal = ((float)value.value * 2.F);
// CHECK: context.gradients[value.id] += (double)((__dxc_ad_seed * 2.F));

[[dxc::AutoDiff(bwd)]]
float f(double value) {
  return (float)value * 2.0f;
}

float main(float value : A) : SV_Target {
  return f((double)value);
}
