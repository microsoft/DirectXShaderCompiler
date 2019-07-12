// RUN: %dxc -T cs_6_5 -E CS %s | FileCheck %s

//  CHECK: define void @CS()

RaytracingAccelerationStructure AccelerationStructure : register(t0);
RWByteAddressBuffer log : register(u0);

RayDesc MakeRayDesc()
{
    RayDesc desc;
    desc.Origin = float3(0,0,0);
    desc.Direction = float3(1,0,0);
    desc.TMin = 0.0f;
    desc.TMax = 9999.0;
    return desc;
}

void DoSomething()
{
    log.Store(0,1);
}

[numThreads(1,1,1)]
void CS()
{
    RayQuery<RAY_FLAG_FORCE_OPAQUE|RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
    RayDesc ray = MakeRayDesc();
    q.TraceRayInline(AccelerationStructure,RAY_FLAG_NONE,0xFF,ray);
    float4x3 mat4x3;
    float3x4 mat3x4;
    while(q.Proceed())
    {
        switch(q.CandidateType())
        {
        case CANDIDATE_NON_OPAQUE_TRIANGLE:
            q.Abort();
            mat3x4 = q.CandidateObjectToWorld3x4();
            mat4x3 = q.CandidateObjectToWorld4x3();
            q.CommitNonOpaqueTriangleHit();
            if(q.CandidateTriangleFrontFace())
            {
                DoSomething();
            }
            if(q.CandidateTriangleBarycentrics().x == 0)
            {
                DoSomething();
            }
            if(q.CandidateGeometryIndex())
            {
                DoSomething();
            }
            if(q.CandidateInstanceID())
            {
                DoSomething();
            }
            if(q.CandidateInstanceIndex())
            {
                DoSomething();
            }
            if(q.CandidateObjectRayDirection().x)
            {
                DoSomething();
            }
            if(q.CandidateObjectRayOrigin().y)
            {
                DoSomething();
            }
            if(q.CandidatePrimitiveIndex())
            {
                DoSomething();
            }
            if(q.CandidateTriangleRayT())
            {
                DoSomething();
            }
            break;
        case CANDIDATE_PROCEDURAL_PRIMITIVE:
        {
            mat3x4 = q.CandidateWorldToObject3x4();
            mat4x3 = q.CandidateWorldToObject4x3();
            if(q.CandidateProceduralPrimitiveNonOpaque())
            {
                DoSomething();
            }
            float t = 0.5;
            q.CommitProceduralPrimitiveHit(t);
            q.Abort();
            break;
        }
        }
    }
    if(mat3x4[0][0] == mat4x3[0][0])
    {
        DoSomething();
    }
    switch(q.CommittedStatus())
    {
    case COMMITTED_NOTHING:
        mat3x4 = q.CommittedObjectToWorld3x4();
        mat4x3 = q.CommittedObjectToWorld4x3();
        break;
    case COMMITTED_TRIANGLE_HIT:
        mat3x4 = q.CommittedWorldToObject3x4();
        mat4x3 = q.CommittedWorldToObject4x3();
        if(q.CommittedTriangleFrontFace())
        {
            DoSomething();
        }
        if(q.CommittedTriangleBarycentrics().y == 0)
        {
            DoSomething();
        }
        break;
    case COMMITTED_PROCEDURAL_PRIMITIVE_HIT:
        if(q.CommittedGeometryIndex())
        {
            DoSomething();
        }
        if(q.CommittedInstanceID())
        {
            DoSomething();
        }
        if(q.CommittedInstanceIndex())
        {
            DoSomething();
        }
        if(q.CommittedObjectRayDirection().z)
        {
            DoSomething();
        }
        if(q.CommittedObjectRayOrigin().x)
        {
            DoSomething();
        }
        if(q.CommittedPrimitiveIndex())
        {
            DoSomething();
        }
        if(q.CommittedRayT())
        {
            DoSomething();
        }
        break;
    }
    if(mat3x4[0][0] == mat4x3[0][0])
    {
        DoSomething();
    }
    if(q.RayFlags())
    {
        DoSomething();
    }
    if(q.RayTMin())
    {
        DoSomething();
    }
    float3 o = q.WorldRayDirection();
    float3 d = q.WorldRayOrigin();
    if(o.x == d.z)
    {
        DoSomething();
    }
}