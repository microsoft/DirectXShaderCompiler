// RUN: %dxr -generate-differentials %s | FileCheck %s

// A single class containing both an (fwd, bwd) method and a (bwd)-only
// method produces two separate `user::ad::{fwd,bwd}::C` blocks. The fwd
// wrapper contains only the (fwd, bwd) method; the bwd wrapper contains
// both methods.

// CHECK: #include <ad/fwd>
// CHECK: #include <ad/bwd>
// CHECK: struct Foo

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: struct Foo : ::Foo {
// CHECK: Value<float> doublescale(Value<float> k)
// CHECK-NOT: onlybwd
// CHECK: } } } // namespace user::ad::fwd

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: struct Foo : ::Foo {
// CHECK: float doublescale(inout GradientContext<float> context, Variable<float> k, float __dxc_ad_seed)
// CHECK: float onlybwd(inout GradientContext<float> context, Variable<float> a, float __dxc_ad_seed)
// CHECK: } } } // namespace user::ad::bwd

struct Foo {
  float scale;
  [[dxc::AutoDiff(fwd, bwd)]]
  float doublescale(float k) { return scale + k; }
  [[dxc::AutoDiff(bwd)]]
  float onlybwd(float a) { return a * a; }
};

float main(float x : A) : SV_Target {
  Foo f;
  return f.doublescale(x);
}
