// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -E main %s -ast-dump-implicit | FileCheck %s

// CHECK: |-FunctionDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit used __builtin_LinAlg_CreateMatrix '__builtin_LinAlg_MatrixRef ()' extern
// CHECK-NEXT: | |-HLSLIntrinsicAttr {{[^ ]+}} <<invalid sloc>> Implicit "op" "" 406
// CHECK-NEXT: | |-AvailabilityAttr {{[^ ]+}} <<invalid sloc>> Implicit  6.10 0 0 ""
// CHECK-NEXT: | `-HLSLBuiltinCallAttr {{[^ ]+}} <<invalid sloc>> Implicit

// CHECK: `-FunctionDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit used __builtin_LinAlg_FillMatrix 'void (__builtin_LinAlg_MatrixRef, unsigned int)' extern
// CHECK-NEXT: |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> matrix '__builtin_LinAlg_MatrixRef'
// CHECK-NEXT: |-ParmVarDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> value 'unsigned int'
// CHECK-NEXT: |-HLSLIntrinsicAttr {{[^ ]+}} <<invalid sloc>> Implicit "op" "" 407
// CHECK-NEXT: |-AvailabilityAttr {{[^ ]+}} <<invalid sloc>> Implicit  6.10 0 0 ""
// CHECK-NEXT: `-HLSLBuiltinCallAttr {{[^ ]+}} <<invalid sloc>> Implicit

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  __builtin_LinAlg_MatrixRef mat = __builtin_LinAlg_CreateMatrix();
  __builtin_LinAlg_FillMatrix(mat, 15);
}
