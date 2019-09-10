// RUN: %dxc -E main -T ps_6_0 %s -Gec | FileCheck %s

// CHECK: COLOR                    0   xyzw        0     NONE   float
// CHECK: TEXCOORD                 0   xy          1     NONE   float
// CHECK: SV_Position              0   xyzw        2      POS   float
// CHECK: SV_Target                1   xyzw        1   TARGET   float   xyzw

struct VOut
{
 float4 color : COLOR0;
 float2 UV    : TEXCOORD0;
 float4 pos   : VPOS;
};

float4 main(VOut In) : CoLoR1
{
 return float4(0,0,0,0);
}
