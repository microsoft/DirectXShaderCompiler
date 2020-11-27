// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s -check-prefix=CHK_RANGE
// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s -check-prefix=CHK_VAROFF

// CHK_RANGE:  error: offset texture instructions must take offset which can resolve to integer literal in the range -8 to 7.
// CHK_RANGE:  error: offset texture instructions must take offset which can resolve to integer literal in the range -8 to 7.
// CHK_RANGE:  error: offset texture instructions must take offset which can resolve to integer literal in the range -8 to 7.

// CHK_VAROFF: Offsets to texture access operations must be immediate values
// CHK_VAROFF: Offsets to texture access operations must be immediate values
// CHK_VAROFF: Offsets to texture access operations must be immediate values
// CHK_VAROFF: Offsets to texture access operations must be immediate values
// CHK_VAROFF: Offsets to texture access operations must be immediate values
// CHK_VAROFF: Offsets to texture access operations must be immediate values


Texture2D t;
SamplerState s;

float4 Range(float2 uv : UV, uint2 offset : O) : SV_TARGET
{
    float4 a = t.GatherRed(s, uv, int2(9,-8));
    float4 b = t.Sample(s, uv, int2(-8,9));
    float4 c = t.Load(0, int2(-8, 9));
    return a + b + c;
}

float4 VarOffset(float2 uv : UV, uint2 offset : O) : SV_TARGET
{
    float4 a = t.GatherRed(s, uv, offset);
    float4 b = t.Sample(s, uv, offset);
    float4 c = t.Load(0, offset);
    return a + b + c;
}
