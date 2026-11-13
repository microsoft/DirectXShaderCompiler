// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

Texture2D<float4> textureResource : register(t0);

// Inactive side-effecting calls retain named primal storage for out arguments.

// CHECK: uint width;
// CHECK: uint height;
// CHECK: uint levels;
// CHECK: textureResource.GetDimensions(0, width, height, levels);
// CHECK: return compute_gradients_seeded(context, multiply<float>(value_expr, (float)width), __dxc_ad_seed);

[[dxc::AutoDiff(bwd)]]
float f(float value) {
  uint width;
  uint height;
  uint levels;
  [[dxc::NoDiff]]
  textureResource.GetDimensions(0, width, height, levels);
  return value * (float)width;
}

float main(float value : A) : SV_Target {
  return f(value);
}
