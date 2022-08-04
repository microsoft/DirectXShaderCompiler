// RUN: %dxc -T cs_6_0 -E main

// CHECK: OpCapability PhysicalStorageBufferAddresses
// CHECK: OpExtension "SPV_KHR_physical_storage_buffer"
// CHECK: OpMemoryModel PhysicalStorageBuffer64 GLSL450

uint64_t Address;
[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_float [[addr]]
  // CHECK-NEXT: OpStore [[buf]] %float Aligned 4
  float x = 42.f;
  vk::RawBufferStore<float>(Address, x);

  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_double [[addr]]
  // CHECK-NEXT: OpStore [[buf]] %double Aligned 8
  double y = 42.0;
  vk::RawBufferStore<double>(Address, y, 8);

  // CHECK:      [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_uint
  // CHECK-NEXT: OpStore [[buf]] %bool Aligned 4
  bool z = true;
  vk::RawBufferStore<bool>(Address, z);

  // CHECK:      [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_v2float
  // CHECK-NEXT: OpStore [[buf]] %v2float Aligned 8
  float2 w = float2(42.f, 1337.f);
  vk::RawBufferStore<float2>(Address, w, 8);

}
