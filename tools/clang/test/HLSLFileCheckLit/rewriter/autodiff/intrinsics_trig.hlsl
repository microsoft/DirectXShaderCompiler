// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Trig intrinsics map to *Expr builders in backward mode.

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: VariableExpr<float> x_expr = makeVariableExpr<float>(x);
// CHECK: sinExpr<float>(x_expr)
// CHECK: cosExpr<float>(x_expr)
// CHECK: tanExpr<float>(x_expr)
// CHECK: asinExpr<float>(x_expr)
// CHECK: acosExpr<float>(x_expr)
// CHECK: atanExpr<float>(x_expr)
// CHECK: sinhExpr<float>(x_expr)
// CHECK: coshExpr<float>(x_expr)
// CHECK: tanhExpr<float>(x_expr)
// CHECK: } } } // namespace user::ad::bwd

[[dxc::AutoDiff(bwd)]]
float f(float x) {
  return sin(x) + cos(x) + tan(x)
       + asin(x) + acos(x) + atan(x)
       + sinh(x) + cosh(x) + tanh(x);
}

float main(float x : A) : SV_Target { return f(x); }
