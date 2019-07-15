// RUN: %dxc -E main -T ps_6_5 %s | FileCheck %s

// Test FeedbackTexture2D*** and their WriteSamplerFeedback methods

FeedbackTexture2D<SAMPLER_FEEDBACK_MIN_MIP> feedbackMinMip;
FeedbackTexture2D<SAMPLER_FEEDBACK_MIP_REGION_USED> feedbackMipRegionUsed;
FeedbackTexture2DArray<SAMPLER_FEEDBACK_MIN_MIP> feedbackMinMipArray;
FeedbackTexture2DArray<SAMPLER_FEEDBACK_MIP_REGION_USED> feebackMipRegionUsedArray;
Texture2D<float> texture2D;
Texture2D<float4> texture2D_float4;
Texture2DArray<float> texture2DArray;
SamplerState samp;

float main() : SV_Target
{
    float2 coords2D = float2(1, 2);
    float3 coords2DArray = float3(1, 2, 3);
    float clamp = 4;
    float bias = 5;
    float lod = 6;
    float ddx = 7;
    float ddy = 8;
    
    // Test every dxil intrinsic
    // CHECK: call void @dx.op.writeSamplerFeedback(
    // CHECK: float 1.000000e+00, float 2.000000e+00, float undef, float 4.000000e+00)
    feedbackMinMip.WriteSamplerFeedback(texture2D, samp, coords2D, clamp);
    // CHECK: call void @dx.op.writeSamplerFeedbackBias(
    // CHECK: float 1.000000e+00, float 2.000000e+00, float undef, float 5.000000e+00, float 4.000000e+00)
    feedbackMinMip.WriteSamplerFeedbackBias(texture2D, samp, coords2D, bias, clamp);
    // CHECK: call void @dx.op.writeSamplerFeedbackLevel(
    // CHECK: float 1.000000e+00, float 2.000000e+00, float undef, float 6.000000e+00)
    feedbackMinMip.WriteSamplerFeedbackLevel(texture2D, samp, coords2D, lod);
    // CHECK: call void @dx.op.writeSamplerFeedbackGrad(
    // CHECK: float 1.000000e+00, float 2.000000e+00, float undef, float 7.000000e+00, float 8.000000e+00, float 4.000000e+00)
    feedbackMinMip.WriteSamplerFeedbackGrad(texture2D, samp, coords2D, ddx, ddy, clamp);
    
    // Test with undef clamp
    // CHECK: call void @dx.op.writeSamplerFeedback(
    // CHECK: float 1.000000e+00, float 2.000000e+00, float undef, float undef)
    feedbackMinMip.WriteSamplerFeedback(texture2D, samp, coords2D);
    // CHECK: call void @dx.op.writeSamplerFeedbackBias(
    // CHECK: float 1.000000e+00, float 2.000000e+00, float undef, float 5.000000e+00, float undef)
    feedbackMinMip.WriteSamplerFeedbackBias(texture2D, samp, coords2D, bias);
    // CHECK: call void @dx.op.writeSamplerFeedbackGrad(
    // CHECK: float 1.000000e+00, float 2.000000e+00, float undef, float 7.000000e+00, float 8.000000e+00, float undef)
    feedbackMinMip.WriteSamplerFeedbackGrad(texture2D, samp, coords2D, ddx, ddy);

    // Test on every FeedbackTexture variant
    // CHECK: call void @dx.op.writeSamplerFeedback(
    // CHECK: float 1.000000e+00, float 2.000000e+00, float undef, float undef)
    feedbackMipRegionUsed.WriteSamplerFeedback(texture2D, samp, coords2D);
    // CHECK: call void @dx.op.writeSamplerFeedback(
    // CHECK: float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float undef)
    feedbackMinMipArray.WriteSamplerFeedback(texture2DArray, samp, coords2DArray);
    // CHECK: call void @dx.op.writeSamplerFeedback(
    // CHECK: float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float undef)
    feebackMipRegionUsedArray.WriteSamplerFeedback(texture2DArray, samp, coords2DArray);

    // Test with overloaded texture type
    // CHECK: call void @dx.op.writeSamplerFeedback(
    // CHECK: float 1.000000e+00, float 2.000000e+00, float undef, float undef)
    feedbackMinMip.WriteSamplerFeedback(texture2D_float4, samp, coords2D);

    return 0;
}