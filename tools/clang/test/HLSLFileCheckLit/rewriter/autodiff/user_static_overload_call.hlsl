// RUN: %dxr -generate-differentials %s | FileCheck %s

struct Functions {
  [[dxc::AutoDiff(bwd)]]
  static float g(float value) {
    return value * value;
  }

  [[dxc::AutoDiff(bwd)]]
  static float2 g(float2 value) {
    return value * value;
  }

  [[dxc::AutoDiff(bwd)]]
  static float f(float value) {
    return g(value) + value;
  }
};

// CHECK: static float g(inout GradientContext<float> context, Variable<float> value, float __dxc_ad_seed)
// CHECK: static float2 g(inout GradientContext<float2> context, Variable<float2> value, float2 __dxc_ad_seed)
// CHECK: static float f(inout GradientContext<float> context, Variable<float> value, float __dxc_ad_seed)
// CHECK: float __dxc_ad_primal = (::Functions::g(value.value) + value.value);
// CHECK: g(__dxc_ad_call_0_context_0, __dxc_ad_call_0_arg_0, __dxc_ad_seed);

float main(float value : A) : SV_Target {
  return Functions::f(value);
}
