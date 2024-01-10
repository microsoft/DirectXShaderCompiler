// RUN: %dxc -T ps_6_0 -E main -fcgl %s -spirv | FileCheck %s

// CHECK: OpExtension "entry_point_extension"
// CHECK: OpExtension "another_extension"
// CHECK: OpExtension "some_extension"

[[vk::ext_extension("some_extension"), vk::ext_extension("another_extension")]]
int val;

[[vk::ext_extension("entry_point_extension")]]
void main() {
  int local = val;
}
