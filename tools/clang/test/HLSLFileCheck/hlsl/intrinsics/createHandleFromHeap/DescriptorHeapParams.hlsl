// RUN: %dxc -T ps_6_6 %s | %FileCheck %s

// Tests using descriptor heap elements directly as parameters to intrinsics
// Previously, the overload matching rejected this before it had a chance
// to add the implicit cast.

FeedbackTexture2D<SAMPLER_FEEDBACK_MIN_MIP> feedbackMinMip;

Texture2D<float> texture2D;
SamplerState samp;

float4 main(uint texIdx : TI, uint sampIdx : SI, float2 coord : C) : SV_Target {
    Texture2D<float4> tex = ResourceDescriptorHeap[texIdx];
    feedbackMinMip.WriteSamplerFeedback(ResourceDescriptorHeap[texIdx], samp, coord, 4);
    return tex.Sample(SamplerDescriptorHeap[sampIdx], coord) * tex.SampleCmp(SamplerDescriptorHeap[sampIdx + 1], coord, 1.0);
}