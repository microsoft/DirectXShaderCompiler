// REQUIRES: spirv
// RUN: %dxc -T ps_6_0 -E main -spirv -verify %s

struct MyStruct {
  float4 a;
};

[[vk::binding(0, 0)]]
vk::SampledTexture2D<MyStruct> tex1; // expected-error {{elements of typed buffers and textures must be scalars or vectors}}

[[vk::binding(1, 0)]]
vk::SampledTexture2D<float[4]> tex2; // expected-error {{elements of typed buffers and textures must be scalars or vectors}}

[[vk::binding(2, 0)]]
vk::SampledTexture2D<float> tex3;

[[vk::binding(3, 0)]]
vk::SampledTexture2D<float4> tex4;

[[vk::binding(4, 0)]]
vk::SampledTexture2D<int> tex5;

[[vk::binding(5, 0)]]
vk::SampledTexture2D<uint> tex6;

void main() {}
