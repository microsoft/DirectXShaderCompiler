// RUN: not %dxc -T lib_6_8 %s 2>&1 | FileCheck %s

namespace dx {}

[shader("raygeneration")]
void main() {
// CHECK: error: no type named 'HitObject' in namespace 'dx'
  dx::HitObject hit;
}
