// RUN: %dxc -T lib_6_8 %s -verify

[shader("raygeneration")]
void main() {
  // expected-error@+1{{use of undeclared identifier 'dx'}}
  dx::HitObject hit;
}
