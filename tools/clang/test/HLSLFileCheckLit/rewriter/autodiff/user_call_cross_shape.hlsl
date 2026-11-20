// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/user_call_cross_shape_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

[[dxc::AutoDiff(bwd)]]
float4 g(float2 value) {
  return float4(value, value.x + value.y, 1.0f);
}

[[dxc::AutoDiff(bwd)]]
float4 f(float2 value) {
  return g(value) * 2.0f;
}

// CHECK: Variable<float2> __dxc_ad_call_0_arg_0 = variable(__dxc_ad_call_0_context_0, value.value);
// CHECK: g(__dxc_ad_call_0_context_0, __dxc_ad_call_0_arg_0, (__dxc_ad_seed * 2.F));
// CHECK: context.gradients[value.id] += __dxc_ad_call_0_arg_0.gradient(__dxc_ad_call_0_context_0);

// At value=(2,3), primal=(4,6,10,2). Seed=(1,2,3,0) is first scaled by
// two, then g routes it to gradient=(2+6,4+6)=(8,10).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 1.000000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 8.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 4, i32 0, float 1.000000e+01

float4 main(float2 value : A) : SV_Target {
  return f(value);
}
