// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s -verify

// expected-no-diagnostics

[numthreads(1,1,1)]
void main() {
  float4 input = {1,2,3,4};
  float4 result;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 1, 2, 1, 1)]] mat;
  __builtin_LinAlg_MatrixVectorMultiply(result, mat, input, 1);
}
