// RUN: %dxc -T lib_6_3 -HV 2021 -verify %s

// 'constexpr' is not supported in HLSL versions prior to 202x.
#if __has_feature(cxx_constexpr)
#error HLSL 2021 should not report constexpr as a feature
#endif

constexpr int g_constexpr = 3; // expected-error {{unknown type name 'constexpr'}} expected-error {{expected unqualified-id}}

constexpr int square(int x) { return x * x; } // expected-error {{unknown type name 'constexpr'}} expected-error {{expected unqualified-id}}

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  constexpr int local = 5; // expected-error {{unknown type name 'constexpr'}} expected-error {{expected unqualified-id}}
  (void)local; // expected-error {{use of undeclared identifier 'local'}}
}
