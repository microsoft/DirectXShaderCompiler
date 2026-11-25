// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Nested constant-buffer indexing uses ArraySubscriptExpr rather than the
// overloaded vector operator[] path. Inactive indices remain primal values.

// CHECK: float f(inout GradientContext<float> context, uint lod, Variable<float> value, float __dxc_ad_seed)
// CHECK: return compute_gradients_seeded(context, multiply<float>(value_expr, (float)mipOffset[(lod / 4)][(lod % 4)]), __dxc_ad_seed);

cbuffer Uniforms : register(b0) {
  uint4 mipOffset[16];
};

[[dxc::AutoDiff(bwd)]]
float f([[dxc::NoDiff]] uint lod, float value) {
  uint offset = mipOffset[lod / 4][lod % 4];
  return value * (float)offset;
}

float main(float value : A) : SV_Target {
  return f(0, value);
}
