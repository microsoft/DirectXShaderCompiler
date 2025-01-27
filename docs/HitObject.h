// == Prototype header for Shader Execution Reordering
//
// This is a partial implementation of the HitObject API building on lower-level
// Clang builtin functions and types. It is intended to inform the design and
// highlight challenges in implementing the HitObject API for HLSL in Clang,
// that is github.com/llvm/llvm-project. Limitations of the HLSL language make a
// complete header-only implementation impossible. Comments in the code discuss
// missing functions and why the header cannot implement them.

struct HitObject {
  // Lower-level builtin type that lowers to %dx.types.HitObject in DXIL.
  __builtin_HitObject handle;

  // HLSL does not support constructors but HitObject needs adequate
  // default initialization:
  //
  // HitObject() : handle(__builtin_HitObject_MakeNop())
  // {}

  template <typename payload_t>
  static HitObject
  TraceRay(RaytracingAccelerationStructure AccelerationStructure, uint RayFlags,
           uint InstanceInclusionMask, uint RayContributionToHitGroupIndex,
           uint MultiplierForGeometryContributionToHitGroupIndex,
           uint MissShaderIndex, RayDesc Ray, inout payload_t Payload) {
    HitObject ho;
    ho.handle = __builtin_HitObject_TraceRay(
        AccelerationStructure, RayFlags, InstanceInclusionMask,
        RayContributionToHitGroupIndex,
        MultiplierForGeometryContributionToHitGroupIndex, MissShaderIndex, Ray,
        Payload);
    return ho;
  }
  template <typename payload_t>
  static void Invoke(in HitObject ho, inout payload_t Payload) {
    __builtin_HitObject_Invoke(ho.handle, Payload);
  }
  template <uint rayFlags>
  static HitObject FromRayQuery(in RayQuery<rayFlags> rq) {
    HitObject ho;
    ho.handle = __builtin_HitObject_FromRayQuery(rq);
    return ho;
  }
  template <uint rayFlags, typename attr_t>
  static HitObject FromRayQuery(in RayQuery<rayFlags> rq,
                                attr_t CommittedCustomAttribs) {
    HitObject ho;
    ho.handle =
        __builtin_HitObject_FromRayQuery(rq, CommittedCustomAttribs);
    return ho;
  }
  static HitObject MakeMiss(in uint rayFlags, in uint missShaderIndex, in RayDesc ray) {
    HitObject ho;
    ho.handle =
        __builtin_HitObject_MakeMiss(rayFlags, missShaderIndex, ray);
    return ho;
  }
  static HitObject MakeNop() {
    HitObject ho;
    ho.handle = __builtin_HitObject_MakeNop();
    return ho;
  }
  bool IsMiss() { return __builtin_HitObject_IsMiss(handle); }
  bool IsHit() { return __builtin_HitObject_IsHit(handle); }
  bool IsNop() { return __builtin_HitObject_IsNop(handle); }
  uint GetRayFlags() { return __builtin_HitObject_GetRayFlags(handle); }
  float GetRayTMin() { return __builtin_HitObject_GetRayTMin(handle); }
  float GetRayTCurrent() { return __builtin_HitObject_GetRayTCurrent(handle); }
  float3 GetWorldRayOrigin() {
    return __builtin_HitObject_GetWorldRayOrigin(handle);
  }
  float3 GetWorldRayDirection() {
    return __builtin_HitObject_GetWorldRayDirection(handle);
  }
  float3 GetObjectRayOrigin() {
    return __builtin_HitObject_GetObjectRayOrigin(handle);
  }
  float3 GetObjectRayDirection() {
    return __builtin_HitObject_GetObjectRayDirection(handle);
  }
  float3x4 GetObjectToWorld3x4() {
    return __builtin_HitObject_GetObjectToWorld3x4(handle);
  }
  float4x3 GetObjectToWorld4x3() {
    return __builtin_HitObject_GetObjectToWorld4x3(handle);
  }
  float3x4 GetWorldToObject3x4() {
    return __builtin_HitObject_GetWorldToObject3x4(handle);
  }
  float4x3 GetWorldToObject4x3() {
    return __builtin_HitObject_GetWorldToObject4x3(handle);
  }
  uint GetInstanceIndex() {
    return __builtin_HitObject_GetInstanceIndex(handle);
  }
  uint GetInstanceID() { return __builtin_HitObject_GetInstanceID(handle); }
  uint GetGeometryIndex() {
    return __builtin_HitObject_GetGeometryIndex(handle);
  }
  uint GetPrimitiveIndex() {
    return __builtin_HitObject_GetPrimitiveIndex(handle);
  }
  uint GetHitKind() { return __builtin_HitObject_GetHitKind(handle); }
  template <typename attr_t> attr_t GetAttributes() {
    // Any builtin that HitObject::GetAttribute() lowers to needs to have attr_t
    // in its signature or Clang cannot infer attr_t for the builtins return
    // type. This means the following does not work:
    //
    //     return __builtin_HitObject_GetAttributes();
    //
    // One way to workaround is to make attrs an 'out' parameter of the builtin.
    attr_t attrs;
    __builtin_HitObject_GetAttributes(handle, attrs);
    return attrs;
  }
  uint GetShaderTableIndex() {
    return __builtin_HitObject_GetShaderTableIndex(handle);
  }
  void SetShaderTableIndex(in uint sbtRecordIdx) {
    __builtin_HitObject_SetShaderTableIndex(handle, sbtRecordIdx);
  }
  uint LoadLocalRootTableConstant(in uint RootConstantOffsetInBytes) {
    return __builtin_HitObject_LoadLocalRootTableConstant(
        handle, RootConstantOffsetInBytes);
  }
};
static void ReorderThread(in HitObject ho) {
  __builtin_HitObject_ReorderThread(ho.handle);
}
static void ReorderThread(in uint CoherenceHint, in uint NumCoherentsBitsFromLSB) {
  __builtin_HitObject_ReorderThread(ho.handle, CoherenceHint, NumCoherentsBitsFromLSB);
}
static void ReorderThread(in HitObject ho, in uint CoherenceHint, in uint NumCoherentsBitsFromLSB) {
  __builtin_HitObject_ReorderThread(ho.handle, CoherenceHint, NumCoherentsBitsFromLSB);
}
