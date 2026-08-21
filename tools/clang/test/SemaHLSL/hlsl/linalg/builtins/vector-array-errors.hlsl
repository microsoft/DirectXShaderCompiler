// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -HV 202x -E main -verify %s

// 'LinAlg<>[]' parameters accept arrays of scalars and arrays of vectors of
// scalars, but nothing else.

struct S {
  float F;
};

groupshared float2x2 MatArr[64];
groupshared S StructArr[64];

[numthreads(1, 1, 1)] void main() {
  __builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(4, 5, 4, 1, 2)]] Mat;

  // expected-error@+2{{no matching function for call to '__builtin_LinAlg_MatrixLoadFromMemory'}}
  // expected-note@+1{{candidate function not viable: no known conversion from 'float2x2 __attribute__((address_space(3)))[64]' to 'float const __attribute__((address_space(3))) (&)[64]' for 2nd argument}}
  __builtin_LinAlg_MatrixLoadFromMemory(Mat, MatArr, 0, 0, 0);

  // expected-error@+2{{no matching function for call to '__builtin_LinAlg_MatrixStoreToMemory'}}
  // expected-note@+1{{candidate function not viable: no known conversion from 'S __attribute__((address_space(3)))[64]' to 'literal float const __attribute__((address_space(3))) (&)[64]' for 2nd argument}}
  __builtin_LinAlg_MatrixStoreToMemory(Mat, StructArr, 0, 0, 0);

  // expected-error@+2{{no matching function for call to '__builtin_LinAlg_MatrixAccumulateToMemory'}}
  // expected-note@+1{{candidate function not viable: no known conversion from 'float2x2 __attribute__((address_space(3)))[64]' to 'float const __attribute__((address_space(3))) (&)[64]' for 2nd argument}}
  __builtin_LinAlg_MatrixAccumulateToMemory(Mat, MatArr, 0, 0, 0, 0);
}
