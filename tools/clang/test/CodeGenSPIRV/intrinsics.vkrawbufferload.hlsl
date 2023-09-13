// RUN: %dxc -T ps_6_0 -E main

// CHECK: OpCapability PhysicalStorageBufferAddresses
// CHECK: OpExtension "SPV_KHR_physical_storage_buffer"
// CHECK: OpMemoryModel PhysicalStorageBuffer64 GLSL450

uint64_t Address;
float4 main() : SV_Target0 {
  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_float [[addr]]
  // CHECK-NEXT: [[load:%\d+]] = OpLoad %float [[buf]] Aligned 4
  // CHECK-NEXT: OpStore %x [[load]]
  float x = vk::RawBufferLoad<float>(Address);

  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_double [[addr]]
  // CHECK-NEXT: [[load:%\d+]] = OpLoad %double [[buf]] Aligned 8
  // CHECK-NEXT: OpStore %y [[load]]
  double y = vk::RawBufferLoad<double>(Address, 8);

  // CHECK:      [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_uint
  // CHECK-NEXT: [[load:%\d+]] = OpLoad %uint [[buf]] Aligned 4
  // CHECK-NEXT: [[z:%\d+]] = OpINotEqual %bool [[load]] %uint_0
  // CHECK-NEXT: OpStore %z [[z]]
  bool z = vk::RawBufferLoad<bool>(Address, 4);

  // CHECK:      [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_v2float
  // CHECK-NEXT: [[load:%\d+]] = OpLoad %v2float [[buf]] Aligned 8
  // CHECK-NEXT: OpStore %w [[load]]
  float2 w = vk::RawBufferLoad<float2>(Address, 8);

  // CHECK:      [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_uint
  // CHECK-NEXT: [[load:%\d+]] = OpLoad %uint [[buf]] Aligned 4
  // CHECK-NEXT: OpStore %v [[load]]
  uint v = vk::RawBufferLoad(Address);

  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_float [[addr]]
  // CHECK-NEXT: [[load:%\d+]] = OpLoad %float [[buf]] Aligned 4
  // CHECK-NEXT: OpStore %u [[load]]
  const uint alignment = 4;
  float u = vk::RawBufferLoad<float>(Address, alignment);

  return float4(w.x, x, y, z);
}
