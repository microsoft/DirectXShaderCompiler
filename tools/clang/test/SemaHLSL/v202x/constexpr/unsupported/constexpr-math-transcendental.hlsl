// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// These expressions are currently unsupported in constant evaluation but could
// be supported in the future.

// Use exactly representable results to avoid approximation-dependent checks.
static_assert(acos(1.0f) == 0.0f, "acos"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(asin(0.0f) == 0.0f, "asin"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(atan(0.0f) == 0.0f, "atan"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(atan2(0.0f, 1.0f) == 0.0f, "atan2"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(cos(0.0f) == 1.0f, "cos"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(cosh(0.0f) == 1.0f, "cosh"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(sin(0.0f) == 0.0f, "sin"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(sinh(0.0f) == 0.0f, "sinh"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(tan(0.0f) == 0.0f, "tan"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(tanh(0.0f) == 0.0f, "tanh"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(degrees(0.0f) == 0.0f, "degrees"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(radians(0.0f) == 0.0f, "radians"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(exp(0.0f) == 1.0f, "exp"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(exp2(3.0f) == 8.0f, "exp2"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(log(1.0f) == 0.0f, "log"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(log10(1.0f) == 0.0f, "log10"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(log2(8.0f) == 3.0f, "log2"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(pow(2.0f, 3.0f) == 8.0f, "pow"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(rcp(4.0f) == 0.25f, "rcp"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(sqrt(4.0f) == 2.0f, "sqrt"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(rsqrt(4.0f) == 0.5f, "rsqrt"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(ldexp(0.5f, 3.0f) == 4.0f, "ldexp"); // expected-error{{static_assert expression is not an integral constant expression}}

static_assert(isfinite(1.0f), "finite value"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(!isinf(1.0f), "non-infinite value"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(!isnan(1.0f), "non-NaN value"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(isnormal(1.0f), "normal value"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(!isnormal(0.0f), "zero is not normal"); // expected-error{{static_assert expression is not an integral constant expression}}
