// RUN: not %dxc -T cs_6_6 -E main -fcgl %s -spirv 2>&1 | FileCheck %s

// CHECK: error: Using SamplerDescriptorHeap/ResourceDescriptorHeap & implicit bindings is disallowed.
[[vk::counter_binding(4)]]
RWStructuredBuffer<uint> a;

[numthreads(1, 1, 1)]
void main() {
  RWStructuredBuffer<uint> b = ResourceDescriptorHeap[1];
}
