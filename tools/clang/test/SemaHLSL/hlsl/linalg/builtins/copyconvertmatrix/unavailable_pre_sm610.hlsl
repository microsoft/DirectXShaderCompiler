// RUN: %dxc -I %hlsl_headers -T cs_6_9 -E main %s -verify

[numthreads(4,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 0, 0)]] mat;
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 1, 1, 1, 1)]] mat2;

  // expected-error@+1{{intrinsic __builtin_LinAlg_CopyConvertMatrix potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_CopyConvertMatrix(mat, mat2, true);
}
