// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: error: return type for ray tracing shaders must be void

// Fine.
[shader("miss")]
void miss_nop() {}

[shader("miss")]
float miss_ret() { return 1.0; }
