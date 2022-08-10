// RUN: %dxc -T cs_6_0 -E main

// CHECK: OpCapability PhysicalStorageBufferAddresses
// CHECK: OpExtension "SPV_KHR_physical_storage_buffer"
// CHECK: OpMemoryModel PhysicalStorageBuffer64 GLSL450

struct XYZW {
  int x;
  int y;
  int z;
  int w;
};

uint64_t Address;
[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[xval:%\d+]] = OpLoad %float %x
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_float [[addr]]
  // CHECK-NEXT: OpStore [[buf]] [[xval]] Aligned 4
  float x = 78.f;
  vk::RawBufferStore<float>(Address, x);

  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[yval:%\d+]] = OpLoad %double %y
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_double [[addr]]
  // CHECK-NEXT: OpStore [[buf]] [[yval]] Aligned 8
  double y = 65.0;
  vk::RawBufferStore<double>(Address, y, 8);

  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[zval:%\d+]] = OpINotEqual %bool
  // CHECK-NEXT: OpStore
  // CHECK-NEXT: [[tmp:%\d+]] = OpAccessChain
  // CHECK-NEXT: [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[zval:%\d+]] = OpLoad %bool %z
  // CHECK-NEXT: [[zsel:%\d+]] = OpSelect %uint [[zval]]
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_uint
  // CHECK-NEXT: OpStore [[buf]] [[zsel]] Aligned 4
  // Note, using address here for bool z to prevent bool from being optimized away
  bool z = Address;
  vk::RawBufferStore<bool>(Address, z);

  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[wval:%\d+]] = OpLoad %v2float %w
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_v2float [[addr]]
  // CHECK-NEXT: OpStore [[buf]] [[wval]] Aligned 8
  float2 w = float2(84.f, 69.f);
  vk::RawBufferStore<float2>(Address, w, 8);

  // CHECK:      [[addr:%\d+]] = OpLoad %ulong
  // CHECK-NEXT: [[xyzwval:%\d+]] = OpLoad %XYZW %xyzw
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_XYZW [[addr]]
  // CHECK-NEXT: OpStore [[buf]] [[xyzwval]] Aligned 4
  XYZW xyzw;
  xyzw.x = 78;
  xyzw.y = 65;
  xyzw.z = 84;
  xyzw.w = 69;
  vk::RawBufferStore<XYZW>(Address, xyzw);
}
