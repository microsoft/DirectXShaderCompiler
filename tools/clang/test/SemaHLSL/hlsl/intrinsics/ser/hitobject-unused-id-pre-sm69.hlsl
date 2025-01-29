// RUN: %dxc -T lib_6_6 %s -verify
// RUN: %dxc -T lib_6_8 %s -verify

[shader("raygeneration")]
void main()
{
  // expected-error@+1{{use of undeclared identifier 'HitObject'}}
  HitObject::MakeNop();
}
