// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// These expressions are currently unsupported in constant evaluation but could
// be supported in the future.

static_assert(abs(-3) == 3, "integer abs"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(abs(-2.5f) == 2.5f, "floating-point abs"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(min(-3, 2) == -3, "signed min");
static_assert(min(3u, 2u) == 2u, "unsigned min"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(min(1.5f, 2.5f) == 1.5f, "floating-point min");
static_assert(max(-3, 2) == 2, "signed max");
static_assert(max(3u, 2u) == 3u, "unsigned max"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(max(1.5f, 2.5f) == 2.5f, "floating-point max");
static_assert(clamp(-2, 0, 4) == 0, "clamp below range"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(clamp(2u, 0u, 4u) == 2u, "clamp within range"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(clamp(5.0f, 0.0f, 4.0f) == 4.0f, "clamp above range"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(mad(2, 3, 4) == 10, "signed mad"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(mad(2u, 3u, 4u) == 10u, "unsigned mad"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(mad(2.0f, 3.0f, 4.0f) == 10.0f, "floating-point mad"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(fma(2.0L, 3.0L, 4.0L) == 10.0L, "double fma"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(sign(-2.0f) == -1, "negative sign"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(sign(0) == 0, "zero sign"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(sign(2u) == 1, "positive sign"); // expected-error{{static_assert expression is not an integral constant expression}}

static_assert(ceil(-1.25f) == -1.0f, "ceil"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(floor(-1.25f) == -2.0f, "floor"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(round(1.75f) == 2.0f, "round"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(trunc(-1.75f) == -1.0f, "trunc"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(frac(-1.25f) == 0.75f, "negative frac"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(fmod(-5.5f, 2.0f) == -1.5f, "negative fmod"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(saturate(-0.5f) == 0.0f, "saturate below range"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(saturate(0.5f) == 0.5f, "saturate within range"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(saturate(1.5f) == 1.0f, "saturate above range"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(lerp(2.0f, 6.0f, 0.25f) == 3.0f, "lerp"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(step(2.0f, 1.0f) == 0.0f, "step below edge"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(step(2.0f, 2.0f) == 1.0f, "step at edge"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(smoothstep(0.0f, 2.0f, 1.0f) == 0.5f, "smoothstep"); // expected-error{{static_assert expression is not an integral constant expression}}

constexpr float arithmetic(float value) {
  return clamp(abs(value), 0.0f, 4.0f); // expected-note{{subexpression not valid in a constant expression}}
}

static_assert(arithmetic(-2.0f) == 2.0f, "intrinsics in constexpr function"); // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{in call to 'arithmetic(-2.000000e+00)'}}
