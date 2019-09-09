// RUN: %dxc -E main -T ps_6_0 %s -Gec | FileCheck %s

// CHECK: SV_Position              0   xyzw        0      POS   float
// CHECK: SV_Target                1   xyzw        1   TARGET   float   xyzw
// CHECK: SV_Depth                 0    N/A   oDepth    DEPTH   float    YES

struct PSIn
{
 float4 Position;
};

struct PSOut
{
  float4 c : COLOR1;
  float d  : DEPTH;
};

PSOut main(PSIn In : VPOS) 
{
    PSOut retValue = { {1, 0, 1, 0}, 0.5 };
    return retValue;
}
