// REQUIRES: dxil-1-10
// RUN: %dxc -E MainPass -T cs_6_10 %s | FileCheck %s

#define NUM_BYTES_OF_SHARED_MEM (32*1024)
#define NUM_DWORDS_SHARED_MEM (NUM_BYTES_OF_SHARED_MEM / 4)
#define THREAD_GROUP_SIZE_X 1024

groupshared uint g_testBufferPASS[NUM_DWORDS_SHARED_MEM];

RWStructuredBuffer <uint> g_output : register(u0);

// CHECK: @MainPass

[GroupSharedLimit(NUM_BYTES_OF_SHARED_MEM)]
[numthreads(THREAD_GROUP_SIZE_X, 1, 1)]
void MainPass( uint3 DTid : SV_DispatchThreadID )
{
    uint iterations = NUM_DWORDS_SHARED_MEM / THREAD_GROUP_SIZE_X;
    
    for (uint i = 0; i < iterations; i++)
    {
        uint index = DTid.x + i * THREAD_GROUP_SIZE_X;
        g_testBufferPASS[index] = index;
    }
    
    // synchronize all threads in the group
    GroupMemoryBarrierWithGroupSync();

    // write the shared data to the output buffer
    for (uint i = 0; i < iterations; i++)
    {
        uint index = DTid.x + i * THREAD_GROUP_SIZE_X;
        g_output[index] = g_testBufferPASS[index];
    }
}

// RUN: not %dxc -E MainFail -T cs_6_10 %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERROR
// CHECK-ERROR: Total Thread Group Shared Memory storage is 32772, exceeded 32768.

groupshared uint g_testBufferFAIL[NUM_DWORDS_SHARED_MEM + 1];

[GroupSharedLimit(NUM_BYTES_OF_SHARED_MEM)]
[numthreads(THREAD_GROUP_SIZE_X, 1, 1)]
void MainFail( uint3 DTid : SV_DispatchThreadID )
{
    uint iterations = NUM_DWORDS_SHARED_MEM / THREAD_GROUP_SIZE_X;
    
    for (uint i = 0; i < iterations; i++)
    {
        uint index = DTid.x + i * THREAD_GROUP_SIZE_X;
        g_testBufferFAIL[index] = index;
    }
    
    // synchronize all threads in the group
    GroupMemoryBarrierWithGroupSync();

    // write the shared data to the output buffer
    for (uint i = 0; i < iterations; i++)
    {
        uint index = DTid.x + i * THREAD_GROUP_SIZE_X;
        g_output[index] = g_testBufferFAIL[index];
    }
}  

// RUN: not %dxc -E MainFail2 -T cs_6_10 %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERROR
[numthreads(THREAD_GROUP_SIZE_X, 1, 1)]
void MainFail2( uint3 DTid : SV_DispatchThreadID )
{
    uint iterations = NUM_DWORDS_SHARED_MEM / THREAD_GROUP_SIZE_X;
    
    for (uint i = 0; i < iterations; i++)
    {
        uint index = DTid.x + i * THREAD_GROUP_SIZE_X;
        g_testBufferFAIL[index] = index;
    }
    
    // synchronize all threads in the group
    GroupMemoryBarrierWithGroupSync();

    // write the shared data to the output buffer
    for (uint i = 0; i < iterations; i++)
    {
        uint index = DTid.x + i * THREAD_GROUP_SIZE_X;
        g_output[index] = g_testBufferFAIL[index];
    }
}
