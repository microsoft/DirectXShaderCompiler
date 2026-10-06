// RUN: %dxc -T lib_6_3 -HV 202x %s -verify

// Verify that an explicit `using namespace hlsl;` at function/block
// scope is honored under HLSL 202x: unqualified intrinsic references
// inside the function resolve, while references in sibling functions
// that have no such using-directive in scope do not.

float UsesUsingInBody(float x, float3 v) {
  using namespace hlsl;
  float a = sin(x);
  float b = cos(x);
  float c = dot(v, v);
  return a + b + c;
}

float UsesUsingInNestedBlock(float x) {
  float result = 0;
  {
    using namespace hlsl;
    result = saturate(x);
  }
  return result;
}

float NoUsingDirectiveHere(float x) {
  // No using-directive in scope: the unqualified name must not resolve.
  return sin(x); // expected-error{{use of undeclared identifier 'sin'}}
}

float NoUsingDirectiveDot(float3 a, float3 b) {
  return dot(a, b); // expected-error{{use of undeclared identifier 'dot'}}
}

float UsingDirectiveDoesNotLeakOutOfBlock(float x) {
  {
    using namespace hlsl;
    (void)sin(x);
  }
  // The `using namespace hlsl;` above is scoped to the inner block and
  // must not affect lookups outside of it.
  return cos(x); // expected-error{{use of undeclared identifier 'cos'}}
}
