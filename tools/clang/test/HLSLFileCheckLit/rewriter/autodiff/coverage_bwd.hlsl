// RUN: FileCheck %s --input-file="%hlsl_src_headers/ad/bwd"

// Symbol-coverage regression test for the hlsl/ad/bwd library.
//
// The backward-mode auto-diff library must provide a builder function for
// every differentiable HLSL intrinsic recognised by the dxr rewriter
// (GetBackwardIntrinsicBuilder in dxcrewriteautodiff.cpp). The builder names
// here exactly match the strings produced by that table.

// Existing pre-coverage entries (sin/cos/exp/log/sqrt/log2/max/min/...).
// CHECK-DAG: MAKE_UNARY_OP_BACK(BackSinExpr, sinExpr)
// CHECK-DAG: MAKE_UNARY_OP_BACK(BackCosExpr, cosExpr)
// CHECK-DAG: MAKE_UNARY_OP_BACK(BackExpExpr, expExpr)
// CHECK-DAG: MAKE_UNARY_OP_BACK(BackLogExpr, logExpr)
// CHECK-DAG: MAKE_UNARY_OP_BACK(BackSqrtExpr, sqrtExpr)
// CHECK-DAG: MAKE_UNARY_OP_BACK(BackLog2Expr, log2Expr)
// CHECK-DAG: MAKE_BINARY_OP_BACK(BackMaxExpr, maxExpr)
// CHECK-DAG: MAKE_BINARY_OP_BACK(BackMinExpr, minExpr)

// Trig (extension).
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackTanExpr, tanExpr
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackAsinExpr, asinExpr
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackAcosExpr, acosExpr
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackAtanExpr, atanExpr
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackSinhExpr, sinhExpr
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackCoshExpr, coshExpr
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackTanhExpr, tanhExpr

// Exp/log (extension).
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackExp2Expr, exp2Expr
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackLog10Expr, log10Expr

// Roots / reciprocals.
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackRsqrtExpr, rsqrtExpr
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackRcpExpr, rcpExpr

// Algebraic.
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackAbsExpr, absExpr
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackSaturateExpr, saturateExpr

// Piecewise-constant.
// CHECK-DAG: MAKE_BACK_UNARY_ZERO_EXPR(BackSignExpr, signExpr
// CHECK-DAG: MAKE_BACK_UNARY_ZERO_EXPR(BackFloorExpr, floorExpr
// CHECK-DAG: MAKE_BACK_UNARY_ZERO_EXPR(BackCeilExpr, ceilExpr
// CHECK-DAG: MAKE_BACK_UNARY_ZERO_EXPR(BackRoundExpr, roundExpr
// CHECK-DAG: MAKE_BACK_UNARY_ZERO_EXPR(BackTruncExpr, truncExpr
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackFracExpr, fracExpr

// Unit conversion.
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackDegreesExpr, degreesExpr
// CHECK-DAG: MAKE_BACK_UNARY_EXPR(BackRadiansExpr, radiansExpr

// Binary extension.
// CHECK-DAG: MAKE_BACK_BINARY_EXPR(BackAtan2Expr, atan2Expr
// CHECK-DAG: MAKE_BACK_BINARY_EXPR(BackFmodExpr, fmodExpr
// CHECK-DAG: MAKE_BACK_BINARY_EXPR(BackLdexpExpr, ldexpExpr
// CHECK-DAG: MAKE_BACK_BINARY_EXPR(BackStepExpr, stepExpr

// Ternary.
// CHECK-DAG: MAKE_BACK_TERNARY_EXPR(BackClampExpr, clampExpr
// CHECK-DAG: MAKE_BACK_TERNARY_EXPR(BackLerpExpr, lerpExpr
// CHECK-DAG: MAKE_BACK_TERNARY_EXPR(BackMadExpr, madExpr
// CHECK-DAG: MAKE_BACK_TERNARY_EXPR(BackFmaExpr, fmaExpr
// CHECK-DAG: smoothstepExpr(A a, B b, C c)

// Geometric / misc smooth.
// CHECK-DAG: MAKE_BINARY_OP_BACK(BackDistanceExpr, distanceExpr)
// CHECK-DAG: MAKE_BINARY_OP_BACK(BackReflectExpr, reflectExpr)
// CHECK-DAG: refractExpr,
// CHECK-DAG: faceforwardExpr,
// CHECK-DAG: litExpr(A a, B b, C c)
// CHECK-DAG: MAKE_BINARY_OP_BACK(BackDstExpr, dstExpr)
// CHECK-DAG: MAKE_UNARY_OP_BACK(BackModfExpr, modfExpr)

// Matrix.
// CHECK-DAG: MAKE_BINARY_OP_BACK(BackMulExprScalar, mulExpr)
// CHECK-DAG: MAKE_UNARY_OP_BACK(BackTransposeExprScalar, transposeExpr)
// CHECK-DAG: MAKE_UNARY_OP_BACK(BackDeterminantExprScalar, determinantExpr)

// Pre-existing dot/cross/length/normalize/power live in the vector section.
// CHECK-DAG: MAKE_BINARY_OP_BACK(BackDotExpr, dotProduct)
// CHECK-DAG: MAKE_BINARY_OP_BACK(BackCrossExpr, crossProduct)
// CHECK-DAG: MAKE_VECTOR_UNARY_OP_BACK(BackLengthExpr, lengthExpr)
// CHECK-DAG: MAKE_VECTOR_UNARY_OP_BACK(BackNormalizeExpr, normalizeExpr)
