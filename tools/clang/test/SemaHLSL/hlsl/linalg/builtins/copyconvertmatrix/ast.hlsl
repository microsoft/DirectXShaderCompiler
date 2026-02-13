// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -E main %s -ast-dump-implicit | FileCheck %s

// CHECK: FunctionDecl {{.*}} implicit used __builtin_LinAlg_CopyConvertMatrix 'void (__builtin_LinAlgMatrix {{.*}}, __builtin_LinAlgMatrix {{.*}}, bool)' extern
// CHECK-NEXT: ParmVarDecl {{.*}} ret '__builtin_LinAlgMatrix {{.*}}'
// CHECK-NEXT: ParmVarDecl {{.*}} source '__builtin_LinAlgMatrix {{.*}}'
// CHECK-NEXT: ParmVarDecl {{.*}} transpose 'bool'
// CHECK-NEXT: HLSLIntrinsicAttr {{.*}} Implicit "op" "" 405
// CHECK-NEXT: AvailabilityAttr {{.*}} Implicit  6.10 0 0 ""

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 2, 2)]] mat1;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat2;
  __builtin_LinAlg_CopyConvertMatrix(mat2, mat1, true);
}
