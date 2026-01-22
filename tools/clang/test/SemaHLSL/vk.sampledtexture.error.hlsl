// REQUIRES: spirv
// RUN: %dxc -T ps_6_0 -E main -spirv -verify %s

struct MyStruct {
  float4 a;
};

[[vk::binding(0, 0)]]
vk::SampledTexture2D<MyStruct> tex1; // expected-error {{'MyStruct' cannot be used as a type parameter where it must be of type float, int, uint or their vector equivalents}}

[[vk::binding(1, 0)]]
vk::SampledTexture2D<float[4]> tex2; // expected-error {{'float [4]' cannot be used as a type parameter where it must be of type float, int, uint or their vector equivalents}}

[[vk::binding(2, 0)]]
vk::SampledTexture2D<float> tex3;

[[vk::binding(3, 0)]]
vk::SampledTexture2D<float4> tex4;

[[vk::binding(4, 0)]]
vk::SampledTexture2D<int> tex5;

[[vk::binding(5, 0)]]
vk::SampledTexture2D<uint> tex6;

void main() {}
