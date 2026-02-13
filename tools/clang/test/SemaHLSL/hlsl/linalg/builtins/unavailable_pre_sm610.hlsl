// RUN: %dxc -I %hlsl_headers -T cs_6_9 -E main %s -verify

RWByteAddressBuffer inbuf;
RWByteAddressBuffer outbuf;

[numthreads(4,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 0, 0)]] mat;

  // expected-error@+1{{intrinsic __builtin_LinAlg_FillMatrix potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_FillMatrix(mat, 1);

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixStoreToDescriptor potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixStoreToDescriptor(mat, outbuf, 1, 1, 1);

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixLoadFromDescriptor potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixLoadFromDescriptor(mat, inbuf, 1, 1, 1);

  float4 input = {1,2,3,4};
  float4 bias = {4,3,2,1};
  float4 result;

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixVectorMultiply potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixVectorMultiply(result, mat, input, 1);
  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixVectorMultiplyAdd potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixVectorMultiplyAdd(result, mat, input, 1, bias, 0);

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixOuterProduct potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixOuterProduct(mat, input, bias);

  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 1, 1, 1, 1)]] mat2;
  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixAccumulate potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixAccumulate(mat2, mat, mat);

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixQueryAccumulatorLayout potentially used by ''main'' requires shader model 6.10 or greater}}
  uint layout = __builtin_LinAlg_MatrixQueryAccumulatorLayout();

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixAccumulateToDescriptor potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixAccumulateToDescriptor(mat, outbuf, 9, 8, 7);

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixLength potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixLength(mat);

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixGetCoordinate potentially used by ''main'' requires shader model 6.10 or greater}}
  uint2 coord = __builtin_LinAlg_MatrixGetCoordinate(mat, 0);

  uint elem;
  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixGetElement potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixGetElement(elem, mat, 0);

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixSetElement potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixSetElement(mat, mat, 1, 1);

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixMatrixMultiply potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixMatrixMultiply(mat2, mat, mat);

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixMatrixMultiplyAccumulate potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixMatrixMultiplyAccumulate(mat, mat2, mat2, mat);
}
