// RUN: %dxc -T lib_6_3 -HV 202x %s -verify

// Under HLSL 202x, HLSL intrinsics live exclusively in the implicit
// 'hlsl' namespace; unqualified references must fail to resolve while
// fully-qualified 'hlsl::' references must succeed.  This file is a
// -verify run, so any unexpected diagnostic causes a failure.

float UseQualifiedSin(float x) {
  // Fully qualified call: must compile cleanly.
  return hlsl::sin(x);
}

float UseQualifiedDot(float3 a, float3 b) {
  // Fully qualified call to an overloaded intrinsic: must compile cleanly.
  return hlsl::dot(a, b);
}

float UseUnqualifiedSin(float x) {
  // Unqualified intrinsic name must not resolve under HLSL 202x.
  return sin(x); // expected-error{{use of undeclared identifier 'sin'}}
}

float UseUnqualifiedDot(float3 a, float3 b) {
  return dot(a, b); // expected-error{{use of undeclared identifier 'dot'}}
}
