// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/typed_pullback_abi_incompatible_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Direct reverse lowering supports a result type that differs from one active
// leaf type. Multiple distinct active leaf types receive separate typed
// gradient contexts while retaining one Variable<T> per active parameter.

// CHECK: float f(inout GradientContext<float2> value_context, inout GradientContext<float> scale_context, Variable<float2> value, Variable<float> scale, float __dxc_ad_seed)
// CHECK: value_context.zeroGradients();
// CHECK: scale_context.zeroGradients();
// CHECK: value_context.gradients[value.id] += float2((__dxc_ad_seed * scale.value), 0.0f);
// CHECK: scale_context.gradients[scale.id] += (__dxc_ad_seed * value.value.x);

// At value=(2,4), scale=5, and seed=3: primal=10, dvalue=(15,0), dscale=6.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.000000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.500000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 0.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 6.000000e+00

[[dxc::AutoDiff(bwd)]]
float f(float2 value, float scale) {
  return value.x * scale;
}
