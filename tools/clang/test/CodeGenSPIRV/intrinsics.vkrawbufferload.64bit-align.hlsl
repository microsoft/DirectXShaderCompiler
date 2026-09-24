// RUN: %dxc -T cs_6_2 -E main -enable-16bit-types -fcgl  %s -spirv | FileCheck %s

// Test that vk::RawBufferLoad/Store with 64-bit types uses alignment 8
// when no explicit alignment is specified.

struct StructWith64BitMember {
    uint64_t member;
};

struct StructWithDoubleMember {
    double member;
};

// Mixes of 16-bit, 32-bit and 64-bit scalar members. The required alignment is
// the largest scalar alignment within the structure.
struct StructWithMixedScalars {
    uint16_t a;
    uint b;
    uint64_t c;
};

// A 16-bit/32-bit mix without any 64-bit member: the largest scalar alignment
// is 4, so alignment 4 is used.
struct StructWith16And32BitScalars {
    uint16_t a;
    float b;
};

// A structure with only 16-bit members. The largest scalar alignment is 2, so
// alignment 2 is used: nothing in the Vulkan spec requires a larger minimum.
struct StructWithOnly16BitScalars {
    uint16_t a;
    half b;
};

// A structure containing vectors of 16-bit, 32-bit and 64-bit elements. The
// 64-bit vector forces alignment 8.
struct StructWithMixedVectors {
    half2 a;
    float3 b;
    double2 c;
};

// A structure containing matrices. The 64-bit matrix forces alignment 8.
struct StructWithMixedMatrices {
    float2x2 a;
    double2x2 b;
};

uint64_t Address;

[numthreads(1, 1, 1)]
void main() {
  // CHECK:      [[addr:%[0-9]+]] = OpLoad %ulong
  // CHECK:      [[buf:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_ulong
  // CHECK-NEXT: [[load:%[0-9]+]] = OpLoad %ulong [[buf]] Aligned 8
  uint64_t scalar = vk::RawBufferLoad<uint64_t>(Address);

  // CHECK:      [[buf_1:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_double
  // CHECK-NEXT: [[load_1:%[0-9]+]] = OpLoad %double [[buf_1]] Aligned 8
  double dbl = vk::RawBufferLoad<double>(Address);

  // CHECK:      [[buf_2:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWith64BitMember
  // CHECK-NEXT: [[load_2:%[0-9]+]] = OpLoad %StructWith64BitMember{{[^ ]*}} [[buf_2]] Aligned 8
  StructWith64BitMember s1 = vk::RawBufferLoad<StructWith64BitMember>(Address);

  // CHECK:      [[buf_3:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWithDoubleMember
  // CHECK-NEXT: [[load_3:%[0-9]+]] = OpLoad %StructWithDoubleMember{{[^ ]*}} [[buf_3]] Aligned 8
  StructWithDoubleMember s2 = vk::RawBufferLoad<StructWithDoubleMember>(Address);

  // Stores should also use alignment 8 for 64-bit types.
  // CHECK:      [[buf_4:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_ulong
  // CHECK-NEXT: OpStore [[buf_4]] {{%[0-9]+}} Aligned 8
  vk::RawBufferStore<uint64_t>(Address, scalar);

  // CHECK:      [[buf_5:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_double
  // CHECK-NEXT: OpStore [[buf_5]] {{%[0-9]+}} Aligned 8
  vk::RawBufferStore<double>(Address, dbl);

  // CHECK:      [[buf_6:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWith64BitMember
  // CHECK:      OpStore [[buf_6]] {{%[0-9]+}} Aligned 8
  vk::RawBufferStore<StructWith64BitMember>(Address, s1);

  // CHECK:      [[buf_7:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWithDoubleMember
  // CHECK:      OpStore [[buf_7]] {{%[0-9]+}} Aligned 8
  vk::RawBufferStore<StructWithDoubleMember>(Address, s2);

  // A structure mixing 16-bit, 32-bit and 64-bit scalars uses alignment 8
  // because of its 64-bit member.
  // CHECK:      [[buf_8:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWithMixedScalars
  // CHECK-NEXT: [[load_8:%[0-9]+]] = OpLoad %StructWithMixedScalars{{[^ ]*}} [[buf_8]] Aligned 8
  StructWithMixedScalars s3 = vk::RawBufferLoad<StructWithMixedScalars>(Address);

  // A structure mixing only 16-bit and 32-bit scalars uses alignment 4.
  // CHECK:      [[buf_9:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWith16And32BitScalars
  // CHECK-NEXT: [[load_9:%[0-9]+]] = OpLoad %StructWith16And32BitScalars{{[^ ]*}} [[buf_9]] Aligned 4
  StructWith16And32BitScalars s4 = vk::RawBufferLoad<StructWith16And32BitScalars>(Address);

  // A structure with only 16-bit scalars has scalar alignment 2, which is the
  // minimum required by the Vulkan spec for such an access.
  // CHECK:      [[buf_10:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWithOnly16BitScalars
  // CHECK-NEXT: [[load_10:%[0-9]+]] = OpLoad %StructWithOnly16BitScalars{{[^ ]*}} [[buf_10]] Aligned 2
  StructWithOnly16BitScalars s5 = vk::RawBufferLoad<StructWithOnly16BitScalars>(Address);

  // A structure containing 16-bit, 32-bit and 64-bit vectors uses alignment 8
  // because of its 64-bit vector member.
  // CHECK:      [[buf_11:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWithMixedVectors
  // CHECK-NEXT: [[load_11:%[0-9]+]] = OpLoad %StructWithMixedVectors{{[^ ]*}} [[buf_11]] Aligned 8
  StructWithMixedVectors s6 = vk::RawBufferLoad<StructWithMixedVectors>(Address);

  // A structure containing 32-bit and 64-bit matrices uses alignment 8 because
  // of its 64-bit matrix member.
  // CHECK:      [[buf_12:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWithMixedMatrices
  // CHECK-NEXT: [[load_12:%[0-9]+]] = OpLoad %StructWithMixedMatrices{{[^ ]*}} [[buf_12]] Aligned 8
  StructWithMixedMatrices s7 = vk::RawBufferLoad<StructWithMixedMatrices>(Address);

  // Stores use the same computed alignments as loads.
  // CHECK:      [[buf_13:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWithMixedScalars
  // CHECK:      OpStore [[buf_13]] {{%[0-9]+}} Aligned 8
  vk::RawBufferStore<StructWithMixedScalars>(Address, s3);

  // CHECK:      [[buf_14:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWith16And32BitScalars
  // CHECK:      OpStore [[buf_14]] {{%[0-9]+}} Aligned 4
  vk::RawBufferStore<StructWith16And32BitScalars>(Address, s4);

  // CHECK:      [[buf_15:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWithOnly16BitScalars
  // CHECK:      OpStore [[buf_15]] {{%[0-9]+}} Aligned 2
  vk::RawBufferStore<StructWithOnly16BitScalars>(Address, s5);

  // CHECK:      [[buf_16:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWithMixedVectors
  // CHECK:      OpStore [[buf_16]] {{%[0-9]+}} Aligned 8
  vk::RawBufferStore<StructWithMixedVectors>(Address, s6);

  // CHECK:      [[buf_17:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_StructWithMixedMatrices
  // CHECK:      OpStore [[buf_17]] {{%[0-9]+}} Aligned 8
  vk::RawBufferStore<StructWithMixedMatrices>(Address, s7);

  // A 16-bit scalar requires only 2-byte alignment.
  // CHECK:      [[buf_18:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_ushort
  // CHECK-NEXT: [[load_18:%[0-9]+]] = OpLoad %ushort [[buf_18]] Aligned 2
  uint16_t u16 = vk::RawBufferLoad<uint16_t>(Address);

  // CHECK:      [[buf_19:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_half
  // CHECK-NEXT: [[load_19:%[0-9]+]] = OpLoad %half [[buf_19]] Aligned 2
  half h = vk::RawBufferLoad<half>(Address);

  // CHECK:      [[buf_20:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_ushort
  // CHECK:      OpStore [[buf_20]] {{%[0-9]+}} Aligned 2
  vk::RawBufferStore<uint16_t>(Address, u16);

  // CHECK:      [[buf_21:%[0-9]+]] = OpBitcast %_ptr_PhysicalStorageBuffer_half
  // CHECK:      OpStore [[buf_21]] {{%[0-9]+}} Aligned 2
  vk::RawBufferStore<half>(Address, h);
}
