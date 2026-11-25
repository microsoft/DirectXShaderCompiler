// RUN: %dxr -generate-differentials %s | FileCheck %s

// `f` is annotated with both forward and backward auto-diff modes but `g`
// is annotated with forward mode only. Forward-mode of `f` therefore has a
// matching `user::ad::fwd::g` to dispatch to via unqualified name lookup,
// while backward-mode of `f` still falls into the unknown-callee branch
// because `g` has no backward overload and is not an intrinsic.
//
// The rewriter inspects the callee's canonical redeclaration chain and reports
// the missing backward mode directly.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> g(Value<float> x)
// CHECK: return (x * x);
// CHECK: } } } // namespace user::ad::fwd

// The user does not provide a backward variant of g, and the rewriter
// must not invent one.
// CHECK-NOT: float g(inout GradientContext<float>

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x)
// CHECK: return (g(x) + x);
// CHECK: } } } // namespace user::ad::fwd

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: _Static_assert(false, "auto-diff cannot generate backward-mode for 'f': callee 'g' does not request backward-mode auto-diff");
// CHECK: return (float)0;
// CHECK: } } } // namespace user::ad::bwd

[[dxc::AutoDiff(fwd)]]
float g(float x) { return x * x; }

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x) { return g(x) + x; }

float main(float x : A) : SV_Target { return f(x); }
