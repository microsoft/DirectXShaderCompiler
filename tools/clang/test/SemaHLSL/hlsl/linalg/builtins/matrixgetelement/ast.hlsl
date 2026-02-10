// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -E main %s -ast-dump-implicit | FileCheck %s

// CHECK: FunctionDecl {{.*}} implicit used __builtin_LinAlg_MatrixGetElement 'void (unsigned int &, __builtin_LinAlgMatrix {{.*}}, unsigned int)' extern
// CHECK-NEXT: ParmVarDecl {{.*}} ret 'unsigned int &&__restrict'
// CHECK-NEXT: ParmVarDecl {{.*}} matrix '__builtin_LinAlgMatrix {{.*}}'
// CHECK-NEXT: ParmVarDecl {{.*}} threadLocalIndex 'unsigned int'
// CHECK-NEXT: HLSLIntrinsicAttr {{.*}} Implicit "op" "" 408
// CHECK-NEXT: AvailabilityAttr {{.*}} Implicit  6.10 0 0 ""

// CHECK: FunctionDecl {{.*}} implicit used __builtin_LinAlg_MatrixGetElement 'void (float &, __builtin_LinAlgMatrix {{.*}}, unsigned int)' extern
// CHECK-NEXT: ParmVarDecl {{.*}} ret 'float &&__restrict'
// CHECK-NEXT: ParmVarDecl {{.*}} matrix '__builtin_LinAlgMatrix {{.*}}'
// CHECK-NEXT: ParmVarDecl {{.*}} threadLocalIndex 'unsigned int'
// CHECK-NEXT: HLSLIntrinsicAttr {{.*}} Implicit "op" "" 408
// CHECK-NEXT: AvailabilityAttr {{.*}} Implicit  6.10 0 0 ""

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 2, 2)]] mat;

  uint elem1;
  __builtin_LinAlg_MatrixGetElement(elem1, mat, 3);

  float elem2;
  __builtin_LinAlg_MatrixGetElement(elem2, mat, 4);
}
