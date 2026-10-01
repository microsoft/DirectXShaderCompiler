// RUN: %dxc -EMSMain -Tms_6_6 %s | %opt -S -hlsl-dxil-pix-meshshader-output-instrumentation,UAVSize=8192 | %FileCheck %s

// CHECK-NOT: @dx.op.emitIndices
// CHECK: %PIX_DebugUAV_Handle = call %dx.types.Handle @dx.op.createHandleFromBinding
// CHECK: call void @dx.op.bufferStore.i32
// CHECK: call void @dx.op.storeVertexOutput.f32
// CHECK-NOT: @dx.op.emitIndices
// CHECK-NOT: declare void @dx.op.storeVertexOutput.i16
// CHECK-NOT: declare void @dx.op.storeVertexOutput.i32
// CHECK-NOT: declare void @dx.op.storeVertexOutput.f16

struct PSInput
{
    float4 position : SV_POSITION;
};

[outputtopology("triangle")]
[numthreads(4, 1, 1)]
void MSMain(
    in uint tid : SV_GroupThreadID,
    out vertices PSInput verts[4])
{
    SetMeshOutputCounts(4, 0);
    verts[tid].position = float4(0, 0, 0, 0);
}
