// RUN: %dxc -T ps_6_0 -E main -HV 2021 -verify %s

// Verify that pre-HLSL 202x rejects `const`-qualified instance methods, and
// that non-const methods remain callable on const (cbuffer) objects for
// backwards compatibility.

struct S {
  int x;
  int get() const { return x; } // expected-error {{const-qualified member functions are unsupported in HLSL before 202x}}
  int getNC() { return x; }
};

cbuffer CB {
  S cs;
};

float4 main() : SV_Target {
  S s = {1};
  return s.get() + cs.getNC();
}
