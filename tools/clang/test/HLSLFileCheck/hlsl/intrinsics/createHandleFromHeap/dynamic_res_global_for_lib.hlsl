// RUN: %dxc -T lib_6_6 %s | %FileCheck %s

// Make sure each entry get 2 createHandleFromHeap.
// CHECK:define void
// CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 218, i32 %{{.*}}, i1 false, i1 false)
// CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 218, i32 %{{.*}}, i1 false, i1 false)
// CHECK:define void
// CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 218, i32 %{{.*}}, i1 false, i1 false)
// CHECK:call %dx.types.Handle @dx.op.createHandleFromHeap(i32 218, i32 %{{.*}}, i1 false, i1 false)

uint ID;
static RWBuffer<float>           g_result      = ResourceDescriptorHeap[ID];
static ByteAddressBuffer         g_rawBuf      = ResourceDescriptorHeap[ID+1];

[NumThreads(1, 1, 1)]
[RootSignature("RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | SAMPLER_HEAP_DIRECTLY_INDEXED), RootConstants(num32BitConstants=1, b0))")]
void csmain(uint ix : SV_GroupIndex)
{
  g_result[ix] = g_rawBuf.Load<float>(ix);
}


[NumThreads(1, 1, 1)]
[RootSignature("RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | SAMPLER_HEAP_DIRECTLY_INDEXED), RootConstants(num32BitConstants=1, b0)")]
void csmain2(uint ix : SV_GroupIndex)
{
  g_result[ix] = g_rawBuf.Load<float>(ix+ID);
}

