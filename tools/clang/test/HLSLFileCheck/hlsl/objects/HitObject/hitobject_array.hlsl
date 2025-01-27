// RUN: %dxc -T lib_6_9 -E main %s 2&>1 | FileCheck %s

// CHECK-NOT: {{.*}}: error: HitObject is not a supported element type

RaytracingAccelerationStructure RTAS;
RWStructuredBuffer<float> UAV : register(u0);

struct [raypayload] Payload {
  float3 dummy : read(closesthit, anyhit) : write(caller, anyhit);
};

HitObject CreateHit(float f) {
  RayDesc rayDesc;
  rayDesc.Origin = float3(f, 1.f, 2.f);
  rayDesc.TMin = 3.0f;
  rayDesc.Direction = float3(f, 5.f, 6.f);
  rayDesc.TMax = 7.0f;

  Payload pld;
  pld.dummy = float3(f, 8.f, 9.f);

  return HitObject::TraceRay(
      RTAS,
      RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES,
      1,
      2,
      4,
      0,
      rayDesc,
      pld);
}

void InvokeArray(HitObject hits[2]) {
  Payload pld;
  pld.dummy = float3(100.f, 101.f, 102.f);
  HitObject::Invoke(hits[0], pld);
  HitObject::Invoke(hits[1], pld);
}

[shader("raygeneration")] void main() {
  HitObject hits[2];
  hits[0] = CreateHit(1.f);
  hits[1] = CreateHit(2.f);
  InvokeArray(hits);
}
