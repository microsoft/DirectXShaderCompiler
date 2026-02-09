// REQUIRES: dxil-1-10

// Pass: GSM fits within the GroupSharedLimit
// RUN: %dxc -E main -T ms_6_10 -DGSM_EXTRA=0 -DUSE_GROUP_SHARED_LIMIT %s | FileCheck %s
// CHECK: @main

// Fail: GSM exceeds GroupSharedLimit (32772 > 32768)
// RUN: not %dxc -E main -T ms_6_10 -DGSM_EXTRA=1 -DUSE_GROUP_SHARED_LIMIT %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERROR
// CHECK-ERROR: Total Thread Group Shared Memory storage is 32772, exceeded 32768.

// Fail: GSM exceeds default mesh shader limit (32772 > 28672)
// RUN: not %dxc -E main -T ms_6_10 -DGSM_EXTRA=1 %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERROR2
// CHECK-ERROR2: Total Thread Group Shared Memory storage is 32772, exceeded 28672.

#define NUM_BYTES_OF_SHARED_MEM (32*1024)
#define NUM_DWORDS_SHARED_MEM (NUM_BYTES_OF_SHARED_MEM / 4)
#define NUM_THREADS 32
#define MAX_VERT 32
#define MAX_PRIM 16

#ifndef GSM_EXTRA
#define GSM_EXTRA 0
#endif

groupshared uint g_testBuffer[NUM_DWORDS_SHARED_MEM + GSM_EXTRA];

struct MeshPerVertex {
    float4 position : SV_Position;
};

struct MeshPerPrimitive {
    float normal : NORMAL;
};

struct MeshPayload {
    float data;
};

#ifdef USE_GROUP_SHARED_LIMIT
[GroupSharedLimit(NUM_BYTES_OF_SHARED_MEM)]
#endif
[numthreads(NUM_THREADS, 1, 1)]
[outputtopology("triangle")]
void main(
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
        g_testBuffer[index] = index;
    }
    
    // synchronize all threads in the group
    GroupMemoryBarrierWithGroupSync();

    // Use shared memory data to set vertex positions
    if (tig < MAX_VERT)
    {
        verts[tig].position = float4(g_testBuffer[tig], 0, 0, 1);
    }
    
    if (tig < MAX_PRIM)
    {
        primIndices[tig] = uint3(tig % MAX_VERT, (tig + 1) % MAX_VERT, (tig + 2) % MAX_VERT);
        prims[tig].normal = mpl.data;
    }
}
