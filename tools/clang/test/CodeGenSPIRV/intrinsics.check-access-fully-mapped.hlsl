// RUN: %dxc -T ps_6_0 -E main

SamplerState      gSampler  : register(s5);
Texture2D<float4> t         : register(t1);

// CHECK: OpCapability SparseResidency

// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %v4float

float4 main(int2 offset: A) : SV_Target {
    uint status;
    float clamp;
    float4 val = t.Sample(gSampler, float2(0.1, 0.2), 1, clamp, status);
    
// CHECK: [[residency_code:%\d+]] = OpLoad %uint %status
// CHECK:        [[success:%\d+]] = OpImageSparseTexelsResident %bool [[residency_code]]
// CHECK:                           OpStore %success [[success]]
    bool success = CheckAccessFullyMapped(status);

    return 1.0;
}
