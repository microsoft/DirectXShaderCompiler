// RUN: %dxc -T lib_6_3 -HV 202x -Wno-c++14-extensions -verify %s

// These expressions are currently unsupported in constant evaluation but could
// be supported in the future.

constexpr bool sincos_outputs(float value) { // expected-error{{constexpr function never produces a constant expression}}
  float sine = 0.0f;
  float cosine = 0.0f;
  sincos(value, sine, cosine); // expected-note{{non-literal type 'void' cannot be used in a constant expression}} expected-note{{subexpression not valid in a constant expression}}
  return sine == 0.0f && cosine == 1.0f;
}

constexpr bool modf_outputs(float value) {
  float integral = 0.0f;
  float fractional = modf(value, integral); // expected-note{{subexpression not valid in a constant expression}}
  return fractional == -0.5f && integral == -2.0f;
}

constexpr bool frexp_outputs(float value) {
  float exponent = 0.0f;
  float mantissa = frexp(value, exponent); // expected-note{{subexpression not valid in a constant expression}}
  return mantissa == 0.5f && exponent == 4.0f;
}

static_assert(sincos_outputs(0.0f), "sincos return through out parameters"); // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{in call to 'sincos_outputs(0.000000e+00)'}}
static_assert(modf_outputs(-2.5f), "modf return and out parameter"); // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{in call to 'modf_outputs(-2.500000e+00)'}}
static_assert(frexp_outputs(8.0f), "frexp return and out parameter"); // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{in call to 'frexp_outputs(8.000000e+00)'}}
