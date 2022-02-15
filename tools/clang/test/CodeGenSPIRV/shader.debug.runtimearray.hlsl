// RUN: %dxc -T ps_6_0 -E main -fspv-debug=vulkan

struct PSInput
{
    float4 color : COLOR;
};

Texture2D bindless[];

sampler DummySampler;

// CHECK: {{%\d+}} = OpExtInst %void {{%\d+}} DebugTypeArray {{%\d+}} %uint_0

float4 main(PSInput input) : SV_TARGET
{
    return input.color * bindless[4].Sample(DummySampler, float2(1,1));
}

