// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials -generated-only %s 2>&1 | FileCheck %s --check-prefix=GENERATED-ERROR

// GENERATED-ERROR: -generated-only cannot merge generated methods into authored differential wrapper 'Foo'

// When the user provides user::ad::fwd::Foo with a SUBSET of the
// annotated methods implemented, the rewriter must merge the missing
// methods into the user's class instead of emitting a separate
// (redefinition) wrapper. The user's `a` survives verbatim; the
// rewriter appends an auto-generated `b` to the same struct.

template <typename T> struct Value { T v; };

struct Foo {
  float scale;
  [[dxc::AutoDiff(fwd)]]
  float a(float k) { return scale * k; }
  [[dxc::AutoDiff(fwd)]]
  float b(float k) { return k + k; }
};

namespace user { namespace ad { namespace fwd {
struct Foo : ::Foo {
  Value<float> a(Value<float> k) { return k; }
};
} } }

// CHECK: #include <ad/fwd>
// The user's wrapper appears, augmented with the auto-generated `b`.
// CHECK: namespace user
// CHECK: namespace ad
// CHECK: namespace fwd
// CHECK: using namespace ::ad::fwd;
// CHECK: struct Foo : public ::Foo
// CHECK: Value<float> {{(fwd::)?}}a(Value<float> k)
// CHECK: return k;
// CHECK: Value<float> b(Value<float> k)
// CHECK: (k + k)
// And the rewriter must NOT also emit a separate wrapper after the
// source struct (which would redefine Foo and fail to compile).
// CHECK-NOT: } } } // namespace user::ad::fwd

float main(float x : A) : SV_Target { Foo f; return f.a(x); }
