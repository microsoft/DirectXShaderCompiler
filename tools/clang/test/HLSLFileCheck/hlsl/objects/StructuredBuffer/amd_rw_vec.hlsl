struct PS_INPUT
{
 float4 pos : SV_POSITION;
 float2 vPos : TEXCOORD0;
 float2 sPos : TEXCOORD1;
};

RWStructuredBuffer<float4> t_output : register(u0);

float4 psMain(in PS_INPUT I)
{
 return float4(0, 0, 0, 1);
}


[numthreads(8, 8, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
 PS_INPUT I = {
 float4(0,0, 0, 1),
 float2(dtid),
 float2(dtid)
 };

 uint2 uspos = uint2(I.pos.xy);

 float3 prev = t_output[uspos.x].rgb;
 t_output[uspos.x].rgb = float3(psMain(I).rgb + prev);
}
