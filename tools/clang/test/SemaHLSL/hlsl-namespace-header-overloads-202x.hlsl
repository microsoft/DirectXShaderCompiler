// RUN: %dxc -T lib_6_3 -HV 202x %s -verify
// expected-no-diagnostics

#include "Inputs/hlsl-namespace-overloads.h"

float QualifiedHeaderOverload(int value) {
  return hlsl::sin(value).value;
}

float UsingDirectiveHeaderOverload(int value) {
  using namespace hlsl;
  return sin(value).value;
}

float UsingDeclarationHeaderOverload(int value) {
  using hlsl::sin;
  return sin(value).value;
}

float UnqualifiedADL(hlsl::HeaderArgument value) {
  return headerFunction(value).value;
}

float IntrinsicNameADL(associated::Argument value) {
  using namespace hlsl;
  return cos(value).value;
}

float HeaderBodyADL(associated::Argument value) {
  return hlsl::callFromHeader(value).value;
}

float HeaderBodyIntrinsic(float value) {
  return hlsl::callIntrinsicFromHeader(value);
}

float3 HeaderTemplateBodyIntrinsic(float3 value) {
  return hlsl::callIntrinsicFromHeaderTemplate(value);
}

float HeaderOverloadWithBuiltinMismatch() {
  return hlsl::TraceRay(1, 2, 3, 4, 5, 6, 7, 8).value;
}

float HeaderOverloadWithBuiltinPayloadMismatch(RayDesc ray) {
  return hlsl::TraceRay(1, 2, 3, 4, 5, 6, ray, 8).value;
}
