// Run: %dxc -T ps_6_0 -E main

// CHECK: deprecated tex2D intrinsic function will not be supported
sampler Sampler;

float4 main(float2 texCoord : TEXCOORD0) : SV_TARGET0
{
    return tex2D(Sampler, texCoord) * 1;
}
