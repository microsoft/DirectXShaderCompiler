// RUN: not %dxc -T cs_6_6 -E main -fcgl %s -spirv 2>&1 | FileCheck %s

[[vk::binding(3, 4)]]
RWStructuredBuffer<uint> a;

[numthreads(1, 1, 1)]
void main() {
  RWStructuredBuffer<uint> b = ResourceDescriptorHeap[1];

// CHECK: error: Using SamplerDescriptorHeap/ResourceDescriptorHeap & implicit bindings is disallowed.
  a.IncrementCounter();
}
