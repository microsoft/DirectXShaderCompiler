// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

struct S {
  float value;
};

struct Other {
  float value;
};

void invalid(float2 v, Texture2D<float4> texture) {
  float4 widened = static_cast<float4>(v); // expected-error {{cannot convert}}
  float resource = static_cast<float>(texture); // expected-error {{cannot convert}}
}

void aggregate_casts(S s, float value) {
  S zero = static_cast<S>(0); // expected-error {{cannot implicitly convert}}
  S splat = static_cast<S>(value); // expected-error {{cannot implicitly convert}}
  Other flattened = static_cast<Other>(s); // expected-error {{cannot implicitly convert}}
  float scalar = static_cast<float>(s); // expected-error {{cannot implicitly convert}}
  float array[2] = static_cast<float[2]>(value); // expected-error {{cannot implicitly convert}}
  static_cast<S>(s).value = value; // expected-error {{expression is not assignable}}

  S c_zero = (S)0;
  S c_splat = (S)value;
  Other c_flattened = (Other)s;
  float c_scalar = (float)s;
  float c_array[2] = (float[2])value;
}

template <typename T>
T invalid_template(float2 value) {
  return static_cast<T>(value); // expected-error {{cannot convert}}
}

void instantiate(float2 value) {
  float4 result = invalid_template<float4>(value); // expected-note {{in instantiation of function template specialization}}
}

template <typename T, typename U>
T invalid_aggregate_template(U value) {
  return static_cast<T>(value); // expected-error {{cannot implicitly convert}}
}

void instantiate_aggregate(S value) {
  Other result = invalid_aggregate_template<Other>(value); // expected-note {{in instantiation of function template specialization}}
}
