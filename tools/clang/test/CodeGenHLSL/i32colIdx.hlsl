// RUN: %dxc -E main -T ps_6_0 %s

struct Input
{
    float2 v : TEXCOORD0;
};

float4 main(Input input) : SV_Target
{
    return input.v[0];
}