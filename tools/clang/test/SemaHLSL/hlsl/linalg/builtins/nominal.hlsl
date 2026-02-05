// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s -verify

// expected-no-diagnostics

ByteAddressBuffer inbuf;
RWByteAddressBuffer outbuf;

[numthreads(1,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat1;
  __builtin_LinAlg_FillMatrix(mat1, 5);
  __builtin_LinAlg_MatrixStoreToDescriptor(mat1, outbuf, 3, 2, 1);

  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 1, 2, 1, 1)]] mat2;
  __builtin_LinAlg_MatrixLoadFromDescriptor(mat2, inbuf, 1, 2, 3);

  float4 input = {1,2,3,4};
  float4 bias = {4,3,2,1};
  float4 result;

  __builtin_LinAlg_MatrixVectorMultiplyAdd(result, mat1, input, 1, bias, 0);
  __builtin_LinAlg_MatrixVectorMultiply(result, mat2, result, 1);

  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat3;
  __builtin_LinAlg_MatrixOuterProduct(mat3, input, bias);
}
