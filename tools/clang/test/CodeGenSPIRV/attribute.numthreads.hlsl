// Run: %dxc -T cs_6_0 -E main

// CHECK: OpEntryPoint GLCompute %main "main"
// CHECK: OpExecutionMode %main LocalSize 8 4 1

[numthreads(8, 4, 1)]
void main() {}
