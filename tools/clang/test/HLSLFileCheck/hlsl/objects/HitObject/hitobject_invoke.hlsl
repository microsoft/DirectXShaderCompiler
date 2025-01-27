// RUN: %dxc -T lib_6_9 -E main %s | FileCheck %s

// CHECK:   %[[HIT:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_MakeNop(i32 262)  ; HitObject_MakeNop()
// CHECK:   call void @dx.op.hitObject_Invoke.struct.Payload(i32 263, %dx.types.HitObject %[[HIT]], %struct.Payload* nonnull %{{[^ ]+}})  ; HitObject_Invoke(hitObject,payload)

RaytracingAccelerationStructure RTAS;
RWStructuredBuffer<float> UAV : register(u0);

struct [raypayload] Payload {
  float3 dummy : read(closesthit) : write(caller, anyhit);
};

[shader("raygeneration")] void main() {
  RayDesc rayDesc;
  rayDesc.Origin = float3(0.0, 1.0, 2.0);
  rayDesc.TMin = 3.0f;
  rayDesc.Direction = float3(4.0, 5.0, 6.0);
  rayDesc.TMax = 7.0f;

  Payload pld;
  pld.dummy = float3(7.0, 8.0, 9.0);

  HitObject hit;
  HitObject::Invoke(hit, pld);
}
