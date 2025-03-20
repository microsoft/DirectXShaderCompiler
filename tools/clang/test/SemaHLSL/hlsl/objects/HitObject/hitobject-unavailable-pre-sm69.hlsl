// RUN: %dxc -T lib_6_8 %s -verify

// Check that the HitObject type name of Shader Execution Reordering is unclaimed pre SM 6.9.

[shader("raygeneration")]
void main() {
  // expected-warning@+3{{potential misuse of built-in type ''dx::HitObject'' in shader model lib_6_8; introduced in shader model 6.9}}
  // expected-warning@+2{{potential misuse of built-in function 'dx::HitObject::MakeNop' in shader model lib_6_8; introduced in shader model 6.9}}
  // expected-warning@+1{{potential misuse of built-in type ''dx::HitObject'' in shader model lib_6_8; introduced in shader model 6.9}}
  dx::HitObject hit = dx::HitObject::MakeNop();
}
