// RUN: %dxc -T ps_6_0 -E main -fcgl %s -spirv | FileCheck %s

// CHECK-NOT: OpExtension "some_extension"
// CHECK-NOT: OpCapability SampleMaskPostDepthCoverage
// CHECK-NOT: OpExtension "ext_on_field"
// CHECK-NOT: OpCapability StorageBuffer8BitAccess

[[vk::ext_extension("some_extension")]]
[[vk::ext_capability(/* SampleMaskPostDepthCoverageCapability */ 4447)]]
int val;

struct T {
  [[vk::ext_extension("ext_on_field")]]
  [[vk::ext_capability(/* StorageBuffer8BitAccess */ 4448)]]
  int v;
};

void main() {
}
