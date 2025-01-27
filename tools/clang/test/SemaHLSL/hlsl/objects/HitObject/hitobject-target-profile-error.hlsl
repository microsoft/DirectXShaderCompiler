// RUN: %dxc -T lib_6_8 %s -verify

namespace dx {}

[shader("raygeneration")]
void main() {
// expected-error@+1{{no type named 'HitObject' in namespace 'dx'}}
  dx::HitObject hit;
}
