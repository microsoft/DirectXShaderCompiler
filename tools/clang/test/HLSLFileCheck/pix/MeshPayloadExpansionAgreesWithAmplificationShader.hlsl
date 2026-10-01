// RUN: %dxc -EASMain -Tas_6_6 %s | %opt -S -hlsl-dxil-PIX-add-tid-to-as-payload,dispatchArgY=1,dispatchArgZ=1 | %FileCheck %s -check-prefixes=AMPLIFICATION
// RUN: %dxc -EMSMain -Tms_6_6 %s | %opt -S -hlsl-dxil-pix-meshshader-output-instrumentation,expand-payload=1,dispatchArgY=1,dispatchArgZ=1,UAVSize=8192,expanded-payload-size=28,expanded-payload-offset=16 | %FileCheck %s -check-prefixes=MESH

// AMPLIFICATION: ExpandedPayloadSize:28
// AMPLIFICATION: ExpandedPayloadAppendedFieldsOffset:16
// AMPLIFICATION: %PIX_AS2MS_Expanded_Type = type { [4 x i32], i32, i32, i32 }
// MESH: %PIX_AS2MS_Expanded_Type = type <{ i64, i32, [1 x i32], i32, i32, i32 }>
// MESH: [[PAYLOAD:%[0-9]+]] = call %PIX_AS2MS_Expanded_Type* @dx.op.getMeshPayload.PIX_AS2MS_Expanded_Type(i32 170)
// MESH: getelementptr %PIX_AS2MS_Expanded_Type, %PIX_AS2MS_Expanded_Type* [[PAYLOAD]], i32 0, i32 3
// MESH: getelementptr %PIX_AS2MS_Expanded_Type, %PIX_AS2MS_Expanded_Type* [[PAYLOAD]], i32 0, i32 4
// MESH: getelementptr %PIX_AS2MS_Expanded_Type, %PIX_AS2MS_Expanded_Type* [[PAYLOAD]], i32 0, i32 5
// MESH: getelementptr inbounds %PIX_AS2MS_Expanded_Type, %PIX_AS2MS_Expanded_Type* [[PAYLOAD]], i32 0, i32 1
// MESH: = !{{{![0-9]+}}, i32 3, i32 1, i32 2, i32 28}

struct AmplificationPayload
{
    uint4 values;
};

struct MeshPayload
{
    uint64_t alignmentAnchor;
    uint xOffsetSelector;
};

struct PSInput
{
    float4 position : SV_POSITION;
    uint selector : SELECTOR;
};

[numthreads(3, 1, 1)]
void ASMain(uint gid : SV_GroupID, uint tid : SV_GroupThreadID)
{
    AmplificationPayload payload;
    payload.values = uint4(0, 0, tid, 0);
    DispatchMesh(1, 1, 1, payload);
}

[outputtopology("triangle")]
[numthreads(3, 1, 1)]
void MSMain(
    in uint tid : SV_GroupThreadID,
    in payload MeshPayload payload,
    out vertices PSInput verts[3],
    out indices uint3 tris[1])
{
    SetMeshOutputCounts(3, 1);
    verts[tid].position = float4(0, 0, 0, 1);
    verts[tid].selector = 100 + payload.xOffsetSelector;
    if (tid == 0)
    {
        tris[0] = uint3(0, 1, 2);
    }
}
