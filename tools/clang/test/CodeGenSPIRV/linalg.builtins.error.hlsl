// RUN: not %dxc -T cs_6_10 -spirv %s 2>&1 | FileCheck %s

// CHECK: error: __builtin_LinAlg_MatrixQueryAccumulatorLayout intrinsic function unimplemented

[numthreads(1, 1, 1)]
void main() {
  uint layout = dx::__builtin_LinAlg_MatrixQueryAccumulatorLayout();
}
