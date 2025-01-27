// RUN: %dxc -T lib_6_6 %s -verify
// RUN: %dxc -T lib_6_8 %s -verify

namespace dx {}

[shader("raygeneration")]
void main()
{
  // expected-error@+1{{no member named 'HitObject' in namespace 'dx'}}
  dx::HitObject::MakeNop();
}
