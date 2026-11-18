// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck %s

// Verifies that passing a heap buffer directly, via a cast at the call site
// rather than through a local alias variable, also emits a diagnostic
// instead of lowering the OpBufferPointerEXT into an invalid function
// argument.

// CHECK: heap buffer alias cannot be passed to a user function

RWByteAddressBuffer outputBytes : register(u0);

uint consume(StructuredBuffer<uint> buf) { return buf[0]; }

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  outputBytes.Store(0, consume((StructuredBuffer<uint>)ResourceDescriptorHeap[0]));
}
