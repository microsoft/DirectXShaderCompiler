// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Forward locals use their own primal types, while assignments to inactive
// parameters remain raw rather than introducing Value<uint> wrappers.

// CHECK: Value<float> f(uint width, uint lod, Value<float> value)
// CHECK: width = (width >> lod);
// CHECK: Value<uint> index = Value<uint>::CreateValue((width % 7));
// CHECK: return (value * Value<float>::CreateValue((float)index.value));

[[dxc::AutoDiff(fwd)]]
float f([[dxc::NoDiff]] uint width, [[dxc::NoDiff]] uint lod, float value) {
  width >>= lod;
  uint index = width % 7;
  return value * (float)index;
}

float main(float value : A) : SV_Target {
  return f(32, 2, value);
}
