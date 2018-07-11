// RUN: %dxc -T ps_6_0 -Od -E main %s | FileCheck %s 

// CHECK: %precise = alloca float, align 4
// CHECK: %globallycoherent = alloca float, align 4
// CHECK: %sample = alloca float, align 4

// Make sure 'precise', 'globallycoherent' and 'sample' can be used as identifiers (FXC back-compat)
float3 foo(float3 sample) {
    return sample;
}

float3 main(float4 input : SV_POSITION) : SV_TARGET
{
    float precise = 1.0f;
    float globallycoherent = 1.0f;
    float sample = 1.0f;
    
    return foo(float3(precise, globallycoherent, sample));
}