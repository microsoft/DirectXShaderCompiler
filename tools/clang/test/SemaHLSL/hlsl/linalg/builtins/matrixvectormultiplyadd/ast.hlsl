// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -E main %s -ast-dump-implicit | FileCheck %s

// CHECK: FunctionDecl {{.*}} implicit used __builtin_LinAlg_MatrixVectorMultiplyAdd 'void (vector<float, 4> &, __builtin_LinAlgMatrix {{.*}}, vector<float, 4>, unsigned int, vector<float, 4>, unsigned int)' extern
// CHECK-NEXT: ParmVarDecl {{.*}} ret 'vector<float, 4> &&__restrict'
// CHECK-NEXT: ParmVarDecl {{.*}} mat '__builtin_LinAlgMatrix {{.*}}'
// CHECK-NEXT: ParmVarDecl {{.*}} input 'vector<float, 4>':'vector<float, 4>'
// CHECK-NEXT: ParmVarDecl {{.*}} input_interp 'unsigned int'
// CHECK-NEXT: ParmVarDecl {{.*}} bias 'vector<float, 4>':'vector<float, 4>'
// CHECK-NEXT: ParmVarDecl {{.*}} bias_interp 'unsigned int'
// CHECK-NEXT: HLSLIntrinsicAttr {{.*}} Implicit "op" "" 423
// CHECK-NEXT: AvailabilityAttr {{.*}} Implicit  6.10 0 0 ""

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 2, 2)]] mat;
  __builtin_LinAlg_FillMatrix(mat, 15);

  float4 input = {1,2,3,4};
  float4 bias = {5,6,7,8};
  float4 result;
  __builtin_LinAlg_MatrixVectorMultiplyAdd(result, mat, input, 1, bias, 2);
}
