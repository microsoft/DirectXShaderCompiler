// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -E main %s -ast-dump-implicit | FileCheck %s

// CHECK: FunctionDecl {{.*}} implicit used __builtin_LinAlg_MatrixOuterProduct 'void (__builtin_LinAlgMatrix {{.*}}, vector<int, 4>, vector<int, 4>)' extern
// CHECK-NEXT: ParmVarDecl {{.*}} ret '__builtin_LinAlgMatrix {{.*}}'
// CHECK-NEXT: ParmVarDecl {{.*}} vecA 'vector<int, 4>':'vector<int, 4>'
// CHECK-NEXT: ParmVarDecl {{.*}} vecB 'vector<int, 4>':'vector<int, 4>'
// CHECK-NEXT: HLSLIntrinsicAttr {{.*}} Implicit "op" "" 421
// CHECK-NEXT: AvailabilityAttr {{.*}} Implicit  6.10 0 0 ""


[shader("compute")]
[numthreads(1,1,1)]
void main() {
  int4 vecA = {1,2,3,4};
  int4 vecB = {1,2,3,4};
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 2, 2)]] mat;
  __builtin_LinAlg_MatrixOuterProduct(mat, vecA, vecB);
}
