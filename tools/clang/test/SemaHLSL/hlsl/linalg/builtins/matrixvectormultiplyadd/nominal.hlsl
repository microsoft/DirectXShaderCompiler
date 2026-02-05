// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s -verify

// expected-no-diagnostics

[numthreads(1,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat;
  float4 input = {1,2,3,4};
  float4 bias = {4, 3, 2, 1};
  float4 result;

  __builtin_LinAlg_MatrixVectorMultiplyAdd(result, mat, input, 1, bias, 0);
}
