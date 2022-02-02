// RUN: %dxc -T ps_6_0 -E main -enable-templates

// CHECK: OpCapability PhysicalStorageBufferAddresses
// CHECK: OpExtension "SPV_KHR_physical_storage_buffer"
// CHECK: OpMemoryModel PhysicalStorageBuffer64 GLSL450

// CHECK: OpTypePointer PhysicalStorageBuffer %uint
// CHECK: OpTypePointer PhysicalStorageBuffer %float

uint64_t Address;
float4 main() : SV_Target0 {
  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_uint [[addr]]
  // CHECK-NEXT: [[ac:%\d+]] = OpAccessChain %_ptr_PhysicalStorageBuffer_uint [[buf]]
  // CHECK-NEXT: OpLoad %uint [[ac]] Aligned 4
  uint Value = vk::RawBufferLoad(Address);

  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_float [[addr]]
  // CHECK-NEXT: [[ac:%\d+]] = OpAccessChain %_ptr_PhysicalStorageBuffer_float [[buf]]
  // CHECK-NEXT: [[load:%\d+]] = OpLoad %float [[ac]] Aligned 4
  // CHECK-NEXT: OpStore %foo [[load]]
  float foo;
  vk::RawBufferLoadToParam(foo, Address);

  // CHECK:      [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_uint
  // CHECK-NEXT: [[ac:%\d+]] = OpAccessChain %_ptr_PhysicalStorageBuffer_uint [[buf]]
  // CHECK-NEXT: [[load:%\d+]] = OpLoad %uint [[ac]] Aligned 4
  // CHECK-NEXT: OpINotEqual %bool [[load]] %uint_0
  bool bar = vk::LoadRawBuffer<bool>(Address);

  return float4(asfloat(Value), foo, bar, 0);
}
