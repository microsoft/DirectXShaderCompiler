// RUN: %dxc -E main -T ps_6_0 -HV 202x %s -verify

// HLSL supports only one fixed base type, not base-class packs.

struct Base1 {
  float x;
};
struct Base2 {
  float y;
};

// expected-error@+3 {{base type ellipsis is unsupported in HLSL}}
// expected-error@+2 {{multiple concrete base types specified}}
template <typename... Bases>
struct Derived : Bases... {
  float z;
};

float main() : SV_Target {
  Derived<Base1, Base2> d; // expected-note {{in instantiation of template class 'Derived<Base1, Base2>' requested here}}
  d.z = 1.0;
  return d.z;
}
