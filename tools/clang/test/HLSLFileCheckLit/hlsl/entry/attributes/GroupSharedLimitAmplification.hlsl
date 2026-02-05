// REQUIRES: dxil-1-10
// RUN: %dxc -E MainPass -T as_6_10 %s | FileCheck %s

#define NUM_BYTES_OF_SHARED_MEM (32*1024)
#define NUM_DWORDS_SHARED_MEM (NUM_BYTES_OF_SHARED_MEM / 4)
#define NUM_THREADS 32

groupshared uint g_testBufferPASS[NUM_DWORDS_SHARED_MEM];

struct Payload {
    float2 dummy;
    float4 pos;
    float color[2];
};

// CHECK: @MainPass

[GroupSharedLimit(NUM_BYTES_OF_SHARED_MEM)]
[numthreads(NUM_THREADS, 1, 1)]
void MainPass(in uint tig : SV_GroupIndex)
{
    uint iterations = NUM_DWORDS_SHARED_MEM / NUM_THREADS;
    
    for (uint i = 0; i < iterations; i++)
    {
        uint index = tig + i * NUM_THREADS;
        g_testBufferPASS[index] = index;
    }
    
    // synchronize all threads in the group
    GroupMemoryBarrierWithGroupSync();

    Payload pld;
    pld.dummy = float2(1.0, 2.0);
    pld.pos = float4(g_testBufferPASS[0], g_testBufferPASS[1], g_testBufferPASS[2], g_testBufferPASS[3]);
    pld.color[0] = 7.0;
    pld.color[1] = 8.0;
    DispatchMesh(NUM_THREADS, 1, 1, pld);
}

// RUN: not %dxc -E MainFail -T as_6_10 %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERROR
// CHECK-ERROR: Total Thread Group Shared Memory storage is 32772, exceeded 32768.

groupshared uint g_testBufferFAIL[NUM_DWORDS_SHARED_MEM + 1];

[GroupSharedLimit(NUM_BYTES_OF_SHARED_MEM)]
[numthreads(NUM_THREADS, 1, 1)]
void MainFail(in uint tig : SV_GroupIndex)
{
    uint iterations = NUM_DWORDS_SHARED_MEM / NUM_THREADS;
    
    for (uint i = 0; i < iterations; i++)
    {
        uint index = tig + i * NUM_THREADS;
        g_testBufferFAIL[index] = index;
    }
    
    // synchronize all threads in the group
    GroupMemoryBarrierWithGroupSync();

    Payload pld;
    pld.dummy = float2(1.0, 2.0);
    pld.pos = float4(g_testBufferFAIL[0], g_testBufferFAIL[1], g_testBufferFAIL[2], g_testBufferFAIL[3]);
    pld.color[0] = 7.0;
    pld.color[1] = 8.0;
    DispatchMesh(NUM_THREADS, 1, 1, pld);
}

// RUN: not %dxc -E MainFail2 -T as_6_10 %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERROR
[numthreads(NUM_THREADS, 1, 1)]
void MainFail2(in uint tig : SV_GroupIndex)
{
    uint iterations = NUM_DWORDS_SHARED_MEM / NUM_THREADS;
    
    for (uint i = 0; i < iterations; i++)
    {
        uint index = tig + i * NUM_THREADS;
        g_testBufferFAIL[index] = index;
    }
    
    // synchronize all threads in the group
    GroupMemoryBarrierWithGroupSync();

    Payload pld;
    pld.dummy = float2(1.0, 2.0);
    pld.pos = float4(g_testBufferFAIL[0], g_testBufferFAIL[1], g_testBufferFAIL[2], g_testBufferFAIL[3]);
    pld.color[0] = 7.0;
    pld.color[1] = 8.0;
    DispatchMesh(NUM_THREADS, 1, 1, pld);
}
