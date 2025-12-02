// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s

// CHECK: call i32 @dx.op.getGroupWaveCount(i32 313
// CHECK: call i32 @dx.op.getGroupWaveIndex(i32 312

RWStructuredBuffer<uint> output0 : register(u0);
RWStructuredBuffer<uint> output1 : register(u1);

[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint waveIdx = GetGroupWaveIndex();
    uint waveCount = GetGroupWaveCount();

    output0[0] = waveIdx;
    output1[0] = waveCount;
}