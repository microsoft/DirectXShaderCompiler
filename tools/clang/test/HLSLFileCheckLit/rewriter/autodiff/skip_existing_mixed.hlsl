// RUN: %dxr -generate-differentials %s | FileCheck %s

// Mixed-skip behaviour for a function attributed with both modes: when
// only the forward-mode user implementation already exists, the rewriter
// must skip emitting fwd but still emit bwd.

template <typename T> struct Value { T v; };

// CHECK: float f(float x)

// The user-provided forward must remain present (printed via DeclPrinter).
// CHECK: namespace user
// CHECK: namespace ad
// CHECK: namespace fwd
// CHECK: Value<float> f(Value<float> x)
// CHECK: return x;

// The missing backward mode is generated after primal declarations in
// dependency order.
// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: using namespace ::ad::bwd;
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: VariableExpr<float> x_expr = makeVariableExpr<float>(x);
// CHECK: } } } // namespace user::ad::bwd

// Make sure the auto-generated forward body is *not* present.
// CHECK-NOT: return ((x * x) + x);

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x) {
  return x * x + x;
}

namespace user { namespace ad { namespace fwd {
Value<float> f(Value<float> x) {
    return x;
}
} } }

float main(float x : A) : SV_Target { return f(x); }
