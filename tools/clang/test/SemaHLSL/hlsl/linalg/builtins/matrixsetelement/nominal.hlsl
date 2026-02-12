// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s -verify

// expected-no-diagnostics

[numthreads(1,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat1;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 2, 2)]] mat2;
  __builtin_LinAlg_MatrixSetElement(mat2, mat1, 1, 1);
}
