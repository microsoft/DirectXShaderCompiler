// RUN: %dxc -T cs_6_9 -E main %s -verify

RWByteAddressBuffer buff;

[numthreads(4,1,1)]
void main() {
  // expected-error@+2{{intrinsic __builtin_LinAlg_CreateMatrix potentially used by ''main'' requires shader model 6.10 or greater}}
  // expected-error@+1{{potential misuse of built-in type '__builtin_LinAlg_MatrixRef' in shader model cs_6_9; introduced in shader model 6.10}}
  __builtin_LinAlg_MatrixRef mat = __builtin_LinAlg_CreateMatrix();

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixLoadFromDescriptor potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixLoadFromDescriptor(mat, buff, 1,1,0);

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixLength potentially used by ''main'' requires shader model 6.10 or greater}}
  uint length = __builtin_LinAlg_MatrixLength(mat);

  // expected-error@+1{{intrinsic __builtin_LinAlg_FillMatrix potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_FillMatrix(mat, length);

  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixStoreToDescriptor potentially used by ''main'' requires shader model 6.10 or greater}}
  __builtin_LinAlg_MatrixStoreToDescriptor(mat, buff, 1,1,0);

}
