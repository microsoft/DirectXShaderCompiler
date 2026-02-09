// REQUIRES: dxil-1-10

// Pass: GSM fits within the GroupSharedLimit
// RUN: %dxc -E main -T as_6_10 -DGSM_EXTRA=0 -DUSE_GROUP_SHARED_LIMIT %s | FileCheck %s
// CHECK: @main

// Fail: GSM exceeds GroupSharedLimit (32772 > 32768)
// RUN: not %dxc -E main -T as_6_10 -DGSM_EXTRA=1 -DUSE_GROUP_SHARED_LIMIT %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERROR
// CHECK-ERROR: Total Thread Group Shared Memory storage is 32772, exceeded 32768.

// Fail: GSM exceeds default amplification shader limit (32772 > 32768)
// RUN: not %dxc -E main -T as_6_10 -DGSM_EXTRA=1 %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERROR2
// CHECK-ERROR2: Total Thread Group Shared Memory storage is 32772, exceeded 32768.

#define NUM_BYTES_OF_SHARED_MEM (32*1024)
#define NUM_DWORDS_SHARED_MEM (NUM_BYTES_OF_SHARED_MEM / 4)
#define NUM_THREADS 32

#ifndef GSM_EXTRA
#define GSM_EXTRA 0
#endif

groupshared uint g_testBuffer[NUM_DWORDS_SHARED_MEM + GSM_EXTRA];

struct Payload {
    float2 dummy;
    float4 pos;
    float color[2];
};

#ifdef USE_GROUP_SHARED_LIMIT
[GroupSharedLimit(NUM_BYTES_OF_SHARED_MEM)]
#endif
[numthreads(NUM_THREADS, 1, 1)]
void main(in uint tig : SV_GroupIndex)
{
    uint iterations = NUM_DWORDS_SHARED_MEM / NUM_THREADS;
    
    for (uint i = 0; i < iterations; i++)
    {
        uint index = tig + i * NUM_THREADS;
        g_testBuffer[index] = index;
    }
    
    // synchronize all threads in the group
    GroupMemoryBarrierWithGroupSync();

    Payload pld;
    pld.dummy = float2(1.0, 2.0);
    pld.pos = float4(g_testBuffer[0], g_testBuffer[1], g_testBuffer[2], g_testBuffer[3]);
    pld.color[0] = 7.0;
    pld.color[1] = 8.0;
    DispatchMesh(NUM_THREADS, 1, 1, pld);
}
