// RUN: %dxc -T ps_6_0 -E main -fcgl %s -spirv | FileCheck %s

// CHECK: OpExtension "another_extension"
// CHECK: OpExtension "some_extension"

[[vk::ext_extension("some_extension"), vk::ext_extension("another_extension")]]
int val;

void main() {
  int local = val;
}
