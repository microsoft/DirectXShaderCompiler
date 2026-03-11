// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -E main %s -ast-dump-implicit | FileCheck %s

// CHECK: FunctionDecl {{.*}} implicit used __builtin_LinAlg_MatrixQueryAccumulatorLayout 'unsigned int ()' extern
// CHECK-NEXT: HLSLIntrinsicAttr {{.*}} Implicit "op" "" 418
// CHECK-NEXT: AvailabilityAttr {{.*}} Implicit  6.10 0 0 ""

[shader("compute")]
[numthreads(1,1,1)]
void main() {
  uint layout = __builtin_LinAlg_MatrixQueryAccumulatorLayout();
}
