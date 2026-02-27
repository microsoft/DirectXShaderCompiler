// RUN: %dxc -I %hlsl_headers -T cs_6_9 -E main %s -verify

[numthreads(4,1,1)]
void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(1, 5, 4, 0, 0)]] mat;

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixGetCoordinate potentially used by ''main'' requires shader model 6.10 or greater}}
  uint2 coord = __builtin_LinAlg_MatrixGetCoordinate(mat, 0);
}
