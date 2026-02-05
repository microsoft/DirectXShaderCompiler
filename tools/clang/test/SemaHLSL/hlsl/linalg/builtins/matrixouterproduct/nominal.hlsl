// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s -verify

// expected-no-diagnostics

[numthreads(1,1,1)]
void main() {
  float4 lhs = {1,2,3,4};
  float4 rhs = {4,3,2,1};

  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat;
  __builtin_LinAlg_MatrixOuterProduct(mat, lhs, rhs);
}
