// REQUIRES: spirv
// RUN: %dxc -T cs_6_2 -E main -spirv -enable-16bit-types -fcgl %s -verify

RWByteAddressBuffer DstBuffer : register(u1);

struct ShortStruct { uint16_t a; };
struct NestedStruct { int a; ShortStruct b; };

[numthreads(1, 1, 1)]
void main()
{
  DstBuffer.Store<half>(0, 1); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
  DstBuffer.Store<uint16_t>(0, 1); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
  DstBuffer.Store<int16_t>(0, 1); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}

  DstBuffer.Store<uint16_t2>(0, 1); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
  DstBuffer.Store<uint16_t4x4>(0, 1); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}

  DstBuffer.Store<ShortStruct>(0, (ShortStruct)0); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
  DstBuffer.Store<NestedStruct>(0, (NestedStruct)0); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}

  uint16_t val = 0;
  DstBuffer.Store<uint16_t>(0, val); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
  DstBuffer.Store<int16_t>(0, (int16_t)0); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
}
