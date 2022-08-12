// RUN: %dxc -T ps_6_0 -E PSMain %s | FileCheck %s

// CHECK: ; Input signature:

float4 BrokenTestArray0[int(.3)*12];
float4 BrokenTestArray1[min(2, 3)];
float4 WorkingTestArray0[max(2, 3)];

float4 PSMain(float4 color : COLOR) : SV_TARGET
{
    return BrokenTestArray0[int(color.x)] +
           BrokenTestArray1[int(color.y)] +
           WorkingTestArray0[int(color.z)];
}