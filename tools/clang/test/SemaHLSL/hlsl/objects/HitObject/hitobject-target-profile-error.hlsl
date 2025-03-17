// RUN: %dxc -T lib_6_8 %s -verify

namespace dx {}

[shader("raygeneration")]
void main() {
  // expected-warning@+1{{potential misuse of built-in function 'dx::HitObject::MakeNop' in shader model lib_6_8; introduced in shader model 6.9}}
  dx::HitObject::MakeNop();
}
