// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/cross_shape_record_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

struct Result {
  float2 position;
  float weight;
};

// Flat record constructors route each field's seed back through the
// corresponding constructor operand.

// CHECK: Result f(inout GradientContext<float2> context, Variable<float2> value, Result __dxc_ad_seed)
// CHECK: Result __dxc_ad_primal = {value.value, (value.value.x + value.value.y)};
// CHECK: context.gradients[value.id] += float2(__dxc_ad_seed.position.x, __dxc_ad_seed.position.y);
// CHECK: context.gradients[value.id] += float2(__dxc_ad_seed.weight, 0.0f);
// CHECK: context.gradients[value.id] += float2(0.0f, __dxc_ad_seed.weight);

// At value=(2,3), primal=(2,3;5). Seed position=(7,11), weight=13 gives
// gradient=(7+13,11+13)=(20,24).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 2.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 3.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 5.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 2.000000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 4, i32 0, float 2.400000e+01

[[dxc::AutoDiff(bwd)]]
Result f(float2 value) {
  Result result = {value, value.x + value.y};
  return result;
}
