// RUN: FileCheck %s --input-file="%hlsl_src_headers/ad/fwd"

// Symbol-coverage regression test for the hlsl/ad/fwd library.
//
// The forward-mode auto-diff library must provide a Value<T> overload for
// every differentiable HLSL intrinsic recognised by the dxr rewriter
// (GetBackwardIntrinsicBuilder in dxcrewriteautodiff.cpp). The list below
// asserts each entry is declared in the library header.

// Trig (sin/cos are declared via MAKE_UNARY_OP earlier in the file).
// CHECK-DAG: MAKE_UNARY_OP(sin, SinExpr)
// CHECK-DAG: MAKE_UNARY_OP(cos, CosExpr)
// CHECK-DAG: Value<T> tan(Value<T>
// CHECK-DAG: Value<T> asin(Value<T>
// CHECK-DAG: Value<T> acos(Value<T>
// CHECK-DAG: Value<T> atan(Value<T>
// CHECK-DAG: Value<T> atan2(Value<T>
// CHECK-DAG: Value<T> sinh(Value<T>
// CHECK-DAG: Value<T> cosh(Value<T>
// CHECK-DAG: Value<T> tanh(Value<T>

// Exp / log family.
// CHECK-DAG: MAKE_UNARY_OP(exp, ExpExpr)
// CHECK-DAG: MAKE_UNARY_OP(log, LogExpr)
// CHECK-DAG: Value<T> exp2(Value<T>
// CHECK-DAG: Value<T> log2(Value<T>
// CHECK-DAG: Value<T> log10(Value<T>

// Roots / reciprocals.
// CHECK-DAG: MAKE_UNARY_OP(sqrt, SqrtExpr)
// CHECK-DAG: Value<T> rsqrt(Value<T>
// CHECK-DAG: Value<T> rcp(Value<T>

// Smooth algebraic.
// CHECK-DAG: Value<T> abs(Value<T>
// CHECK-DAG: Value<T> saturate(Value<T>
// CHECK-DAG: Value<T> clamp(Value<T>
// CHECK-DAG: Value<T> max(Value<T>
// CHECK-DAG: Value<T> min(Value<T>
// CHECK-DAG: Value<T> lerp(Value<T>
// CHECK-DAG: Value<T> mad(Value<T>
// CHECK-DAG: Value<T> fma(Value<T>
// CHECK-DAG: Value<T> smoothstep(Value<T>

// Piecewise-constant.
// CHECK-DAG: Value<T> sign(Value<T>
// CHECK-DAG: Value<T> step(Value<T>
// CHECK-DAG: Value<T> floor(Value<T>
// CHECK-DAG: Value<T> ceil(Value<T>
// CHECK-DAG: Value<T> round(Value<T>
// CHECK-DAG: Value<T> trunc(Value<T>
// CHECK-DAG: Value<T> frac(Value<T>
// CHECK-DAG: Value<T> fmod(Value<T>
// CHECK-DAG: Value<T> modf(Value<T>

// Unit conversion.
// CHECK-DAG: Value<T> degrees(Value<T>
// CHECK-DAG: Value<T> radians(Value<T>
// CHECK-DAG: Value<T> ldexp(Value<T>

// Geometric (scalar variants for distance/reflect/refract/faceforward; the
// vector-mode dot/cross/length/normalize live in the expression-template
// section earlier in the file).
// CHECK-DAG: MAKE_VECTOR_BINARY_OP(dot, DotExpr)
// CHECK-DAG: MAKE_VECTOR_BINARY_OP(cross, CrossExpr)
// CHECK-DAG: MAKE_VECTOR_UNARY_OP(length, LengthExpr)
// CHECK-DAG: MAKE_VECTOR_UNARY_OP(normalize, NormalizeExpr)
// CHECK-DAG: Value<T> distance(Value<T>
// CHECK-DAG: Value<T> reflect(Value<T>
// CHECK-DAG: Value<T> refract(Value<T>
// CHECK-DAG: Value<T> faceforward(Value<T>

// Matrix (already in the earlier expression-template section).
// CHECK-DAG: MatMulExpr<T, N, K, M, L, R> matMul
// CHECK-DAG: TransposeExpr<T, N, M, E> transposeExpr
// CHECK-DAG: DetExpr<T, N, E> determinantExpr

// Misc smooth.
// CHECK-DAG: Value<T> lit(Value<T>
