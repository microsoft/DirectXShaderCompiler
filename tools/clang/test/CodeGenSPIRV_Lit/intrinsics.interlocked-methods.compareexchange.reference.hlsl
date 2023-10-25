// RUN: not %dxc -T cs_6_6 -E main -fcgl -spirv %s 2>&1 | FileCheck %s

groupshared uint value;

[numthreads(1, 1, 1)]
void main() {
// CHECK: error: InterlockedCompareExchange requires a reference as output parameter
  InterlockedCompareExchange(value, 1, 2, 3);
}
