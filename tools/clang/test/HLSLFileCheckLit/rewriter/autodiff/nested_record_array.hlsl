// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

struct Inner {
  float values[2];
};

struct Outer {
  Inner inner;
  float tail;
};

// Nested record and fixed-array initializer lists flatten against the root
// result seed without constructing temporary record or array cotangents.

// CHECK: Outer f(inout GradientContext<float2> context, Variable<float2> value, Outer __dxc_ad_seed)
// CHECK: Outer __dxc_ad_primal =
// CHECK-SAME: value.value.x, value.value.y
// CHECK-SAME: value.value.x + value.value.y
// CHECK: context.gradients[value.id] += float2(__dxc_ad_seed.inner.values[0], 0.0f);
// CHECK: context.gradients[value.id] += float2(0.0f, __dxc_ad_seed.inner.values[1]);
// CHECK: context.gradients[value.id] += float2(__dxc_ad_seed.tail, 0.0f);
// CHECK: context.gradients[value.id] += float2(0.0f, __dxc_ad_seed.tail);

[[dxc::AutoDiff(bwd)]]
Outer f(float2 value) {
  Outer result = {{{value.x, value.y}}, value.x + value.y};
  return result;
}

float4 main(float2 value : A) : SV_Target {
  Outer result = f(value);
  return float4(result.inner.values[0], result.inner.values[1], result.tail,
                1.0f);
}
