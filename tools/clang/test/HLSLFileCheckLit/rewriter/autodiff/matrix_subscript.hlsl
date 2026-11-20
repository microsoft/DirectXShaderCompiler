// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/matrix_subscript_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Nested inactive row and column indices route a scalar result seed into one
// selected matrix lane.

// CHECK: float f(inout GradientContext<float2x2> context, Variable<float2x2> value, uint row, uint column, float __dxc_ad_seed)
// CHECK: context.gradients[value.id] += float2x2((row == 0 ? vector<float, 2>((column == 0 ? __dxc_ad_seed : 0.0f), (column == 1 ? __dxc_ad_seed : 0.0f)).x : 0.0f),

// For value=(2,3;4,5), row=1, column=0, and seed=3, the primal is 4 and
// the only nonzero gradient lane is [1][0].
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 0.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 0.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 3.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 4, i32 0, float 0.000000e+00

[[dxc::AutoDiff(bwd)]]
float f(float2x2 value, [[dxc::NoDiff]] uint row,
        [[dxc::NoDiff]] uint column) {
  return value[row][column];
}
