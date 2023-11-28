// RUN: %dxc -T ps_6_0 -E main -fcgl %s -spirv | FileCheck %s

// CHECK-NOT: OpExtension "some_extension"
// CHECK-NOT: OpCapability SampleMaskPostDepthCoverage

[[vk::ext_extension("some_extension")]]
[[vk::ext_capability(/* SampleMaskPostDepthCoverageCapability */ 4447)]]
int val;

void main() {
}
