// RUN: %dxc -T ps_6_0 -Od -E main %s | FileCheck %s

// CHECK: %precise = alloca float, align 4
// CHECK: %globallycoherent = alloca float, align 4
// CHECK: %sample = alloca float, align 4

// CHECK: call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %MyBuffer_UAV_structbuf, i32 0, i32 0)
// CHECK: call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %MyBuffer_UAV_structbuf, i32 0, i32 16)
// CHECK: call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %MyBuffer_UAV_structbuf, i32 0, i32 32)
// CHECK: call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68, %dx.types.Handle %MyBuffer_UAV_structbuf, i32 0, i32 48)

// Make sure 'precise', 'globallycoherent' and 'sample' can be used as identifiers (FXC back-compat)
float3 foo(float3 sample) {
    return sample;
}

struct S {
  float4 center;
  float4 precise;
  float4 sample;
  float4 globallycoherent;
};

RWStructuredBuffer<S> MyBuffer;

float3 main(float4 input : SV_POSITION) : SV_TARGET
{
    float precise = 1.0f;
    float globallycoherent = 1.0f;
    float sample = 1.0f;

    return foo(float3(precise, globallycoherent, sample)) +
           MyBuffer[0].center + MyBuffer[0].precise +
           MyBuffer[0].sample + MyBuffer[0].globallycoherent;
}