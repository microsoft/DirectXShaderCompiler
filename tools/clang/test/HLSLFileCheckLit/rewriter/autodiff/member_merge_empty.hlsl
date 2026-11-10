// RUN: %dxr -generate-differentials %s | FileCheck %s

// When the user provides an EMPTY user::ad::fwd::Foo wrapper, every
// annotated method is added by the rewriter into that same class.
// This is the simplest opt-in pattern for users who want to keep the
// wrapper class declaration in their source but let the tooling fill
// in the bodies.

struct Foo {
  [[dxc::AutoDiff(fwd)]]
  float a(float k) { return k * k; }
};

namespace user { namespace ad { namespace fwd {
struct Foo : ::Foo {};
} } }

// CHECK: #include <ad/fwd>
// CHECK: namespace user
// CHECK: namespace ad
// CHECK: namespace fwd
// CHECK: using namespace ::ad::fwd;
// CHECK: struct Foo : public ::Foo
// CHECK: Value<float> a(Value<float> k)
// CHECK: (k * k)
// CHECK-NOT: } } } // namespace user::ad::fwd

float main(float x : A) : SV_Target { Foo f; return f.a(x); }
