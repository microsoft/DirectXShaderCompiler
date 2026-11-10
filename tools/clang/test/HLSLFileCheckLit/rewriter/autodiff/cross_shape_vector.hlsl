// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/cross_shape_vector_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// A float4 result routes its first two seed components through scalar swizzles
// into the active float2 parameter. Constant result lanes contribute nothing.

// CHECK: float4 f(inout GradientContext<float2> context, Variable<float2> uv, float4 __dxc_ad_seed)
// CHECK: float4 __dxc_ad_primal = float4(uv.value.x, uv.value.y, 0.F, 1.F);
// CHECK: context.zeroGradients();
// CHECK: context.gradients[uv.id] += float2(__dxc_ad_seed.x, 0.0f);
// CHECK: context.gradients[uv.id] += float2(0.0f, __dxc_ad_seed.y);
// CHECK: return __dxc_ad_primal;

// Primal=(2,3,0,1); seed=(4,5,6,7) routes gradient=(4,5).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 2.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 3.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 0.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 1.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 4, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 5, i32 0, float 5.000000e+00

[[dxc::AutoDiff(bwd)]]
float4 f(float2 uv) {
  return float4(uv.x, uv.y, 0.0f, 1.0f);
}
