// RUN: %dxc -T lib_6_3 -HV 202x %s -verify

// Verify that an explicit `using namespace hlsl;` at translation-unit
// scope is honored under HLSL 202x: unqualified references to HLSL
// intrinsics resolve through the nominated namespace and need not use
// the `hlsl::` qualifier.

// expected-no-diagnostics

namespace hlsl {}
using namespace hlsl;

float UseUnqualifiedSin(float x) {
  return sin(x);
}

float UseUnqualifiedCos(float x) {
  return cos(x);
}

float UseUnqualifiedDot(float3 a, float3 b) {
  return dot(a, b);
}

float UseUnqualifiedSaturate(float x) {
  return saturate(x);
}

float UseQualifiedAlsoStillWorks(float x) {
  return hlsl::sin(x);
}
