// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Read-only resource subscripts with inactive indices remain primal loads in
// the typed expression graph and scale the active input's cotangent.

// CHECK: float f(inout GradientContext<float> context, uint index, Variable<float> value, float __dxc_ad_seed)
// CHECK: return compute_gradients_seeded(context, multiply<float>(value_expr, coefficients[index]), __dxc_ad_seed);

StructuredBuffer<float> coefficients : register(t0);

[[dxc::AutoDiff(bwd)]]
float f([[dxc::NoDiff]] uint index, float value) {
  return value * coefficients[index];
}

float main(uint index : A, float value : B) : SV_Target {
  return f(index, value);
}
