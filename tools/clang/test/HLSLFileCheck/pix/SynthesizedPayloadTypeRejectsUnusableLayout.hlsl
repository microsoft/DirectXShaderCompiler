// RUN: %dxc -EMSMain -Tms_6_6 %s | %opt -S -hlsl-dxil-pix-meshshader-output-instrumentation,expand-payload=1,UAVSize=8192,expanded-payload-size=16,expanded-payload-offset=6 | %FileCheck %s
// RUN: %dxc -EMSMain -Tms_6_6 %s | %opt -S -hlsl-dxil-pix-meshshader-output-instrumentation,expand-payload=1,UAVSize=8192,expanded-payload-size=18,expanded-payload-offset=4 | %FileCheck %s
// RUN: %dxc -EMSMain -Tms_6_6 %s | %opt -S -hlsl-dxil-pix-meshshader-output-instrumentation,expand-payload=1,UAVSize=8192,expanded-payload-size=12,expanded-payload-offset=4 | %FileCheck %s
// RUN: %dxc -EMSMain -Tms_6_6 %s | %opt -S -hlsl-dxil-pix-meshshader-output-instrumentation,expand-payload=1,UAVSize=8192,expanded-payload-size=16388,expanded-payload-offset=4 | %FileCheck %s
// RUN: %dxc -EMSMain -Tms_6_6 %s | %opt -S -hlsl-dxil-pix-meshshader-output-instrumentation,expand-payload=1,UAVSize=8192,expanded-payload-size=16,expanded-payload-offset=4294967292 | %FileCheck %s

// CHECK: MeshPayloadExpansionFailed
// CHECK-NOT: %PIX_AS2MS_Expanded_Type
// CHECK-NOT: getMeshPayload
// CHECK: %GroupIdX = call i32 @dx.op.groupId.i32(i32 94, i32 0)
// CHECK: %GroupIdY = call i32 @dx.op.groupId.i32(i32 94, i32 1)
// CHECK: %GroupIdZ = call i32 @dx.op.groupId.i32(i32 94, i32 2)
// CHECK: %PIX_DebugUAV_Handle = call %dx.types.Handle @dx.op.createHandleFromBinding
// CHECK: call void @dx.op.bufferStore.i32
// CHECK: call void @dx.op.storeVertexOutput.f32
// CHECK-NOT: %PIX_AS2MS_Expanded_Type
// CHECK-NOT: getMeshPayload

struct PSInput
{
    float4 position : SV_POSITION;
};

struct Payload
{
    uint value;
};

[outputtopology("triangle")]
[numthreads(4, 1, 1)]
void MSMain(
    in payload Payload payload,
    in uint tid : SV_GroupThreadID,
    out vertices PSInput verts[4],
    out indices uint3 triangles[2])
{
    SetMeshOutputCounts(4, 2);
    verts[tid].position = float4(0, 0, 0, 0);
    triangles[tid % 2] = uint3(0, tid + 1, tid + 2);
}
