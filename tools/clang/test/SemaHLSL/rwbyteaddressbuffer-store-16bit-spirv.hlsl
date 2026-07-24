// REQUIRES: spirv
// RUN: %dxc -T cs_6_2 -E main -spirv -enable-16bit-types -fcgl %s -verify

RWByteAddressBuffer DstBuffer : register(u1);

[numthreads(1, 1, 1)]
void main()
{
  DstBuffer.Store<uint16_t>(0, 1); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
}
