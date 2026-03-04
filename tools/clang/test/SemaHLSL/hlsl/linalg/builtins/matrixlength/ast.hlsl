// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -E main %s -ast-dump-implicit | FileCheck %s

// CHECK: FunctionDecl {{.*}} implicit used __builtin_LinAlg_MatrixLength 'unsigned int (__builtin_LinAlgMatrix {{.*}})' extern
// CHECK-NEXT: ParmVarDecl {{.*}} matrix '__builtin_LinAlgMatrix {{.*}}'
// CHECK-NEXT: HLSLIntrinsicAttr {{.*}} Implicit "op" "" 409
// CHECK-NEXT: AvailabilityAttr {{.*}} Implicit  6.10 0 0 ""

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 2, 2)]] mat;
  __builtin_LinAlg_MatrixLength(mat);
}
