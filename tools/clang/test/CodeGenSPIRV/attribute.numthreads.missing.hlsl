// Run: %dxc -T cs_6_0 -E main

// CHECK: OpEntryPoint GLCompute %main "main"
// CHECK: OpExecutionMode %main LocalSize 1 1 1

void main() {}
