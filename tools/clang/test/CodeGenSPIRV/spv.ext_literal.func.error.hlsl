// RUN: not %dxc -T ps_6_0 -E main -fcgl  %s -spirv 2>&1 | FileCheck %s

void main() {
// CHECK: error: vk::ext_literal() may only be used with vk::SpirvType
  vk::ext_literal(4);
}
