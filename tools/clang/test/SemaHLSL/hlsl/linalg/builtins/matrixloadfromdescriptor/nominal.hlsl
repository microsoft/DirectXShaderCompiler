// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s -verify

// expected-no-diagnostics

ByteAddressBuffer inbuf;

[numthreads(1,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 1, 2, 1, 1)]] mat;
  __builtin_LinAlg_MatrixLoadFromDescriptor(mat, inbuf, 1, 2, 3);
}
