// RUN: %dxc -T ps_6_0 -E main -enable-templates

// CHECK: OpCapability PhysicalStorageBufferAddresses
// CHECK: OpExtension "SPV_KHR_physical_storage_buffer"
// CHECK: OpMemoryModel PhysicalStorageBuffer64 GLSL450

uint64_t Address;
float4 main() : SV_Target0 {
  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_float [[addr]]
  // CHECK-NEXT: [[ac:%\d+]] = OpAccessChain %_ptr_PhysicalStorageBuffer_float [[buf]]
  // CHECK-NEXT: [[load:%\d+]] = OpLoad %float [[ac]] Aligned 4
  // CHECK-NEXT: OpStore %x [[load]]
  float x;
  vk::RawBufferLoadInto(x, Address);

  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_double [[addr]]
  // CHECK-NEXT: [[ac:%\d+]] = OpAccessChain %_ptr_PhysicalStorageBuffer_double [[buf]]
  // CHECK-NEXT: [[load:%\d+]] = OpLoad %double [[ac]] Aligned 8
  // CHECK-NEXT: OpStore %y [[load]]
  double y;
  vk::RawBufferLoadInto(y, Address, 8);

  // CHECK:      [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_uint
  // CHECK-NEXT: [[ac:%\d+]] = OpAccessChain %_ptr_PhysicalStorageBuffer_uint [[buf]]
  // CHECK-NEXT: [[load:%\d+]] = OpLoad %uint [[ac]] Aligned 4
  // CHECK-NEXT: OpINotEqual %bool [[load]] %uint_0
  bool z = vk::RawBufferLoad<bool, 4>(Address);

  // CHECK:      [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_v2float
  // CHECK-NEXT: [[ac:%\d+]] = OpAccessChain %_ptr_PhysicalStorageBuffer_v2float [[buf]]
  // CHECK-NEXT: [[load:%\d+]] = OpLoad %v2float [[ac]] Aligned 8
  float2 w = vk::RawBufferLoad<float2, 8>(Address);

  return float4(w.x, x, y, z);
}
