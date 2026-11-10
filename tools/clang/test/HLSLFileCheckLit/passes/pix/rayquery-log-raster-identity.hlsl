// RUN: %dxc -T vs_6_5 -E VSMain %s | %dxopt -S -hlsl-dxil-pix-rayquery-log,maxNumEntriesInLog=8 | FileCheck %s --check-prefix=VS
// RUN: %dxc -T ps_6_5 -E PSMain %s | %dxopt -S -hlsl-dxil-pix-rayquery-log,maxNumEntriesInLog=8 | FileCheck %s --check-prefix=PS-NO-POS
// RUN: %dxc -T ps_6_5 -E PSMain %s | %dxopt -S -hlsl-dxil-pix-rayquery-log,maxNumEntriesInLog=8,upstreamSVPositionRow=0 | FileCheck %s --check-prefix=PS-POS
// RUN: %dxc -T gs_6_5 -E GSMain %s | %dxopt -S -hlsl-dxil-pix-rayquery-log,maxNumEntriesInLog=8 | FileCheck %s --check-prefix=GS

// VS-DAG: SV_VertexID
// VS-DAG: SV_InstanceID
// VS-DAG: %IrtVertexId = call i32 @dx.op.loadInput.i32
// VS-DAG: %IrtInstanceId = call i32 @dx.op.loadInput.i32

// PS-NO-POS-NOT: IrtPixelPosition
// PS-NO-POS: or i32 %{{.*}}, 1073741824

// PS-POS: %IrtPixelPosition = call float @dx.op.loadInput.f32
// PS-POS: %IrtPixelPositionIndex = fptoui float %IrtPixelPosition to i32
// PS-POS: or i32 %{{.*}}, 1073741824
// PS-POS-NEXT: select i1 false, i32 %{{.*}}, i32 %{{.*}}
// PS-POS: SV_Position

// GS: %IrtPrimitiveId = call i32 @dx.op.primitiveID.i32
// GS: %IrtGeometryShaderInstanceId = call i32 @dx.op.gsInstanceID.i32
// GS-NOT: SV_GSInstanceID
// GS-NOT: %IrtPrimitiveId = call i32 @dx.op.loadInput.i32
// GS-NOT: %IrtGeometryShaderInstanceId = call i32 @dx.op.loadInput.i32

RaytracingAccelerationStructure Scene : register(t0);

struct VertexOutput
{
    float4 position : SV_Position;
};

void RunQuery()
{
    RayDesc ray;
    ray.Origin = float3(0.0, 0.0, 0.0);
    ray.Direction = float3(0.0, 0.0, 1.0);
    ray.TMin = 0.0;
    ray.TMax = 100.0;

    RayQuery<RAY_FLAG_NONE> query;
    query.TraceRayInline(Scene, RAY_FLAG_NONE, 0xff, ray);
    while (query.Proceed())
    {
    }
}

float4 VSMain() : SV_Position
{
    RunQuery();
    return float4(0.0, 0.0, 0.0, 1.0);
}

float4 PSMain() : SV_Target
{
    RunQuery();
    return float4(1.0, 0.0, 0.0, 1.0);
}

[maxvertexcount(3)]
[instance(2)]
void GSMain(triangle VertexOutput input[3], inout TriangleStream<VertexOutput> stream)
{
    RunQuery();
    stream.Append(input[0]);
    stream.Append(input[1]);
    stream.Append(input[2]);
}
