// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck %s

// Verifies: a rejected bound/heap assignment that is used as a value is
// diagnosed without crashing the compiler.
//
// The rejected assignment has no SPIR-V value of its own, but the enclosing
// expression, here a method call on the assignment result, still needs one.

// CHECK: mixing bound and descriptor heap resources in the same variable is not supported
// CHECK-NOT: Internal compiler error

StructuredBuffer<float> bound : register(t0);
RWBuffer<float> o : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 t : SV_DispatchThreadID) {
  StructuredBuffer<float> b = ResourceDescriptorHeap[1];
  o[t.x] = (b = bound).Load(0);
}
