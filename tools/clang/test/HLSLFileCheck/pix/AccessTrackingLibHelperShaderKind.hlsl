// RUN: %dxc -T lib_6_6 -Od %s | %opt -S -hlsl-dxil-pix-shader-access-instrumentation,config=.256;512;1024. | %FileCheck %s

// A descriptor-heap access record carries the shader kind of the entry
// point that reached the access. HeapHelper is [noinline] so the access
// stays in the helper. The descriptor index is a parameter so the helper
// is not folded away.
//
// The kind occupies bits 31:28. An out-of-bounds record sets the
// instruction-ordinal indicator (bit 27). RayGeneration is 7 and UAVWrite
// is 3, so the in-bounds flags are 0x73000000 == 1929379840 and the
// out-of-bounds value is 0x78000000 == 2013265920. Under the module kind,
// Library (6), those values would be 0x63000000 and 0x68000000.

// CHECK: define void {{.*}}HeapHelper
// CHECK-NOT: 1660944384
// CHECK: mul i32 {{.*}}, 1929379840
// CHECK-NOT: 1744830464
// CHECK: mul i32 {{.*}}, 2013265920

[noinline]
export void HeapHelper(uint descriptorIndex)
{
    RWByteAddressBuffer heapBuffer = ResourceDescriptorHeap[descriptorIndex];
    heapBuffer.Store(0, 1);
}

[shader("raygeneration")]
void RayGen()
{
    HeapHelper(1);
}
