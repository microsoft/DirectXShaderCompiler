// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s -ast-dump-implicit | FileCheck %s --check-prefix AST
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s --check-prefix DXIL

// AST: |-CXXRecordDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit referenced class __builtin_la_MatrixRef definition
// AST-NEXT: | |-FinalAttr {{[^ ]+}} <<invalid sloc>> Implicit final
// AST-NEXT: | |-AvailabilityAttr {{[^ ]+}} <<invalid sloc>> Implicit  6.10 0 0 ""
// AST-NEXT: | |-HLSLMatrixRefAttr {{[^ ]+}} <<invalid sloc>> Implicit
// AST-NEXT: | |-FieldDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit h 'int'
// AST-NEXT: | |-CXXConstructorDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> __builtin_la_MatrixRef 'void ()'
// AST-NEXT: | | |-HLSLIntrinsicAttr {{[^ ]+}} <<invalid sloc>> Implicit "op" "" 395
// AST-NEXT: | | `-HLSLCXXOverloadAttr {{[^ ]+}} <<invalid sloc>> Implicit
// AST-NEXT: | `-CXXDestructorDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit referenced ~__builtin_la_MatrixRef 'void () noexcept' inline


// AST: `-FunctionDecl {{[^ ]+}} <<invalid sloc>> <invalid sloc> implicit used __builtin_la_CreateMatrix '__builtin_la_MatrixRef ()' extern
// AST-NEXT: |-HLSLIntrinsicAttr {{[^ ]+}} <<invalid sloc>> Implicit "op" "" 395
// AST-NEXT: |-AvailabilityAttr {{[^ ]+}} <<invalid sloc>> Implicit  6.10 0 0 ""

// DXIL-LABEL: define void @main()
[numthreads(4,1,1)]
void main() {
  // DXIL: call %dx.types.MatrixRef @dx.op.createMatrix(i32 312)  ; CreateMatrix()
  __builtin_la_MatrixRef mat = __builtin_la_CreateMatrix();
}
