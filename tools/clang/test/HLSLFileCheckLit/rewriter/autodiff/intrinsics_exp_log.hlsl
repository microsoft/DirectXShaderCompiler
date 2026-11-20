// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Exponential / logarithm / power family.

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, Variable<float> y, float __dxc_ad_seed)
// CHECK: expExpr<float>(x_expr)
// CHECK: exp2Expr<float>(x_expr)
// CHECK: logExpr<float>(x_expr)
// CHECK: log2Expr<float>(x_expr)
// CHECK: log10Expr<float>(x_expr)
// CHECK: power<float>(x_expr, y_expr)
// CHECK: } } } // namespace user::ad::bwd

[[dxc::AutoDiff(bwd)]]
float f(float x, float y) {
  return exp(x) + exp2(x)
       + log(x) + log2(x) + log10(x)
       + pow(x, y);
}

float main(float x : A) : SV_Target { return f(x, x); }
