// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials -generated-only %s | count 0

// When user::ad::fwd::f already exists in the translation unit, the
// rewriter must not emit another forward-mode implementation for f.
// The existing user-provided body is preserved as-is, and no
// auto-generated `user::ad::fwd` block referring to f is appended.
//
// Minimal stub types are declared locally so that the rewriter can parse
// the user-provided forward function without dragging in the full
// hlsl/ad/fwd header (which requires HLSL 2021 + a working enable_if).

template <typename T> struct Value { T v; };

// CHECK: float f(float x)
// CHECK: namespace user
// CHECK: namespace ad
// CHECK: namespace fwd
// CHECK: Value<float> f(Value<float> x)
// CHECK: return x;
// The auto-generated body would have been ((x * x) + x); make sure it
// never appears anywhere in the output.
// CHECK-NOT: ((x * x) + x)

[[dxc::AutoDiff(fwd)]]
float f(float x) {
  return x * x + x;
}

namespace user { namespace ad { namespace fwd {
Value<float> f(Value<float> x) {
    return x;
}
} } }

float main(float x : A) : SV_Target { return f(x); }
