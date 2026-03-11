// RUN: %dxc -I %hlsl_headers -T cs_6_9 -E main %s -verify

[numthreads(4,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 0, 0)]] mat;
  float4 input = {1,2,3,4};
  float4 result;

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixVectorMultiply potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixVectorMultiply(result, mat, input, 1);
}
