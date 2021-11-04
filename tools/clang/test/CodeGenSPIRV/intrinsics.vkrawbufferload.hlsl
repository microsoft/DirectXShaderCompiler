// RUN: %dxc -T ps_6_0 -E main

// CHECK: OpCapability PhysicalStorageBufferAddresses
// CHECK: OpExtension "SPV_KHR_physical_storage_buffer"
// CHECK: OpMemoryModel PhysicalStorageBuffer64 GLSL450

uint64_t Address;
float4 main() : SV_Target0 {
  // CHECK: OpTypePointer PhysicalStorageBuffer %uint
  // CHECK: [[addr:%\d+]] = OpLoad %ulong {{%\d+}}
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_uint [[addr]]
  // CHECK-NEXT: [[ac:%\d+]] = OpAccessChain %_ptr_PhysicalStorageBuffer_uint [[buf]]
  // CHECK-NEXT: OpLoad %uint [[ac]] Aligned 4
  uint Value = vk::RawBufferLoad(Address);
  return asfloat(Value);
}
