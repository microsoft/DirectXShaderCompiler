// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// In HLSL 202x, 'static_assert' is recognized as a keyword and accepts
// both the C++11 form (condition + message) and the C++17 form
// (condition only). Statically-verifiable, true conditions produce no
// diagnostic; false conditions produce an error; non-constant
// conditions produce an error indicating the expression is not a
// constant.

// --- True conditions: no diagnostics expected for these. ---

static_assert(1 == 1, "math: equality is reflexive");
static_assert(sizeof(float) == 4, "float is 32 bits");
static_assert(2 + 2 == 4, "arithmetic still works");

// C++17 form: no message.
static_assert(1 < 2);
static_assert(sizeof(int) == 4);

// In a function body.
void in_function_body() {
  static_assert(sizeof(float4) == 16, "float4 is 16 bytes");
  static_assert(sizeof(float4) == 16);
}

// In a struct/class member-specification.
struct S {
  static_assert(sizeof(float) == 4, "float is 4 bytes inside a struct");
  static_assert(sizeof(float) == 4);
  float member;
};

// --- False conditions: each should trigger a static_assert failure. ---

static_assert(1 == 2, "one is not two"); // expected-error{{static_assert failed "one is not two"}}
static_assert(sizeof(float) == 8, "float is not 8 bytes"); // expected-error{{static_assert failed "float is not 8 bytes"}}

// C++17 form (no message) failing.
static_assert(1 == 2); // expected-error{{static_assert failed}}

// --- Non-constant conditions: should be rejected as not constant. ---

cbuffer CB { int g_runtime; };

static_assert(g_runtime == 0, "runtime value"); // expected-error{{static_assert expression is not an integral constant expression}}

void uses_param(int p) { // expected-note{{declared here}}
  static_assert(p == 0, "parameter is runtime"); // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{read of non-const variable 'p' is not allowed in a constant expression}}
}
