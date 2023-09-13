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

// CHECK: %temp_var_S = OpVariable %_ptr_Function_S Function
// CHECK: %temp_var_S_0 = OpVariable %_ptr_Function_S Function
// CHECK: %temp_var_S_1 = OpVariable %_ptr_Function_S Function
// CHECK: %temp_var_S_2 = OpVariable %_ptr_Function_S Function

struct S {
  uint f1;
  uint f2 : 1;
  uint f3 : 1;
  uint f4;
};

uint64_t Address;

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {


  {
    // CHECK: [[tmp:%\d+]] = OpAccessChain %_ptr_Uniform_ulong %_Globals %int_0
    // CHECK: [[tmp:%\d+]] = OpLoad %ulong [[tmp]]
    // CHECK: [[ptr:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_S_0 [[tmp]]
    // CHECK: [[tmp:%\d+]] = OpLoad %S_0 [[ptr]] Aligned 4
    // CHECK: [[member0:%\d+]] = OpCompositeExtract %uint [[tmp]] 0
    // CHECK: [[member1:%\d+]] = OpCompositeExtract %uint [[tmp]] 1
    // CHECK: [[member2:%\d+]] = OpCompositeExtract %uint [[tmp]] 2
    // CHECK: [[tmp:%\d+]] = OpCompositeConstruct %S [[member0]] [[member1]] [[member2]]
    // CHECK: OpStore %temp_var_S [[tmp]]
    // CHECK: [[tmp:%\d+]] = OpAccessChain %_ptr_Function_uint %temp_var_S %int_0
    // CHECK: [[tmp:%\d+]] = OpLoad %uint [[tmp]]
    // CHECK: OpStore %tmp1 [[tmp]]
    uint tmp1 = vk::RawBufferLoad<S>(Address).f1;
  }

  {
    // CHECK: [[tmp:%\d+]] = OpAccessChain %_ptr_Uniform_ulong %_Globals %int_0
    // CHECK: [[tmp:%\d+]] = OpLoad %ulong [[tmp]]
    // CHECK: [[ptr:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_S_0 [[tmp]]
    // CHECK: [[tmp:%\d+]] = OpLoad %S_0 [[ptr]] Aligned 4
    // CHECK: [[member0:%\d+]] = OpCompositeExtract %uint [[tmp]] 0
    // CHECK: [[member1:%\d+]] = OpCompositeExtract %uint [[tmp]] 1
    // CHECK: [[member2:%\d+]] = OpCompositeExtract %uint [[tmp]] 2
    // CHECK: [[tmp:%\d+]] = OpCompositeConstruct %S [[member0]] [[member1]] [[member2]]
    // CHECK: OpStore %temp_var_S_0 [[tmp]]
    // CHECK: [[tmp:%\d+]] = OpAccessChain %_ptr_Function_uint %temp_var_S_0 %int_1
    // CHECK: [[tmp:%\d+]] = OpLoad %uint [[tmp]]
    // CHECK: [[tmp:%\d+]] = OpBitFieldUExtract %uint [[tmp]] %uint_0 %uint_1
    // CHECK: OpStore %tmp2 [[tmp]]
    uint tmp2 = vk::RawBufferLoad<S>(Address).f2;
  }

  {
    // CHECK: [[tmp:%\d+]] = OpAccessChain %_ptr_Uniform_ulong %_Globals %int_0
    // CHECK: [[tmp:%\d+]] = OpLoad %ulong [[tmp]]
    // CHECK: [[ptr:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_S_0 [[tmp]]
    // CHECK: [[tmp:%\d+]] = OpLoad %S_0 [[ptr]] Aligned 4
    // CHECK: [[member0:%\d+]] = OpCompositeExtract %uint [[tmp]] 0
    // CHECK: [[member1:%\d+]] = OpCompositeExtract %uint [[tmp]] 1
    // CHECK: [[member2:%\d+]] = OpCompositeExtract %uint [[tmp]] 2
    // CHECK: [[tmp:%\d+]] = OpCompositeConstruct %S [[member0]] [[member1]] [[member2]]
    // CHECK: OpStore %temp_var_S_1 [[tmp]]
    // CHECK: [[tmp:%\d+]] = OpAccessChain %_ptr_Function_uint %temp_var_S_1 %int_1
    // CHECK: [[tmp:%\d+]] = OpLoad %uint [[tmp]]
    // CHECK: [[tmp:%\d+]] = OpBitFieldUExtract %uint [[tmp]] %uint_1 %uint_1
    // CHECK: OpStore %tmp3 [[tmp]]
    uint tmp3 = vk::RawBufferLoad<S>(Address).f3;
  }

  {
    // CHECK: [[tmp:%\d+]] = OpAccessChain %_ptr_Uniform_ulong %_Globals %int_0
    // CHECK: [[tmp:%\d+]] = OpLoad %ulong [[tmp]]
    // CHECK: [[ptr:%\d+]] = OpBitcast %_ptr_PhysicalStorageBuffer_S_0 [[tmp]]
    // CHECK: [[tmp:%\d+]] = OpLoad %S_0 [[ptr]] Aligned 4
    // CHECK: [[member0:%\d+]] = OpCompositeExtract %uint [[tmp]] 0
    // CHECK: [[member1:%\d+]] = OpCompositeExtract %uint [[tmp]] 1
    // CHECK: [[member2:%\d+]] = OpCompositeExtract %uint [[tmp]] 2
    // CHECK: [[tmp:%\d+]] = OpCompositeConstruct %S [[member0]] [[member1]] [[member2]]
    // CHECK: OpStore %temp_var_S_2 [[tmp]]
    // CHECK: [[tmp:%\d+]] = OpAccessChain %_ptr_Function_uint %temp_var_S_2 %int_2
    // CHECK: [[tmp:%\d+]] = OpLoad %uint [[tmp]]
    // CHECK: OpStore %tmp4 [[tmp]]
    uint tmp4 = vk::RawBufferLoad<S>(Address).f4;
  }
}

