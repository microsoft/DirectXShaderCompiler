// RUN: %dxr -generate-differentials %s | FileCheck %s

// Backward-mode autodiff on a member function generates a wrapper struct
// in user::ad::bwd with a Variable<T>-based method whose body chains
// expression builders just like the free-function case.

// CHECK: #include <ad/bwd>
// CHECK: struct Box
// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: using namespace ::ad::bwd;
// CHECK: struct Box : ::Box {
// CHECK: float volume(inout GradientContext<float> context, Variable<float> h, float __dxc_ad_seed)
// CHECK: VariableExpr<float> h_expr = makeVariableExpr<float>(h);
// CHECK: multiply<float>
// CHECK: } } } // namespace user::ad::bwd

struct Box {
  float base;
  [[dxc::AutoDiff(bwd)]]
  float volume(float h) {
    return base * h;
  }
};

float main(float x : A) : SV_Target {
  Box b;
  b.base = 1.0;
  return b.volume(x);
}
