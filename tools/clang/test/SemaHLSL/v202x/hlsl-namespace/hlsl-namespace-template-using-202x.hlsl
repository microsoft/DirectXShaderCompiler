// RUN: %dxc -T lib_6_3 -HV 202x %s -verify
// expected-no-diagnostics

// Deferred intrinsic lookup must preserve using-directives from the template
// definition scope, even though template rebuilding has no active Scope.
template <typename T> T UseBlockUsing(T value) {
  using namespace hlsl;
  return sin(value);
}

float InstantiateBlockUsing(float value) {
  return UseBlockUsing(value);
}

template <typename T> T UseBlockDeclaration(T value) {
  using hlsl::cos;
  return cos(value);
}

float3 InstantiateBlockDeclaration(float3 value) {
  return UseBlockDeclaration(value);
}

using namespace hlsl;

template <typename T> T UseTranslationUnitUsing(T value) {
  return sin(value);
}

float InstantiateTranslationUnitUsing(float value) {
  return UseTranslationUnitUsing(value);
}
