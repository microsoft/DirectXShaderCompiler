// RUN: %dxc -T cs_6_0 -E main

// CHECK: OpCapability PhysicalStorageBufferAddresses
// CHECK: OpExtension "SPV_KHR_physical_storage_buffer"
// CHECK: OpMemoryModel PhysicalStorageBuffer64 GLSL450
// CHECK-NOT: OpMemberDecorate %XYZW 0 Offset 0
// CHECK-NOT: OpMemberDecorate %XYZW 1 Offset 4
// CHECK-NOT: OpMemberDecorate %XYZW 2 Offset 8
// CHECK-NOT: OpMemberDecorate %XYZW 3 Offset 12
// CHECK: OpMemberDecorate %XYZW_0 0 Offset 0
// CHECK: OpMemberDecorate %XYZW_0 1 Offset 4
// CHECK: OpMemberDecorate %XYZW_0 2 Offset 8
// CHECK: OpMemberDecorate %XYZW_0 3 Offset 12
// CHECK: %XYZW = OpTypeStruct %int %int %int %int
// CHECK: %_ptr_Function_XYZW = OpTypePointer Function %XYZW
// CHECK: %XYZW_0 = OpTypeStruct %int %int %int %int
// CHECK: %_ptr_PhysicalStorageBuffer_XYZW_0 = OpTypePointer PhysicalStorageBuffer %XYZW_0

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
  // CHECK-NEXT: [[buf:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_XYZW_0 [[addr]]
  // CHECK-NEXT: [[member1:%\d+]] = OpCompositeExtract %int [[xyzwval]] 0
  // CHECK-NEXT: [[member2:%\d+]] = OpCompositeExtract %int [[xyzwval]] 1
  // CHECK-NEXT: [[member3:%\d+]] = OpCompositeExtract %int [[xyzwval]] 2
  // CHECK-NEXT: [[member4:%\d+]] = OpCompositeExtract %int [[xyzwval]] 3
  // CHECK-NEXT: [[p_xyzwval:%\d+]] = OpCompositeConstruct %XYZW_0 [[member1]] [[member2]] [[member3]] [[member4]]
  // CHECK-NEXT: OpStore [[buf]] [[p_xyzwval]] Aligned 4
  XYZW xyzw;
  xyzw.x = 78;
  xyzw.y = 65;
  xyzw.z = 84;
  xyzw.w = 69;
  vk::RawBufferStore<XYZW>(Address, xyzw);
}
