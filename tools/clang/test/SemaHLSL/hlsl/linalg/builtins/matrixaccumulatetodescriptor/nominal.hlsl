// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s -verify

// expected-no-diagnostics

RWByteAddressBuffer outbuf;

[numthreads(1,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat;
  __builtin_LinAlg_MatrixAccumulateToDescriptor(mat, outbuf, 2, 2, 2);
}
