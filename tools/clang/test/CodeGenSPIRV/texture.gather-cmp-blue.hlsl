// Run: %dxc -T ps_6_0 -E main

SamplerComparisonState gSampler : register(s5);

Texture2D<float4> gTexture : register(t1);

float4 main(float2 location: A, float comparator: B) : SV_Target {
    return gTexture.GatherCmpBlue(gSampler, location, comparator, int2(1, 2));
}

// CHECK: :8:21: error: no equivalent for GatherCmpBlue intrinsic method in Vulkan
