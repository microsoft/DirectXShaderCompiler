// RUN: %dxc %s -T lib_6_9 -DHIT1=1 -DHIT2=1 -DHIT3=1 -DHIT4=1
// RUN: %dxc %s -T lib_6_9 -DHIT1=0 -DHIT2=1 -DHIT3=1 -DHIT4=0
// RUN: %dxc %s -T lib_6_9 -DHIT1=1 -DHIT2=0 -DHIT3=0 -DHIT4=1
// RUN: %dxc %s -T lib_6_9 -DHIT1=1 -DHIT2=0 -DHIT3=1 -DHIT4=0
// RUN: %dxc %s -T lib_6_9 -DHIT1=0 -DHIT2=1 -DHIT3=0 -DHIT4=1

struct[raypayload] PerRayData
{ 
    uint dummy : read(anyhit,closesthit,miss,caller) : write(anyhit,miss,closesthit,caller);
};

struct Attrs
{
    float2 barycentrics : BARYCENTRICS;
};

RaytracingAccelerationStructure topObject : register(t0);

[shader("raygeneration")]
void raygen()
{
    RayDesc ray = {{0, 1, 2}, 3,  {4, 5, 6}, 7};

    PerRayData payload;
#if HIT1
    dx::HitObject hit1 = dx::HitObject::MakeMiss(RAY_FLAG_NONE, 0, ray);
    dx::MaybeReorderThread(hit1);
#endif
#if HIT2
    dx::HitObject hit2 = dx::HitObject::TraceRay(topObject, RAY_FLAG_SKIP_TRIANGLES, 0xFF, 0, 0, 0, ray, payload);
    dx::MaybeReorderThread(hit2);
#endif
#if HIT3
    RayQuery<RAY_FLAG_NONE> rayQuery;
    rayQuery.TraceRayInline(topObject, RAY_FLAG_NONE, 0xFF, ray);
    dx::HitObject hit3 = dx::HitObject::FromRayQuery(rayQuery);
    dx::MaybeReorderThread(hit3);
#endif
#if HIT4
    TraceRay(topObject, RAY_FLAG_SKIP_TRIANGLES, 0xFF, 0, 0, 0, ray, payload);
#endif
}
