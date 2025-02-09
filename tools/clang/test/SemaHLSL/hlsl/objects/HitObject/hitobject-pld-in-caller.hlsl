// RUN: %dxc -T lib_6_9 %s -verify

RaytracingAccelerationStructure RTAS;
RWStructuredBuffer<float> UAV : register(u0);

RayDesc MakeRayDesc() {
  RayDesc desc;
  desc.Origin = float3(0, 0, 0);
  desc.Direction = float3(1, 0, 0);
  desc.TMin = 0.0f;
  desc.TMax = 9999.0;
  return desc;
}

struct [raypayload] Payload {
  HitObject hit : write(anyhit) : read(closesthit);
};

[shader("raygeneration")]
void
main() {
  // expected-error@+2{{payload parameter 'pldTraceRayCallArg' must be a user-defined type composed of only numeric types}}
  RayDesc rayDesc = MakeRayDesc();
  Payload pldTraceRayCallArg;
  HitObject hit = HitObject::TraceRay(
      RTAS,
      RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES,
      1,
      2,
      4,
      0,
      rayDesc,
      pldTraceRayCallArg);
  // expected-error@+1{{payload parameter 'pldInvokeCallArg' must be a user-defined type composed of only numeric types}}
  Payload pldInvokeCallArg;
  HitObject::Invoke(hit, pldInvokeCallArg);
}
