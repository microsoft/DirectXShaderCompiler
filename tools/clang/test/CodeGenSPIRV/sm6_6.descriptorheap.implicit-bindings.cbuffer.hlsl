// RUN: not %dxc -T ps_6_6 -E main %s -spirv  2>&1 | FileCheck %s

// CHECK: error: Using SamplerDescriptorHeap/ResourceDescriptorHeap & implicit bindings is disallowed.
cbuffer a {
  float4 value;
}

float4 main() : SV_Target {
  RWStructuredBuffer<float4> buffer = ResourceDescriptorHeap[1];
  buffer[0] = value;
  return value;
}
