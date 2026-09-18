// RUN: %dxc -T lib_6_3 -HV 2021 -verify %s

// Variadic templates are a HLSL 202x feature. HLSL 2021 (the version prior
// to 202x that supports templates at all) must continue to reject the C++
// variadic template syntax.

template <typename T, typename... Rest>
// expected-error@-1{{variadic templates are not supported in HLSL}}
T Sum(T First, Rest... Others) {
  // expected-error@-1{{unknown type name 'Rest'}}
  // expected-error@-2{{variadic arguments is unsupported in HLSL}}
  // expected-error@-3{{expected ')'}}
  // expected-note@-4{{to match this '('}}
  return First;
}

template <int... Values>
// expected-error@-1{{'...' must be innermost component of anonymous pack declaration}}
// expected-error@-2{{variadic templates are not supported in HLSL}}
// expected-error@-3{{expected ',' or '>' in template-parameter-list}}
struct IntPack {};

uint CallSizeofPack() {
  // sizeof...() is only meaningful with variadic templates, and remains
  // unsupported before HLSL 202x.
  return sizeof...(Values);
  // expected-error@-1{{expected expression}}
}

int CallInitListExpansion() {
  int a = 1, b = 2, c = 3;
  // Pack expansion inside a braced-init-list also remains unsupported
  // before HLSL 202x.
  int values[3] = {a, b, c...};
  // expected-error@-1{{expansion is unsupported in HLSL}}
  return values[0];
}
