// RUN: %dxc -T ps_6_0 -E main -fcgl -Vd %s -spirv | FileCheck %s

// CHECK: OpCapability SampleMaskPostDepthCoverage

[[vk::ext_capability(/* SampleMaskPostDepthCoverageCapability */ 4447)]]
int val;

void main() {
  int local = val;
}
