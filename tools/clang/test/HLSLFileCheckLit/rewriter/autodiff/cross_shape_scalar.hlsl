// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/cross_shape_scalar_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// A scalar result routes repeated uses of uv.x and one use of uv.y back into a
// float2 gradient. Contributions to the same leaf accumulate.

// CHECK: float f(inout GradientContext<float2> context, Variable<float2> uv, float __dxc_ad_seed)
// CHECK: float __dxc_ad_primal = ((uv.value.x * uv.value.x) + uv.value.y);
// CHECK: context.gradients[uv.id] += float2((__dxc_ad_seed * uv.value.x), 0.0f);
// CHECK: context.gradients[uv.id] += float2((__dxc_ad_seed * uv.value.x), 0.0f);
// CHECK: context.gradients[uv.id] += float2(0.0f, __dxc_ad_seed);

// At uv=(2,3), f=7. Seed 2 scales gradient (2x,1) to (8,2).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 7.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 8.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 2.000000e+00

[[dxc::AutoDiff(bwd)]]
float f(float2 uv) {
  return uv.x * uv.x + uv.y;
}
