// RUN: not %dxc -T lib_6_8 %s 2>&1 | FileCheck %s

[shader("raygeneration")]
void main() {
// CHECK: error: unknown type name 'HitObject'
  HitObject hit;
}
