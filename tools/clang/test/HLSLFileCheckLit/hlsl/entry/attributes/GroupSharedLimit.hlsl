// REQUIRES: dxil-1-10

// Pass: GSM fits within the GroupSharedLimit
// RUN: %dxc -E main -T cs_6_10 -DGSM_EXTRA=0 -DUSE_GROUP_SHARED_LIMIT %s | FileCheck %s
// CHECK: @main

// Fail: GSM exceeds GroupSharedLimit (32772 > 32768)
// RUN: not %dxc -E main -T cs_6_10 -DGSM_EXTRA=1 -DUSE_GROUP_SHARED_LIMIT %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERROR
// CHECK-ERROR: Total Thread Group Shared Memory storage is 32772, exceeded 32768.

// Fail: GSM exceeds default compute shader limit (32772 > 32768)
// RUN: not %dxc -E main -T cs_6_10 -DGSM_EXTRA=1 %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERROR2
// CHECK-ERROR2: Total Thread Group Shared Memory storage is 32772, exceeded 32768.

#define NUM_BYTES_OF_SHARED_MEM (32*1024)
#define NUM_DWORDS_SHARED_MEM (NUM_BYTES_OF_SHARED_MEM / 4)
#define THREAD_GROUP_SIZE_X 1024

#ifndef GSM_EXTRA
#define GSM_EXTRA 0
#endif

groupshared uint g_testBuffer[NUM_DWORDS_SHARED_MEM + GSM_EXTRA];

RWStructuredBuffer <uint> g_output : register(u0);

#ifdef USE_GROUP_SHARED_LIMIT
[GroupSharedLimit(NUM_BYTES_OF_SHARED_MEM)]
#endif
[numthreads(THREAD_GROUP_SIZE_X, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint iterations = NUM_DWORDS_SHARED_MEM / THREAD_GROUP_SIZE_X;
    
    for (uint i = 0; i < iterations; i++)
    {
        uint index = DTid.x + i * THREAD_GROUP_SIZE_X;
        g_testBuffer[index] = index;
    }
    
    // synchronize all threads in the group
    GroupMemoryBarrierWithGroupSync();

    // write the shared data to the output buffer
    for (uint i = 0; i < iterations; i++)
    {
        uint index = DTid.x + i * THREAD_GROUP_SIZE_X;
        g_output[index] = g_testBuffer[index];
    }
}
