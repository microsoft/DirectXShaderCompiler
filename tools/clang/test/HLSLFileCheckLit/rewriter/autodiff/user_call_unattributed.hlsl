// RUN: %dxr -generate-differentials %s | FileCheck %s

// `f` is annotated with [[dxc::AutoDiff(fwd, bwd)]] but the helper `g` it
// calls is a plain user function with no auto-diff attribute.
//
// In forward mode the generated `user::ad::fwd::f` writes the call as
// `g(x)` unqualified. Because no `user::ad::fwd::g` overload exists, this
// will not type-check against `Value<float>` once compiled, but the
// rewriter emits the call text verbatim so the failure surfaces clearly to
// the user with a normal overload-resolution diagnostic at the next stage.
//
// In backward mode the call is unrecognised by the intrinsic builder
// table, so the rewriter emits a `_Static_assert(false, ...)` stub that
// names the offending callee. This is the documented behaviour for any
// non-differentiable call site (cf. NoDiff_user_call.hlsl); the test
// pins it down for the case where the call appears in the function body
// without an explicit [[dxc::NoDiff]] opt-out.

// CHECK-NOT: namespace user { namespace ad { namespace fwd
// CHECK-NOT: Value<float> g(
// CHECK-NOT: float g(inout GradientContext<float>

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x)
// CHECK: return (g(x) + x);
// CHECK: } } } // namespace user::ad::fwd

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: _Static_assert(false, "auto-diff cannot generate backward-mode for 'f': unknown callee 'g' has no auto-diff builder");
// CHECK: return (float)0;
// CHECK: } } } // namespace user::ad::bwd

float g(float x) { return x * x; }

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x) { return g(x) + x; }

float main(float x : A) : SV_Target { return f(x); }
