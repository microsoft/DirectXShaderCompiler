// RUN: %dxc -E main -T ps_6_5 -fspv-target-env=vulkan1.2

// RUN: %dxc -T gs_6_5 -E main
// CHECK:  OpCapability RayQueryKHR
// CHECK:  OpExtension "SPV_KHR_ray_query

RaytracingAccelerationStructure AccelerationStructure : register(t0);
RayDesc MakeRayDesc()
{
    RayDesc desc;
    desc.Origin = float3(0,0,0);
    desc.Direction = float3(1,0,0);
    desc.TMin = 0.0f;
    desc.TMax = 9999.0;
    return desc;
}
void doInitialize(RayQuery<RAY_FLAG_FORCE_OPAQUE> query, RayDesc ray)
{
    query.TraceRayInline(AccelerationStructure,RAY_FLAG_FORCE_NON_OPAQUE,0xFF,ray);
}

uint4 main() : SV_Target
{
  RayQuery<RAY_FLAG_FORCE_OPAQUE> q;
  RayDesc ray = MakeRayDesc();
// CHECK:  [[accel:%\d+]] = OpLoad %accelerationStructureNV %AccelerationStructure
// CHECK:  OpRayQueryInitializeKHR [[rayquery]] [[accel]] %uint_1 %uint_255 {{%\d+}} %float_0 {{%\d+}} %float_9999

  q.TraceRayInline(AccelerationStructure,RAY_FLAG_FORCE_OPAQUE, 0xFF, ray);
// CHECK: OpRayQueryInitializeKHR [[rayquery]] [[accel]] %uint_3 %uint_255 {{%\d+}} %float_0 {{%\d+}} %float_9999
  doInitialize(q, ray);
  return float4(1.0, 0.0, 0.0, 1.0);
}