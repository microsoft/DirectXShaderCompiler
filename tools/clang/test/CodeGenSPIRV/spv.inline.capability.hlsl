// RUN: %dxc -T ps_6_0 -E main -fcgl -Vd %s -spirv | FileCheck %s

// CHECK: OpCapability Int8
// CHECK: OpCapability SampleMaskPostDepthCoverage

[[vk::ext_capability(/* SampleMaskPostDepthCoverageCapability */ 4447)]]
int val;

[[vk::ext_capability(/* Int8 */ 39)]]
void main() {
  int local = val;
}
