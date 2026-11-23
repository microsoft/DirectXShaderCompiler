// RUN: not %dxc -T vs_6_0 -E main -fcgl  %s -spirv  2>&1 | FileCheck %s
// Note: this test uses FileCheck instead of -verify because the error
// diagnostic comes from the code generation layer instead of sema.
[[vk::binding(10, 2)]] float4 gVec = 1.0;

float4 main() : A { return gVec; }

// CHECK: :4:38: warning: initializer for a variable in a cbuffer will be ignored
// CHECK: :4:3: error: variable 'gVec' will be placed in $Globals so cannot have vk::binding attribute
