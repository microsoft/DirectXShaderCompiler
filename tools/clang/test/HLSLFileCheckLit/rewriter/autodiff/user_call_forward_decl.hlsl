// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

float g(float value);

[[dxc::AutoDiff(bwd)]]
float f(float value) {
  return g(value) + value;
}

[[dxc::AutoDiff(bwd)]]
float g(float value) {
  return value * value;
}

// CHECK: float g(inout GradientContext<float> context, Variable<float> value, float __dxc_ad_seed);
// CHECK: float f(inout GradientContext<float> context, Variable<float> value, float __dxc_ad_seed)
// CHECK: g(__dxc_ad_call_0_context_0, __dxc_ad_call_0_arg_0, __dxc_ad_seed);

float main(float value : A) : SV_Target {
  return f(value);
}
