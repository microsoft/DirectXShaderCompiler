// RUN: %dxc -T ds_6_5 -E main %s | %dxopt -S -hlsl-dxil-pix-rayquery-log,maxNumEntriesInLog=8 | FileCheck %s

// SV_PrimitiveID is not declared by this domain shader, so it has no signature
// element. The pass must still read it through dx.op.primitiveID for identity.

// CHECK: %IrtPrimitiveId = call i32 @dx.op.primitiveID.i32(i32 108)
// CHECK: @dx.op.domainLocation.f32(i32 105, i8 0)
// CHECK: @dx.op.domainLocation.f32(i32 105, i8 1)

RaytracingAccelerationStructure Scene : register(t0);
RWStructuredBuffer<float> Output : register(u0);

struct PatchConstants
{
    float edges[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

struct ControlPoint
{
    float4 position : SV_Position;
};

[domain("tri")]
ControlPoint main(PatchConstants constants, float3 domainLocation : SV_DomainLocation, const OutputPatch<ControlPoint, 3> patch)
{
    RayDesc ray;
    ray.Origin = patch[0].position.xyz;
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.0f;
    ray.TMax = 100.0f;

    RayQuery<RAY_FLAG_NONE> query;
    query.TraceRayInline(Scene, RAY_FLAG_NONE, 0xFF, ray);
    query.Proceed();
    Output[0] = query.CommittedRayT();

    ControlPoint output;
    output.position = patch[0].position * domainLocation.x + patch[1].position * domainLocation.y + patch[2].position * domainLocation.z;
    return output;
}
