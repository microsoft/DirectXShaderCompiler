// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main %s -verify

// expected-no-diagnostics

RWStructuredBuffer<int> input;
RWStructuredBuffer<int> output;

[numthreads(1,1,1)]
void main() {
  __builtin_LinAlg_MatrixRef mat = __builtin_LinAlg_CreateMatrix();
  output[0] = __builtin_LinAlg_MatrixLength(mat);
}
