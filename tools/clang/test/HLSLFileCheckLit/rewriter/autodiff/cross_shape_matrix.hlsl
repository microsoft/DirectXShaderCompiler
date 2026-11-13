// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/cross_shape_matrix_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Matrix constructors flatten their operands in row-major constructor order.
// Repeated vector components accumulate the corresponding matrix seed lanes.

// CHECK: float2x2 f(inout GradientContext<float2> context, Variable<float2> value, float2x2 __dxc_ad_seed)
// CHECK: float2x2 __dxc_ad_primal = float2x2(value.value.x, value.value.y, value.value.y, value.value.x);
// CHECK: context.gradients[value.id] += float2(__dxc_ad_seed[0][0], 0.0f);
// CHECK: context.gradients[value.id] += float2(0.0f, __dxc_ad_seed[0][1]);
// CHECK: context.gradients[value.id] += float2(0.0f, __dxc_ad_seed[1][0]);
// CHECK: context.gradients[value.id] += float2(__dxc_ad_seed[1][1], 0.0f);

// At value=(2,3), primal rows are (2,3) and (3,2). Seed (1,2;3,4)
// accumulates gradient (1+4,2+3)=(5,5).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 2.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 3.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 3.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 2.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 4, i32 0, float 5.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 5, i32 0, float 5.000000e+00

[[dxc::AutoDiff(bwd)]]
float2x2 f(float2 value) {
  return float2x2(value.x, value.y, value.y, value.x);
}
