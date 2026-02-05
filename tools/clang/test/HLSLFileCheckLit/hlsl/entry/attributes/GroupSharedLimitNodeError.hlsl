// REQUIRES: dxil-1-10
// RUN: not %dxc -T lib_6_10 %s 2>&1 | FileCheck %s

// Node shader error test for GroupSharedLimit attribute
// This test verifies that exceeding the GroupSharedLimit produces an error.

// CHECK: Total Thread Group Shared Memory storage is 32772, exceeded 32768.

#define NUM_BYTES_OF_SHARED_MEM (32*1024)
#define NUM_DWORDS_SHARED_MEM (NUM_BYTES_OF_SHARED_MEM / 4)
#define NUM_THREADS 1024

// Buffer exceeds the limit by 4 bytes (1 uint)
groupshared uint g_testBuffer[NUM_DWORDS_SHARED_MEM + 1];

RWStructuredBuffer<uint> g_output : register(u0);

struct MY_INPUT_RECORD {
    uint data;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(2, 1, 1)]
[NumThreads(NUM_THREADS, 1, 1)]
[GroupSharedLimit(NUM_BYTES_OF_SHARED_MEM)]
void NodeMain(DispatchNodeInputRecord<MY_INPUT_RECORD> myInput)
{
    uint tid = myInput.Get().data;
    uint iterations = NUM_DWORDS_SHARED_MEM / NUM_THREADS;
    
    for (uint i = 0; i < iterations; i++)
    {
        uint index = tid + i * NUM_THREADS;
        g_testBuffer[index] = index;
    }
    
    GroupMemoryBarrierWithGroupSync();

    // Write the shared data to the output buffer
    for (uint i = 0; i < iterations; i++)
    {
        uint index = tid + i * NUM_THREADS;
        g_output[index] = g_testBuffer[index];
    }
}
