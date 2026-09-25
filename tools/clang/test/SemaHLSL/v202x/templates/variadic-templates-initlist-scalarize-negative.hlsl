// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// Compare diagnostics for pack-expanded and explicit initializer lists.

float4 ManualTooFew(float a, float b) {
  float4 v = { a, b }; // expected-error {{too few elements in vector initialization (expected 4 elements, have 2)}}
  return v;
}

template <typename... Ts>
float4 PackTooFew(Ts... vals) {
  float4 v = { vals... }; // expected-error {{too few elements in vector initialization (expected 4 elements, have 2)}}
  return v;
}

float3 ManualTooMany(float a, float b, float c, float d) {
  float3 v = { a, b, c, d }; // expected-error {{too many elements in vector initialization (expected 3 elements, have 4)}}
  return v;
}

template <typename... Ts>
float3 PackTooMany(Ts... vals) {
  float3 v = { vals... }; // expected-error {{too many elements in vector initialization (expected 3 elements, have 4)}}
  return v;
}

struct S { float a; float2 b; float c; };

S ManualStructTooFew(float a, float b) {
  S s = { a, b }; // expected-error {{too few elements in vector initialization (expected 4 elements, have 2)}}
  return s;
}

template <typename... Ts>
S PackStructTooFew(Ts... vals) {
  S s = { vals... }; // expected-error {{too few elements in vector initialization (expected 4 elements, have 2)}}
  return s;
}

float2x2 ManualMatrixTooFew(float a, float b, float c) {
  float2x2 m = { a, b, c }; // expected-error {{too few elements in vector initialization (expected 4 elements, have 3)}}
  return m;
}

template <typename... Ts>
float2x2 PackMatrixTooFew(Ts... vals) {
  float2x2 m = { vals... }; // expected-error {{too few elements in vector initialization (expected 4 elements, have 3)}}
  return m;
}

export
float UseAll() {
  float4 v1 = PackTooFew(1.0, 2.0); // expected-note {{in instantiation of function template specialization 'PackTooFew<float, float>' requested here}}
  float3 v2 = PackTooMany(1.0, 2.0, 3.0, 4.0); // expected-note {{in instantiation of function template specialization 'PackTooMany<float, float, float, float>' requested here}}
  S s = PackStructTooFew(1.0, 2.0); // expected-note {{in instantiation of function template specialization 'PackStructTooFew<float, float>' requested here}}
  float2x2 m = PackMatrixTooFew(1.0, 2.0, 3.0); // expected-note {{in instantiation of function template specialization 'PackMatrixTooFew<float, float, float>' requested here}}
  return v1.x + v2.x + s.a + m._11 + ManualTooFew(1.0, 2.0).x + ManualTooMany(1.0, 2.0, 3.0, 4.0).x + ManualStructTooFew(1.0, 2.0).a + ManualMatrixTooFew(1.0, 2.0, 3.0)._11;
}
