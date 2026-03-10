// RUN: %dxc -I %hlsl_headers -T cs_6_9 -E main %s -verify

[numthreads(4,1,1)]
void main() {
  // expected-error@+1{{intrinsic __builtin_LinAlg_MatrixQueryAccumulatorLayout potentially used by ''main'' requires shader model 6.10 or greater}}
  uint layout = __builtin_LinAlg_MatrixQueryAccumulatorLayout();
}
