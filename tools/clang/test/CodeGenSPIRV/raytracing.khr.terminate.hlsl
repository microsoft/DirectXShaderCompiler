// Run: %dxc "./build.x64/Release/bin/dxc.exe" -T lib_6_3 -fspv-target-env=vulkan1.2 -spirv tools\clang\test\CodeGenSPIRV\raytracing.khr.library.hlsl
// CHECK:  OpCapability RayTracingKHR
// CHECK:  OpExtension "SPV_KHR_ray_tracing"

// CHECK:  OpTypePointer IncomingRayPayloadNV %Payload
struct Payload
{
  float4 color;
};
// CHECK:  OpTypePointer HitAttributeNV %Attribute
struct Attribute
{
  float2 bary;
};

[shader("anyhit")]
void MyAHitMain(inout Payload MyPayload, in Attribute MyAttr) {

// CHECK:  OpLoad %uint [[l]]
  uint _16 = HitKind();

  if (_16 == 1U) {
// CHECK:  OpIgnoreIntersectionKHR
// CHECK-NEXT: OpLabel
    IgnoreHit();
  } else {
// CHECK:  OpTerminateRayKHR
// CHECK-NEXT: OpLabel
    AcceptHitAndEndSearch();
  }
}


[shader("anyhit")]
void MyAHitMain2(inout Payload MyPayload, in Attribute MyAttr) {
// CHECK:  OpTerminateRayKHR
// CHECK-NEXT: OpFunctionEnd
    AcceptHitAndEndSearch();
}
