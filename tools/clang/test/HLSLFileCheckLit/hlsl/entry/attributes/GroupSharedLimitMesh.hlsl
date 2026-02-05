// REQUIRES: dxil-1-10
// RUN: %dxc -E MainPass -T ms_6_10 %s | FileCheck %s

#define NUM_BYTES_OF_SHARED_MEM (32*1024)
#define NUM_DWORDS_SHARED_MEM (NUM_BYTES_OF_SHARED_MEM / 4)
#define NUM_THREADS 32
#define MAX_VERT 32
#define MAX_PRIM 16

groupshared uint g_testBufferPASS[NUM_DWORDS_SHARED_MEM];

struct MeshPerVertex {
    float4 position : SV_Position;
};

struct MeshPerPrimitive {
    float normal : NORMAL;
};

struct MeshPayload {
    float data;
};

// CHECK: @MainPass

[GroupSharedLimit(NUM_BYTES_OF_SHARED_MEM)]
[numthreads(NUM_THREADS, 1, 1)]
[outputtopology("triangle")]
void MainPass(
            out indices uint3 primIndices[MAX_PRIM],
            out vertices MeshPerVertex verts[MAX_VERT],
            out primitives MeshPerPrimitive prims[MAX_PRIM],
            in payload MeshPayload mpl,
            in uint tig : SV_GroupIndex
         )
{
    SetMeshOutputCounts(MAX_VERT, MAX_PRIM);
    
    uint iterations = NUM_DWORDS_SHARED_MEM / NUM_THREADS;
    
    for (uint i = 0; i < iterations; i++)
    {
        uint index = tig + i * NUM_THREADS;
        g_testBufferPASS[index] = index;
    }
    
    // synchronize all threads in the group
    GroupMemoryBarrierWithGroupSync();

    // Use shared memory data to set vertex positions
    if (tig < MAX_VERT)
    {
        verts[tig].position = float4(g_testBufferPASS[tig], 0, 0, 1);
    }
    
    if (tig < MAX_PRIM)
    {
        primIndices[tig] = uint3(tig % MAX_VERT, (tig + 1) % MAX_VERT, (tig + 2) % MAX_VERT);
        prims[tig].normal = mpl.data;
    }
}

// RUN: not %dxc -E MainFail -T ms_6_10 %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERROR
// CHECK-ERROR: Total Thread Group Shared Memory storage is 32772, exceeded 32768.

groupshared uint g_testBufferFAIL[NUM_DWORDS_SHARED_MEM + 1];

[GroupSharedLimit(NUM_BYTES_OF_SHARED_MEM)]
[numthreads(NUM_THREADS, 1, 1)]
[outputtopology("triangle")]
void MainFail(
            out indices uint3 primIndices[MAX_PRIM],
            out vertices MeshPerVertex verts[MAX_VERT],
            out primitives MeshPerPrimitive prims[MAX_PRIM],
            in payload MeshPayload mpl,
            in uint tig : SV_GroupIndex
         )
{
    SetMeshOutputCounts(MAX_VERT, MAX_PRIM);
    
    uint iterations = NUM_DWORDS_SHARED_MEM / NUM_THREADS;
    
    for (uint i = 0; i < iterations; i++)
    {
        uint index = tig + i * NUM_THREADS;
        g_testBufferFAIL[index] = index;
    }
    
    // synchronize all threads in the group
    GroupMemoryBarrierWithGroupSync();

    // Use shared memory data to set vertex positions
    if (tig < MAX_VERT)
    {
        verts[tig].position = float4(g_testBufferFAIL[tig], 0, 0, 1);
    }
    
    if (tig < MAX_PRIM)
    {
        primIndices[tig] = uint3(tig % MAX_VERT, (tig + 1) % MAX_VERT, (tig + 2) % MAX_VERT);
        prims[tig].normal = mpl.data;
    }
}

// RUN: not %dxc -E MainFail2 -T ms_6_10 %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERROR2
// CHECK-ERROR2: Total Thread Group Shared Memory storage is 32772, exceeded 28672.

[numthreads(NUM_THREADS, 1, 1)]
[outputtopology("triangle")]
void MainFail2(
            out indices uint3 primIndices[MAX_PRIM],
            out vertices MeshPerVertex verts[MAX_VERT],
            out primitives MeshPerPrimitive prims[MAX_PRIM],
            in payload MeshPayload mpl,
            in uint tig : SV_GroupIndex
         )
{
    SetMeshOutputCounts(MAX_VERT, MAX_PRIM);
    
    uint iterations = NUM_DWORDS_SHARED_MEM / NUM_THREADS;
    
    for (uint i = 0; i < iterations; i++)
    {
        uint index = tig + i * NUM_THREADS;
        g_testBufferFAIL[index] = index;
    }
    
    // synchronize all threads in the group
    GroupMemoryBarrierWithGroupSync();

    // Use shared memory data to set vertex positions
    if (tig < MAX_VERT)
    {
        verts[tig].position = float4(g_testBufferFAIL[tig], 0, 0, 1);
    }
    
    if (tig < MAX_PRIM)
    {
        primIndices[tig] = uint3(tig % MAX_VERT, (tig + 1) % MAX_VERT, (tig + 2) % MAX_VERT);
        prims[tig].normal = mpl.data;
    }
}
