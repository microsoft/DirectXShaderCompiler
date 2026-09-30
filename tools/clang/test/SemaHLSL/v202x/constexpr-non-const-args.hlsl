// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// This test verifies the compiler's behavior when a constexpr function is
// invoked with arguments that are not constant expressions.
//
// Calling a constexpr function with non-constexpr arguments is perfectly
// legal: the call is simply not a constant expression and is evaluated at
// runtime like any ordinary function call.
//
// What *is* an error is requiring the result of such a call to be a constant
// expression -- e.g. using it to initialize a `constexpr` variable, as an
// array bound, or inside `static_assert`. Those cases must be diagnosed
// during Sema.

constexpr int square(int x) { return x * x; }
constexpr int add(int a, int b) { return a + b; }
constexpr float fmul(float a, float b) { return a * b; }

//===----------------------------------------------------------------------===//
// Non-constant sources used as arguments
//===----------------------------------------------------------------------===//

int   g_runtime;     // cbuffer-backed
float g_runtime_f;

int   plain(int x) { return x; }  // not constexpr

//===----------------------------------------------------------------------===//
// constexpr function call with a runtime global as argument
//
// Forcing the call into a constexpr / constant-expression context fails.
//===----------------------------------------------------------------------===//

constexpr int kSqRuntime = square(g_runtime); // expected-error{{constexpr variable 'kSqRuntime' must be initialized by a constant expression}}
constexpr int kAddRuntime = add(g_runtime, 1); // expected-error{{constexpr variable 'kAddRuntime' must be initialized by a constant expression}}
constexpr float kFmulRuntime = fmul(g_runtime_f, 2.0f); // expected-error{{constexpr variable 'kFmulRuntime' must be initialized by a constant expression}}

// A constexpr call wrapping the result of a non-constexpr function is also
// not a constant expression.
constexpr int kSqPlain = square(plain(3)); // expected-error{{constexpr variable 'kSqPlain' must be initialized by a constant expression}}

//===----------------------------------------------------------------------===//
// constexpr function call with a non-constant argument inside static_assert
//===----------------------------------------------------------------------===//

static_assert(square(g_runtime) == 0, "sq runtime"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(add(g_runtime, 1) == 1, "add runtime"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(square(plain(2)) == 4, "sq plain"); // expected-error{{static_assert expression is not an integral constant expression}}

//===----------------------------------------------------------------------===//
// Local-scope: function parameters and non-const locals as arguments
//===----------------------------------------------------------------------===//

void f_param(int p) { // expected-note 2 {{declared here}}
  // Calling constexpr functions with a non-constexpr parameter is fine
  // *at runtime*; just don't ask for the result to be a constant.
  int ok_runtime = square(p);  // OK: runtime use
  (void)ok_runtime;

  constexpr int kBad = square(p); // expected-error{{constexpr variable 'kBad' must be initialized by a constant expression}} expected-note{{read of non-const variable 'p' is not allowed in a constant expression}}
  (void)kBad;

  static_assert(square(p) == 0, "param in static_assert"); // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{read of non-const variable 'p' is not allowed in a constant expression}}
}

void f_local() {
  int local = 4;                 // not const   expected-note 2 {{declared here}}
  int ok = add(local, 1);        // OK: ordinary runtime call
  (void)ok;

  constexpr int kBad = add(local, 1); // expected-error{{constexpr variable 'kBad' must be initialized by a constant expression}} expected-note{{read of non-const variable 'local' is not allowed in a constant expression}}
  (void)kBad;

  static_assert(add(local, 0) == 4, "local in static_assert"); // expected-error{{static_assert expression is not an integral constant expression}} expected-note{{read of non-const variable 'local' is not allowed in a constant expression}}
}

//===----------------------------------------------------------------------===//
// Mixed: one constexpr arg, one non-constexpr arg
//
// A single non-constant argument is enough to defeat constant evaluation.
//===----------------------------------------------------------------------===//

constexpr int kConst = 7;

constexpr int kMixed = add(kConst, g_runtime); // expected-error{{constexpr variable 'kMixed' must be initialized by a constant expression}}
static_assert(add(kConst, g_runtime) == 7, "mixed args"); // expected-error{{static_assert expression is not an integral constant expression}}

//===----------------------------------------------------------------------===//
// Nested: outer constexpr call where an inner call has non-constexpr args
//===----------------------------------------------------------------------===//

constexpr int kNested = square(add(g_runtime, 1)); // expected-error{{constexpr variable 'kNested' must be initialized by a constant expression}}

//===----------------------------------------------------------------------===//
// Runtime-only uses are accepted: calling constexpr fns with non-constexpr
// args is fine as long as the result isn't required to be constant.
//===----------------------------------------------------------------------===//

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
  int v = square(g_runtime);          // ok: runtime use
  v = add(v, g_runtime);              // ok
  v = (int)fmul(g_runtime_f, 2.0f);   // ok
  (void)v;

  // Parameter -> constexpr fn -> runtime variable is fine.
  int p = 3;
  int q = square(p);
  (void)q;

  f_param(1);
  f_local();
}
