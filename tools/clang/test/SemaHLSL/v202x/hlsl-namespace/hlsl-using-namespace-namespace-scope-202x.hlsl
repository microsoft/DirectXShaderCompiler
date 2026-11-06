// RUN: %dxc -T lib_6_3 -HV 202x %s -verify

// Verify that an explicit `using namespace hlsl;` placed inside a
// user-defined namespace allows unqualified intrinsic references inside
// that namespace (and in member function definitions declared in it),
// while leaving lookups in sibling namespaces unaffected.

namespace Math {
  using namespace hlsl;

  float MySin(float x) {
    return sin(x);
  }

  float MyDot(float3 a, float3 b) {
    return dot(a, b);
  }

  // Forward-declared here, defined out-of-line below.
  float MyCos(float x);
}

// The using-directive inside `namespace Math` is also visible when
// defining its members out-of-line via a qualified definition.
float Math::MyCos(float x) {
  return cos(x);
}

namespace Other {
  // No `using namespace hlsl;` here: unqualified intrinsic references
  // must not resolve.
  float StillNeedsQualifier(float x) {
    return sin(x); // expected-error{{use of undeclared identifier 'sin'}}
  }

  // The qualified form continues to work.
  float QualifiedWorks(float x) {
    return hlsl::sin(x);
  }
}

// Translation-unit-scope code outside any `using namespace hlsl;` still
// requires the qualifier.
float TUScopeStillRequiresQualifier(float x) {
  return cos(x); // expected-error{{use of undeclared identifier 'cos'}}
}
