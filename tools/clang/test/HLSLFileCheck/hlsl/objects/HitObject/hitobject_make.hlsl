// RUN: %dxc -O0 -T lib_6_9 -E main %s | FileCheck %s

// CHECK:   %[[NOPONE:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_MakeNop(i32 262)  ; HitObject_MakeNop()
// CHECK:   call void @dx.op.hitObject_Invoke.struct.Payload(i32 263, %dx.types.HitObject %[[NOPONE]]
// CHECK:   %[[NOPTWO:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_MakeNop(i32 262)  ; HitObject_MakeNop()
// CHECK:   call void @dx.op.hitObject_Invoke.struct.Payload(i32 263, %dx.types.HitObject %[[NOPTWO]]
// CHECK:   %[[NOPTHREE:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_MakeNop(i32 262)  ; HitObject_MakeNop()
// CHECK:   call void @dx.op.hitObject_Invoke.struct.Payload(i32 263, %dx.types.HitObject %[[NOPTHREE]]
// CHECK:   %[[MISS:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_MakeMiss(i32 261, i32 4, i32 0, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 1.000000e+00, float 0.000000e+00, float 0.000000e+00, float 9.999000e+03)  ; HitObject_MakeMiss(RayFlags,MissShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax)
// CHECK:   call void @dx.op.hitObject_Invoke.struct.Payload(i32 263, %dx.types.HitObject %[[MISS]]

RayDesc MakeRayDesc() {
  RayDesc desc;
  desc.Origin = float3(0, 0, 0);
  desc.Direction = float3(1, 0, 0);
  desc.TMin = 0.0f;
  desc.TMax = 9999.0;
  return desc;
}

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
  {
    HitObject hit;
    CallInvoke(hit);
  }
  {
    HitObject hit = HitObject::MakeNop();
    CallInvoke(hit);
  }
  {
    CallInvoke(HitObject::MakeNop());
  }

  CallInvoke(HitObject::MakeMiss(RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0, MakeRayDesc()));
}
