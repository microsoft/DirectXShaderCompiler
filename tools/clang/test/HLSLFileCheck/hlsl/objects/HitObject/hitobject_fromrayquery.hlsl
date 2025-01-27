// RUN: %dxc -T lib_6_9 -E main %s | FileCheck %s

// CHECK:   %[[RQ:[^ ]+]] = call i32 @dx.op.allocateRayQuery(i32 178, i32 5)  ; AllocateRayQuery(constRayFlags)
// CHECK:   call void @dx.op.rayQuery_TraceRayInline(i32 179, i32 %[[RQ]], %dx.types.Handle %{{[^ ]+}}, i32 0, i32 255, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 1.000000e+00, float 0.000000e+00, float 0.000000e+00, float 9.999000e+03)  ; RayQuery_TraceRayInline(rayQueryHandle,accelerationStructure,rayFlags,instanceInclusionMask,origin_X,origin_Y,origin_Z,tMin,direction_X,direction_Y,direction_Z,tMax)
// CHECK:   %[[RQHIT:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_FromRayQuery(i32 259, i32 %[[RQ]])  ; HitObject_FromRayQuery(rayQueryHandle)
// CHECK:   call void @dx.op.hitObject_Invoke.struct.Payload(i32 263, %dx.types.HitObject %[[RQHIT]],
// CHECK:   %[[IMPLATTRS:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_FromRayQueryWithAttrs.struct.CustomAttrs(i32 260, i32 %[[RQ]], %struct.CustomAttrs* nonnull %{{[^ ]+}})  ; HitObject_FromRayQueryWithAttrs(rayQueryHandle,CommittedAttribs)
// CHECK:   call void @dx.op.hitObject_Invoke.struct.Payload(i32 263, %dx.types.HitObject %[[IMPLATTRS]],
// CHECK:   %[[EXPLATTRS:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_FromRayQueryWithAttrs.struct.CustomAttrs(i32 260, i32 %[[RQ]], %struct.CustomAttrs* nonnull %{{[^ ]+}})  ; HitObject_FromRayQueryWithAttrs(rayQueryHandle,CommittedAttribs)
// CHECK:   call void @dx.op.hitObject_Invoke.struct.Payload(i32 263, %dx.types.HitObject %[[EXPLATTRS]],
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

struct CustomAttrs {
  float x;
  float y;
};

struct [raypayload] Payload {
  float3 dummy : read(closesthit) : write(caller, anyhit);
};

void CallInvoke(in HitObject hit) {
  RayDesc rayDesc;
  rayDesc.Origin = float3(0.0, 1.0, 2.0);
  rayDesc.TMin = 3.0f;
  rayDesc.Direction = float3(4.0, 5.0, 6.0);
  rayDesc.TMax = 7.0f;
  Payload pld = {{7.0, 8.0, 9.0}};
  HitObject::Invoke(hit, pld);
}

[shader("raygeneration")] void main() {
  RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
  RayDesc ray = MakeRayDesc();
  q.TraceRayInline(RTAS, RAY_FLAG_NONE, 0xFF, ray);

  CallInvoke(HitObject::FromRayQuery(q));

  CustomAttrs attrs = {1.f, 2.f};

  // Implicit template instantiation
  CallInvoke(HitObject::FromRayQuery(q, attrs));

  // Explicit template instantiation
  CallInvoke(HitObject::FromRayQuery<CustomAttrs>(q, attrs));
}
