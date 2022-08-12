// RUN: %dxc -E PSMain -T ps_6_0 %s | FileCheck %s

// CHECK: define void @PSMain()

// fix me: float4 BrokenTestArray0[int(.3)*12]; 
float4 BrokenTestArray1[min(2, 3)];
float4 WorkingTestArray0[max(2, 3)];

float4 PSMain(float4 color : COLOR) : SV_TARGET
{
    return BrokenTestArray1[int(color.y)] +
           WorkingTestArray0[int(color.z)];
}