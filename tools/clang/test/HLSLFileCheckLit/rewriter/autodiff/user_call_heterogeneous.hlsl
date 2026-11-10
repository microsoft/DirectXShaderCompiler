// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/user_call_heterogeneous_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

[[dxc::AutoDiff(bwd)]]
float g([[dxc::NoDiff]] float bias, float2 value, float scale) {
  return value.x * scale + bias;
}

[[dxc::AutoDiff(bwd)]]
float f([[dxc::NoDiff]] float bias, float2 value, float scale) {
  return g(bias, value, scale) * 2.0f;
}

// CHECK: GradientContext<float2> __dxc_ad_call_0_context_0 = (GradientContext<float2>)0;
// CHECK: GradientContext<float> __dxc_ad_call_0_context_1 = (GradientContext<float>)0;
// CHECK: Variable<float2> __dxc_ad_call_0_arg_1 = variable(__dxc_ad_call_0_context_0, value.value);
// CHECK: Variable<float> __dxc_ad_call_0_arg_2 = variable(__dxc_ad_call_0_context_1, scale.value);
// CHECK: g(__dxc_ad_call_0_context_0, __dxc_ad_call_0_context_1, bias, __dxc_ad_call_0_arg_1, __dxc_ad_call_0_arg_2, (__dxc_ad_seed * 2.F));
// CHECK: value_context.gradients[value.id] += __dxc_ad_call_0_arg_1.gradient(__dxc_ad_call_0_context_0);
// CHECK: scale_context.gradients[scale.id] += __dxc_ad_call_0_arg_2.gradient(__dxc_ad_call_0_context_1);

// At bias=1, value=(2,3), scale=4: primal=18. Seed 3 produces
// dvalue=(24,0) and dscale=12.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.800000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.400000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 0.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 1.200000e+01

float main(float value : A) : SV_Target {
  return f(1.0f, float2(value, 3.0f), 4.0f);
}
