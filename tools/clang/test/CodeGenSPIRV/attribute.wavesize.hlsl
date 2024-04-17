// RUN: %dxc -T cs_6_6 -E main -fcgl -spirv %s 2>&1 | FileCheck %s

// CHECK: warning: Wave size is not supported by SPIR-V

[WaveSize(32)]
[numthreads(1, 1, 1)]
void main() {}