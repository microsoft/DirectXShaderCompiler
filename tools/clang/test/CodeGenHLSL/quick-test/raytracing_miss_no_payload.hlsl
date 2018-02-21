// RUN: %dxc -T lib_6_3 %s | FileCheck %s

// CHECK: error: shader must include inout payload structure parameter

[shader("miss")]
void miss_no_payload() {}
