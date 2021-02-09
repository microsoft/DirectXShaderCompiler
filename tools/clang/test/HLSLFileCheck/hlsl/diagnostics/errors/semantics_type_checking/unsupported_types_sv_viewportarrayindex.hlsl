// RUN: %dxc -E main -T ds_6_2 -DTY=uint %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=float %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=float2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=float3 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=float4 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=int2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=int2x2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=bool2 %s | FileCheck %s -check-prefix=CHK_ERR

// CHK_NO_ERR: define void @main
// CHK_ERR: error: invalid type used for 'SV_ViewportArrayIndex' semantic


struct DSFoo
{
    float Edges[4]  : SV_TessFactor;
    float Inside[2] : SV_InsideTessFactor;
    float4 a : AAA;
    float3 b[3] : BBB;
};

struct HSFoo
{
    float3 pos : AAA_HSFoo;
};

uint g_Idx1;

[domain("quad")]
float4 main(OutputPatch<HSFoo, 16> p, DSFoo input, TY UV : SV_ViewportArrayIndex) : SV_Position
{
    return 0;
}