// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -ast-dump %s | FileCheck %s

// CHECK: CXXRecordDecl {{.*}} struct S definition
// CHECK: FieldDecl {{.*}} handle '__builtin_LinAlg_Matrix':'__builtin_LinAlg_Matrix'
struct S {
  __builtin_LinAlg_Matrix handle;
};

// CHECK: VarDecl {{.*}} global_handle '__builtin_LinAlg_Matrix':'__builtin_LinAlg_Matrix'
__builtin_LinAlg_Matrix global_handle;

// CHECK: FunctionDecl {{.*}} f1 'void (__builtin_LinAlg_Matrix)'
// CHECK: ParmVarDecl {{.*}} m '__builtin_LinAlg_Matrix':'__builtin_LinAlg_Matrix'
void f1(__builtin_LinAlg_Matrix m);

// CHECK: FunctionDecl {{.*}} f2 '__builtin_LinAlg_Matrix ()'
__builtin_LinAlg_Matrix f2();

// CHECK: FunctionDecl {{.*}} main 'void ()'
// CHECK-NEXT: CompoundStmt
// CHECK-NEXT: DeclStmt
// CHECK-NEXT: VarDecl {{.*}} m '__builtin_LinAlg_Matrix':'__builtin_LinAlg_Matrix'
[shader("compute")]
[numthreads(4,1,1)]
void main() {
  __builtin_LinAlg_Matrix m;
}
