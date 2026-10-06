// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// This test verifies that constexpr variable declarations are rejected when
// their initializer is not a constant expression. The compiler must diagnose
// each such case during Sema (before code generation).

//===----------------------------------------------------------------------===//
// Non-const globals (cbuffer-backed) cannot initialize a constexpr variable
//===----------------------------------------------------------------------===//

int g_runtime;            // cbuffer-backed, not a constant expression
float g_runtime_f;
uint g_runtime_u;

constexpr int kFromRuntime = g_runtime;        // expected-error{{constexpr variable 'kFromRuntime' must be initialized by a constant expression}}
constexpr float kFromRuntimeF = g_runtime_f;   // expected-error{{constexpr variable 'kFromRuntimeF' must be initialized by a constant expression}}
constexpr uint kFromRuntimeU = g_runtime_u;    // expected-error{{constexpr variable 'kFromRuntimeU' must be initialized by a constant expression}}

// Arithmetic on a runtime value is still not constant.
constexpr int kRuntimePlusOne = g_runtime + 1; // expected-error{{constexpr variable 'kRuntimePlusOne' must be initialized by a constant expression}}

// Every constexpr variable declaration requires an initializer.
constexpr int kMissingInitializer; // expected-error{{constexpr variable declaration must be a definition}}
static constexpr int kMissingStaticInitializer; // expected-error{{constexpr variable declaration must be a definition}}

//===----------------------------------------------------------------------===//
// Non-constexpr function calls cannot initialize a constexpr variable
//===----------------------------------------------------------------------===//

int plain_fn(int x) { return x + 1; }
float plain_fn_f(float x) { return x * 2.0f; }

constexpr int kFromPlainFn = plain_fn(1);         // expected-error{{constexpr variable 'kFromPlainFn' must be initialized by a constant expression}}
constexpr float kFromPlainFnF = plain_fn_f(1.0f); // expected-error{{constexpr variable 'kFromPlainFnF' must be initialized by a constant expression}}

//===----------------------------------------------------------------------===//
// Non-const globals cannot be used as array bounds via constexpr
//===----------------------------------------------------------------------===//

// A constexpr variable that fails to initialize cannot be used to size an
// array. The constexpr initializer error fires; the array decl is a
// downstream consequence we don't pin down here.
constexpr int kBadBound = g_runtime; // expected-error{{constexpr variable 'kBadBound' must be initialized by a constant expression}}

//===----------------------------------------------------------------------===//
// Local scope: parameters and non-const locals are not constant expressions
//===----------------------------------------------------------------------===//

void use_param(int p) {                  // expected-note 2 {{declared here}}
  constexpr int kFromParam = p;          // expected-error{{constexpr variable 'kFromParam' must be initialized by a constant expression}} expected-note{{read of non-const variable 'p' is not allowed in a constant expression}}
  constexpr int kFromParamPlus = p + 1;  // expected-error{{constexpr variable 'kFromParamPlus' must be initialized by a constant expression}} expected-note{{read of non-const variable 'p' is not allowed in a constant expression}}
  (void)kFromParam;
  (void)kFromParamPlus;
}

void use_local() {
  constexpr int kMissingLocalInitializer; // expected-error{{constexpr variable declaration must be a definition}}

  int local = 7;                            // not const   expected-note 2 {{declared here}}
  constexpr int kFromLocal = local;         // expected-error{{constexpr variable 'kFromLocal' must be initialized by a constant expression}} expected-note{{read of non-const variable 'local' is not allowed in a constant expression}}
  constexpr int kFromLocalCalc = local * 2; // expected-error{{constexpr variable 'kFromLocalCalc' must be initialized by a constant expression}} expected-note{{read of non-const variable 'local' is not allowed in a constant expression}}
  (void)kFromLocal;
  (void)kFromLocalCalc;

  static int s_local = 3;                   // mutable static, not constant   expected-note{{declared here}}
  constexpr int kFromStatic = s_local;      // expected-error{{constexpr variable 'kFromStatic' must be initialized by a constant expression}} expected-note{{read of non-const variable 's_local' is not allowed in a constant expression}}
  (void)kFromStatic;
}

void use_plain_fn_local() {
  constexpr int kFromPlainFnLocal = plain_fn(2); // expected-error{{constexpr variable 'kFromPlainFnLocal' must be initialized by a constant expression}}
  (void)kFromPlainFnLocal;
}

//===----------------------------------------------------------------------===//
// static_assert with non-constant operands also fails
//===----------------------------------------------------------------------===//

static_assert(g_runtime == 0, "runtime in static_assert"); // expected-error{{static_assert expression is not an integral constant expression}}
static_assert(plain_fn(1) == 2, "plain fn in static_assert"); // expected-error{{static_assert expression is not an integral constant expression}}

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
  use_param(1);
  use_local();
  use_plain_fn_local();
}
