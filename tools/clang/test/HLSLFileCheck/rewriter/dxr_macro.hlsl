// RUN: %dxr -Dfloat4=0  %s 2>&1 | FileCheck %s
// RUN: %dxr -line-directive -Dfloat4=0  %s 2>&1 | FileCheck %s
// RUN: %dxr -decl-global-cb -Dfloat4=0  %s 2>&1 | FileCheck %s

// Make sure define float4 as 0 cause error for dxr.

// CHECK:error: expected member name or ';' after declaration specifiers
// CHECK-NEXT:    float4 position : SV_Position;
// CHECK-NEXT:    ^
// CHECK-NEXT: note: expanded from here
// CHECK-NEXT:#define float4 0
// CHECK-NEXT:               ^

struct PSInput
{
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

float4 main(PSInput input) : SV_Target0
{
    return input.color * input.color;
}
