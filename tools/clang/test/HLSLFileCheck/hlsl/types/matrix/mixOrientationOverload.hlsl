// RUN: %dxc -ast-dump -Tlib_6_3 %s | FileCheck %s

// Make sure only one orientation cast created.
// CHECK:FunctionDecl 0x{{.+}} foo 'float4x4 (column_major float4x4, row_major float4x4)'
// CHECK-NEXT: ParmVarDecl 0x[[A:[0-9a-f]+]] {{.+}} col:36 used a 'column_major float4x4':'matrix.internal::matrix<float, 4, 4, 0>'
// CHECK-NEXT: ParmVarDecl 0x[[B:[0-9a-f]+]] {{.+}} col:58 used b 'row_major float4x4':'matrix.internal::matrix<float, 4, 4, 1>'
// CHECK-NEXT: CompoundStmt
// CHECK-NEXT: `-ReturnStmt
// CHECK-NEXT:   `-CallExpr 0x{{.+}} <col:10, col:17> 'matrix<float, 4, 4, 0>':'matrix.internal::matrix<float, 4, 4, 0>'
// CHECK-NEXT:     |-ImplicitCastExpr 0x{{.+}} <col:10> 'matrix<float, 4, 4, 0> (*)(matrix<float, 4, 4>, matrix<float, 4, 4, 0>)' <FunctionToPointerDecay>
// CHECK-NEXT:     | `-DeclRefExpr 0x{{.+}} <col:10> 'matrix<float, 4, 4, 0> (matrix<float, 4, 4>, matrix<float, 4, 4, 0>)' lvalue Function 0x{{.+}} 'mul' 'matrix<float, 4, 4, 0> (matrix<float, 4, 4>, matrix<float, 4, 4, 0>)'
// CHECK-NEXT:     |-ImplicitCastExpr 0x{{.+}} <col:14> 'column_major float4x4':'matrix.internal::matrix<float, 4, 4, 0>' <LValueToRValue>
// CHECK-NEXT:     | `-DeclRefExpr 0x{{.+}} <col:14> 'column_major float4x4':'matrix.internal::matrix<float, 4, 4, 0>' lvalue ParmVar 0x[[A]] 'a' 'column_major float4x4':'matrix.internal::matrix<float, 4, 4, 0>'
// CHECK-NEXT:     `-ImplicitCastExpr 0x{{.+}} <col:16> 'matrix.internal::matrix<float, 4, 4, 0>' <HLSLRowMajorToColMajor>
// CHECK-NEXT:       `-ImplicitCastExpr 0x{{.+}} <col:16> 'row_major float4x4':'matrix.internal::matrix<float, 4, 4, 1>' <LValueToRValue>
// CHECK-NEXT:         `-DeclRefExpr 0x{{.+}} <col:16> 'row_major float4x4':'matrix.internal::matrix<float, 4, 4, 1>' lvalue ParmVar 0x[[B]] 'b' 'row_major float4x4':'matrix.internal::matrix<float, 4, 4, 1>'
// CHECK-NEXT:HLSLExportAttr 0x{{.+}}

export
float4x4 foo(column_major float4x4 a, row_major float4x4 b) {
  return mul(a,b);
}
