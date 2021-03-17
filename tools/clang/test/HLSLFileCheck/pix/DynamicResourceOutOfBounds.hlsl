// RUN: %dxc -EMain -Tcs_6_6 %s | %opt -S -hlsl-dxil-pix-shader-access-instrumentation,config=.256;272;1024. | %FileCheck %s

static RWByteAddressBuffer DynamicBuffer = ResourceDescriptorHeap[1];
[numthreads(1, 1, 1)]
void Main()
{
    uint val = DynamicBuffer.Load(0u);
    DynamicBuffer.Store(0u, val);
}

// check it's 6.6:
// CHECK: call %dx.types.Handle @dx.op.createHandleFromBinding

// Offset for out-of-bounds should be 256
// CHECK:i32 256, i32 undef, i32 1375731712
// CHECK:rawBufferLoad
