// RUN: %dxc -T lib_6_6 -HV 2021 %s -verify

// In HLSL 2021 the implicit 'hlsl' namespace does not exist. A user namespace
// with that name must not gain intrinsic members, while global usage continues
// to work.

namespace hlsl {}

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  float a = sin(0.5);           // OK
  float b = ::sin(0.5);         // OK
  float c = hlsl::sin(0.5);     // expected-error{{no member named 'sin' in namespace 'hlsl'}}
}
