// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -E main %s -ast-dump-implicit | FileCheck %s

// CHECK: FunctionDecl {{.*}} implicit used __builtin_LinAlg_MatrixAccumulate 'void (__builtin_LinAlgMatrix {{.*}}, __builtin_LinAlgMatrix {{.*}}, __builtin_LinAlgMatrix {{.*}})' extern
// CHECK-NEXT: ParmVarDecl {{.*}} matrixC '__builtin_LinAlgMatrix {{.*}}'
// CHECK-NEXT: ParmVarDecl {{.*}} matrixLHS '__builtin_LinAlgMatrix {{.*}}'
// CHECK-NEXT: ParmVarDecl {{.*}} matrixRHS '__builtin_LinAlgMatrix {{.*}}'
// CHECK-NEXT: HLSLIntrinsicAttr {{.*}} Implicit "op" "" 415
// CHECK-NEXT: AvailabilityAttr {{.*}} Implicit  6.10 0 0 ""


[shader("compute")]
[numthreads(1,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 2, 2)]] mat;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 2, 2)]] mat2;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 2, 2)]] mat3;
  __builtin_LinAlg_MatrixAccumulate(mat, mat2, mat3);
}
