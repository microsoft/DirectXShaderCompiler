// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -E main %s -ast-dump-implicit | FileCheck %s

// CHECK: FunctionDecl {{.*}} implicit used __builtin_LinAlg_MatrixSetElement 'void (__builtin_LinAlgMatrix {{.*}}, __builtin_LinAlgMatrix {{.*}}, unsigned int, unsigned int)' extern
// CHECK-NEXT: ParmVarDecl {{.*}} ret '__builtin_LinAlgMatrix {{.*}}'
// CHECK-NEXT: ParmVarDecl {{.*}} matrix '__builtin_LinAlgMatrix {{.*}}'
// CHECK-NEXT: ParmVarDecl {{.*}} threadLocalIndex 'unsigned int'
// CHECK-NEXT: ParmVarDecl {{.*}} value 'unsigned int'
// CHECK-NEXT: HLSLIntrinsicAttr {{.*}} Implicit "op" "" 412
// CHECK-NEXT: AvailabilityAttr {{.*}} Implicit  6.10 0 0 ""

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 2, 2)]] mat1;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat2;
  __builtin_LinAlg_MatrixSetElement(mat2, mat1, 1, 1);
}
