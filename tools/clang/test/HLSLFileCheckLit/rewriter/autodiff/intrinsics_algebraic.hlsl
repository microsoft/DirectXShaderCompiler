// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Algebraic / piecewise-smooth intrinsics: sqrt, rsqrt, rcp, abs,
// min/max/clamp, lerp, saturate, smoothstep, step, fmod.

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: sqrtExpr<float>(x_expr)
// CHECK: rsqrtExpr<float>(x_expr)
// CHECK: rcpExpr<float>(x_expr)
// CHECK: absExpr<float>(x_expr)
// CHECK: minExpr<float>(x_expr, y_expr)
// CHECK: maxExpr<float>(x_expr, y_expr)
// CHECK: clampExpr<float>(x_expr, x_expr, y_expr)
// CHECK: lerpExpr<float>(x_expr, y_expr, x_expr)
// CHECK: saturateExpr<float>(x_expr)
// CHECK: stepExpr<float>(x_expr, y_expr)
// CHECK: smoothstepExpr<float>(x_expr, y_expr, x_expr)
// CHECK: fmodExpr<float>(x_expr, y_expr)
// CHECK: } } } // namespace user::ad::bwd

[[dxc::AutoDiff(bwd)]]
float f(float x, float y) {
  return sqrt(x) + rsqrt(x) + rcp(x) + abs(x)
       + min(x, y) + max(x, y) + clamp(x, x, y)
       + lerp(x, y, x) + saturate(x)
       + step(x, y) + smoothstep(x, y, x)
       + fmod(x, y);
}

float main(float x : A) : SV_Target { return f(x, x); }
