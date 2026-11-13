// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/cross_shape_subscript_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// An inactive runtime index selects the primal vector component and routes the
// scalar seed into a one-hot vector cotangent.

// CHECK: float f(inout GradientContext<float2> context, Variable<float2> uv, uint index, float __dxc_ad_seed)
// CHECK: float __dxc_ad_primal = uv.value[index];
// CHECK: context.gradients[uv.id] += float2((index == 0 ? __dxc_ad_seed : 0.0f), (index == 1 ? __dxc_ad_seed : 0.0f));

// At uv=(2,5), index=1, and seed=3: primal=5 and gradient=(0,3).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 5.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 0.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 3.000000e+00

[[dxc::AutoDiff(bwd)]]
float f(float2 uv, [[dxc::NoDiff]] uint index) {
  return uv[index];
}
