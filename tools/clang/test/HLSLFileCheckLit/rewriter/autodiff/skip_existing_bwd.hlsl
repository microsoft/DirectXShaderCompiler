// RUN: %dxr -generate-differentials %s | FileCheck %s

// When user::ad::bwd::f already exists in the translation unit, the
// rewriter must not emit another backward-mode implementation for f.
//
// Minimal stub types are declared locally so that the rewriter can parse
// the user-provided backward function without dragging in the full
// hlsl/ad/bwd header.

template <typename T> struct Variable { T v; };
template <typename T> struct GradientContext { T g; };

// CHECK: float f(float x)
// CHECK: namespace user
// CHECK: namespace ad
// CHECK: namespace bwd
// CHECK: Variable<float> f(inout GradientContext<float> context, Variable<float> x)
// The auto-generated rewriter body declares a VariableExpr<T> for every
// parameter and chains `add`/`multiply` calls. Make sure neither of those
// appear in the output - the user's body must be the only one emitted.
// CHECK-NOT: makeVariableExpr<float>(x)
// CHECK-NOT: add<float>

[[dxc::AutoDiff(bwd)]]
float f(float x) {
  return x * x + x;
}

namespace user { namespace ad { namespace bwd {
Variable<float> f(inout GradientContext<float> context, Variable<float> x) {
    return x;
}
} } }

float main(float x : A) : SV_Target { return f(x); }
