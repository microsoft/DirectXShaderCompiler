// RUN: %dxc -T ps_6_0 -E main -HV 2021

// CHECK: OpCapability PhysicalStorageBufferAddresses
// CHECK: OpExtension "SPV_KHR_physical_storage_buffer"
// CHECK: OpMemoryModel PhysicalStorageBuffer64 GLSL450

struct S {
  uint f1;
  uint f2 : 1;
  uint f3 : 1;
  uint f4;
};

uint64_t Address;

// CHECK: OpMemberDecorate %S_0 0 Offset 0
// CHECK: OpMemberDecorate %S_0 1 Offset 4
// CHECK: OpMemberDecorate %S_0 2 Offset 8
// CHECK: %S = OpTypeStruct %uint %uint %uint
// CHECK: [[ptr_f_S:%\w+]] = OpTypePointer Function %S
// CHECK: %S_0 = OpTypeStruct %uint %uint %uint
// CHECK: [[ptr_p_S:%\w+]] = OpTypePointer PhysicalStorageBuffer %S_0

void main() : B {
// CHECK: [[tmp_S:%\w+]] = OpVariable [[ptr_f_S]] Function
// CHECK: [[value:%\d+]] = OpAccessChain %_ptr_Uniform_ulong %_Globals %int_0
// CHECK: [[value:%\d+]] = OpLoad %ulong [[value]]
// CHECK: [[value:%\d+]] = OpBitcast [[ptr_p_S]] [[value]]
// CHECK: [[value:%\d+]] = OpLoad %S_0 [[value]] Aligned 4
// CHECK: [[member0:%\d+]] = OpCompositeExtract %uint [[value]] 0
// CHECK: [[member1:%\d+]] = OpCompositeExtract %uint [[value]] 1
// CHECK: [[member2:%\d+]] = OpCompositeExtract %uint [[value]] 2
// CHECK: [[value:%\d+]] = OpCompositeConstruct %S [[member0]] [[member1]] [[member2]]
// CHECK: OpStore [[tmp_S]] [[value]]
// CHECK: [[value:%\d+]] = OpAccessChain %_ptr_Function_uint [[tmp_S]] %int_1
// CHECK: [[value:%\d+]] = OpLoad %uint [[value]]
// CHECK: [[value:%\d+]] = OpBitFieldUExtract %uint [[value]] %uint_1 %uint_1
// CHECK: OpStore %tmp [[value]]
  uint tmp = vk::RawBufferLoad<S>(Address).f3;
}

