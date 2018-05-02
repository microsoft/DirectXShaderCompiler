// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// CHECK: error: shader must include inout payload structure parameter
// CHECK: error: shader must include attributes structure parameter

[shader("closesthit")]
void closesthit_no_payload() {}
