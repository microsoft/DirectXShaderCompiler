// RUN: %dxc -T lib_6_9 -E main %s | FileCheck %s

// CHECK:   %[[HIT:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_TraceRay.struct.Payload(i32 258, %dx.types.Handle %{{.+}}, i32 513, i32 1, i32 2, i32 4, i32 0, float 0.000000e+00, float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00, float 5.000000e+00, float 6.000000e+00, float 7.000000e+00, %struct.Payload* nonnull %{{[^ ]+}})  ; HitObject_TraceRay(accelerationStructure,rayFlags,instanceInclusionMask,rayContributionToHitGroupIndex,multiplierForGeometryContributionToHitGroupIndex,missShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax,payload)
// CHECK:   call void @dx.op.reorderThread(i32 264, %dx.types.HitObject %[[HIT]], i32 undef, i32 0)  ; ReorderThread(hitObject,coherenceHint,numCoherenceHintBitsFromLSB)

RaytracingAccelerationStructure RTAS;
RWStructuredBuffer<float> UAV : register(u0);

struct[raypayload] Payload {
  float3 dummy : read(closesthit) : write(caller, anyhit);
};

struct HitObjectWrapper {
  HitObject handle;

  // HLSL does not support constructors but HitObjectImpl needs adequate default initialization:
  // HitObject() : handle(__builtin_HitObjectImpl_MakeNop()) {}

  template <typename payload_t>
  static HitObjectWrapper TraceRay(
      in RaytracingAccelerationStructure AccelerationStructure,
      in uint RayFlags,
      in uint InstanceInclusionMask,
      in uint RayContributionToHitGroupIndex,
      in uint MultiplierForGeometryContributionToHitGroupIndex,
      in uint MissShaderIndex,
      in RayDesc Ray,
      inout payload_t Payload) {
    HitObjectWrapper wrapper;
    wrapper.handle = HitObject::TraceRay(AccelerationStructure, RayFlags, InstanceInclusionMask, RayContributionToHitGroupIndex, MultiplierForGeometryContributionToHitGroupIndex, MissShaderIndex, Ray, Payload);
    return wrapper;
  }
};

static void ReorderThreadWrapper(in HitObjectWrapper hitWrap) {
  ReorderThread(hitWrap.handle);
}

[shader("raygeneration")] void main() {
  RayDesc rayDesc;
  rayDesc.Origin = float3(0.0, 1.0, 2.0);
  rayDesc.TMin = 3.0f;
  rayDesc.Direction = float3(4.0, 5.0, 6.0);
  rayDesc.TMax = 7.0f;

  Payload pld;
  pld.dummy = float3(7.0, 8.0, 9.0);

  HitObjectWrapper wrap = HitObjectWrapper::TraceRay(
      RTAS,
      RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES,
      1,
      2,
      4,
      0,
      rayDesc,
      pld);
  ReorderThreadWrapper(wrap);
}
