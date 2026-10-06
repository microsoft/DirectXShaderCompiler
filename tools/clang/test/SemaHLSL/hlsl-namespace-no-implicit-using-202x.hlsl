// RUN: %dxc -T lib_6_3 -HV 202x %s -verify

// Under HLSL 202x there must NOT be an implicit 'using namespace hlsl;'
// directive at translation-unit scope.  Therefore unqualified references
// to HLSL intrinsics must fail with the standard Clang 'use of
// undeclared identifier' diagnostic; only the 'hlsl::'-qualified form
// resolves.

void unqualified_fails() {
  // Each unqualified HLSL intrinsic should be reported as undeclared,
  // because the implicit 'hlsl' namespace is not nominated by any
  // using-directive in this translation unit.
  float x = 1.0f;
  float a = sin(x);          // expected-error{{use of undeclared identifier 'sin'}}
  float b = cos(x);          // expected-error{{use of undeclared identifier 'cos'}}
  float3 v = float3(1, 0, 0);
  float c = dot(v, v);       // expected-error{{use of undeclared identifier 'dot'}}
  float d = saturate(x);     // expected-error{{use of undeclared identifier 'saturate'}}
}

void qualified_succeeds() {
  // The qualified form must continue to work.
  float x = 1.0f;
  float a = hlsl::sin(x);
  float b = hlsl::cos(x);
  float3 v = float3(1, 0, 0);
  float c = hlsl::dot(v, v);
  float d = hlsl::saturate(x);
  // Silence -Wunused-value
  (void)a; (void)b; (void)c; (void)d;
}

void global_qualification_fails() {
  float x =
      ::sin(1.0f); // expected-error{{no member named 'sin' in the global namespace}}
}

namespace outer {
namespace hlsl {}

void nested_hlsl_namespace_does_not_gain_intrinsics() {
  float x = hlsl::sin(1.0f); // expected-error{{no member named 'sin'}}
}
}
