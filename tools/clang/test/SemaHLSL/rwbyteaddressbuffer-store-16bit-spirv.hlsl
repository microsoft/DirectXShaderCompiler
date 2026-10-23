// REQUIRES: spirv
// RUN: %dxc -T cs_6_2 -E main -spirv -enable-16bit-types -fcgl %s -verify

RWByteAddressBuffer DstBuffer : register(u1);

struct ShortStruct { uint16_t a; };
struct NestedStruct { int a; ShortStruct b; };

struct Base { uint16_t x; };
struct Derived : Base { uint y; };

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

  DstBuffer.Store<min16float>(0, (min16float)1); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
  DstBuffer.Store<min16uint>(0, (min16uint)1); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
  DstBuffer.Store<min16int>(0, (min16int)1); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
  DstBuffer.Store<min10float>(0, (min10float)0); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
  DstBuffer.Store<min12int>(0, (min12int)0); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
  DstBuffer.Store<min16uint4>(0, (min16uint4)1); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
  DstBuffer.Store<min16uint4x4>(0, (min16uint4x4)1); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}

  DstBuffer.Store<Derived>(0, (Derived)1); // expected-error {{RWByteAddressBuffer does not support storing 16bit types in Vulkan}}
}
