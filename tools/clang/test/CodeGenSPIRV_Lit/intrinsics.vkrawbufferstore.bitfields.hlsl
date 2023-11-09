// RUN: %dxc -T cs_6_0 -E main -HV 2021

// CHECK: OpCapability PhysicalStorageBufferAddresses
// CHECK: OpExtension "SPV_KHR_physical_storage_buffer"
// CHECK: OpMemoryModel PhysicalStorageBuffer64 GLSL450
// CHECK-NOT: OpMemberDecorate %S 0 Offset 0
// CHECK-NOT: OpMemberDecorate %S 1 Offset 4
// CHECK-NOT: OpMemberDecorate %S 2 Offset 8
// CHECK-NOT: OpMemberDecorate %S 3 Offset 12
// CHECK: OpMemberDecorate %S_0 0 Offset 0
// CHECK: OpMemberDecorate %S_0 1 Offset 4
// CHECK: OpMemberDecorate %S_0 2 Offset 8
// CHECK-NOT: OpMemberDecorate %S_0 3 Offset 12

// CHECK: %S = OpTypeStruct %uint %uint %uint
// CHECK: %_ptr_Function_S = OpTypePointer Function %S
// CHECK: %S_0 = OpTypeStruct %uint %uint %uint
// CHECK: %_ptr_PhysicalStorageBuffer_S_0 = OpTypePointer PhysicalStorageBuffer %S_0


struct S {
  uint f1;
  uint f2 : 1;
  uint f3 : 3;
  uint f4;
};

uint64_t Address;

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  // CHECK: %tmp = OpVariable %_ptr_Function_S Function
  S tmp;

  // CHECK: [[tmp:%\d+]] = OpAccessChain %_ptr_Function_uint %tmp %int_0
  // CHECK: OpStore [[tmp]] %uint_2
  tmp.f1 = 2;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %tmp %int_1
  // CHECK: [[tmp:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK: [[tmp:%\d+]] = OpBitFieldInsert %uint [[tmp]] %uint_1 %uint_0 %uint_1
  // CHECK: OpStore [[ptr]] [[tmp]]
  tmp.f2 = 1;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %tmp %int_1
  // CHECK: [[tmp:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK: [[tmp:%\d+]] = OpBitFieldInsert %uint [[tmp]] %uint_0 %uint_1 %uint_3
  // CHECK: OpStore [[ptr]] [[tmp]]
  tmp.f3 = 0;

  // CHECK: [[tmp:%\d+]] = OpAccessChain %_ptr_Function_uint %tmp %int_2
  // CHECK: OpStore [[tmp]] %uint_3
  tmp.f4 = 3;
  vk::RawBufferStore<S>(Address, tmp);
}

