// RUN: %dxc -T cs_6_9 -HV 202x -E main %s -verify

groupshared float SharedArr[64];

void fn(groupshared float Arr[64], float F) {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] mat;

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixLoadFromMemory potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixLoadFromMemory(mat, Arr, 0, 0, 0);
}

[numthreads(4,1,1)]
void main() {
  fn(SharedArr, 6.0);
}
