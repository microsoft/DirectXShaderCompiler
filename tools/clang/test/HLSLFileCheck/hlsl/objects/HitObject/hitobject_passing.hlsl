// RUN: %dxc -T lib_6_9 -E main %s | FileCheck %s

// CHECK:   %[[MISS:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_MakeMiss(i32 261, i32 513, i32 0, float 0.000000e+00, float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00, float 5.000000e+00, float 6.000000e+00, float 7.000000e+00)  ; HitObject_MakeMiss(RayFlags,MissShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax)
// CHECK:   call void @dx.op.hitObject_Invoke.struct.Payload(i32 263, %dx.types.HitObject %[[MISS]], %struct.Payload* nonnull %[[PLD:[^ ]+]])  ; HitObject_Invoke(hitObject,payload)
// CHECK:   %{{[^ ]+}} = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160,
// CHECK:   %[[HANDLE:[^ ]+]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %{{[^ ]+}}, %dx.types.ResourceProperties { i32 16, i32 0 })  ; AnnotateHandle(res,props)  resource: RTAccelerationStructure
// CHECK:   %[[TRACE:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_TraceRay.struct.Payload(i32 258, %dx.types.Handle %[[HANDLE]], i32 513, i32 1, i32 2, i32 4, i32 0, float 1.000000e+01, float 1.100000e+01, float 1.200000e+01, float 1.300000e+01, float 1.400000e+01, float 1.500000e+01, float 1.600000e+01, float 1.700000e+01, %struct.Payload* nonnull %[[PLD]])  ; HitObject_TraceRay(accelerationStructure,rayFlags,instanceInclusionMask,rayContributionToHitGroupIndex,multiplierForGeometryContributionToHitGroupIndex,missShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax,payload)
// CHECK:   call void @dx.op.hitObject_Invoke.struct.Payload(i32 263, %dx.types.HitObject %[[TRACE]], %struct.Payload* nonnull %[[PLD]])  ; HitObject_Invoke(hitObject,payload)

RaytracingAccelerationStructure RTAS;
RWStructuredBuffer<float> UAV : register(u0);

struct [raypayload] Payload {
  float3 dummy : write(caller, anyhit) : read(closesthit);
};

HitObject foo(inout HitObject hit, inout Payload pld) {
  HitObject::Invoke(hit, pld);
  RayDesc rayDesc;
  rayDesc.Origin = float3(10.0, 11.0, 12.0);
  rayDesc.TMin = 13.0f;
  rayDesc.Direction = float3(14.0, 15.0, 16.0);
  rayDesc.TMax = 17.0f;

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

[shader("raygeneration")] void main() {
  RayDesc rayDesc;
  rayDesc.Origin = float3(0.0, 1.0, 2.0);
  rayDesc.TMin = 3.0f;
  rayDesc.Direction = float3(4.0, 5.0, 6.0);
  rayDesc.TMax = 7.0f;

  Payload pld;
  pld.dummy = float3(7.0, 8.0, 9.0);

  HitObject hit = HitObject::MakeMiss(RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, 0, rayDesc);

  hit = foo(hit, pld);

  HitObject::Invoke(hit, pld);
}
