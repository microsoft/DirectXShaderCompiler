// RUN: %dxc -T ps_6_0 -E main -fspv-reserve-resource-array-bindings -Vd -fcgl  %s -spirv | FileCheck %s

// Regression test: globals that are not resources (here, one with an inline
// SPIR-V storage class) are also visited when reserving bindings. They must
// fall back to a single binding instead of tripping the resource-only
// assertion in getNumBindingsUsedByResourceType.

[[vk::ext_extension("SPV_KHR_ray_tracing")]]
[[vk::ext_capability(/* RayTracingKHR */ 4479)]]
[[vk::ext_storage_class(/*RayPayloadKHR*/5338)]]
float4 payload;
// CHECK-DAG: [[ptr_payload_v4:%[a-zA-Z0-9_]+]] = OpTypePointer RayPayloadKHR %v4float
// CHECK-DAG: %payload = OpVariable [[ptr_payload_v4]] RayPayloadKHR

float4 main() : SV_Target0 {
  return payload;
}
