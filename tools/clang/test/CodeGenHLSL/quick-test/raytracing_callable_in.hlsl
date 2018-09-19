// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// CHECK: error: callable parameter must be declared inout
// CHECK: error: callable parameter must be a user defined type with only numeric contents.

[shader("callable")]
void callable_in( in float4x4 param ) {}
