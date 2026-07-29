// RUN: %dxc -spirv -E main -T cs_6_7 %s | FileCheck %s

struct Foo {
  uint a : 16;
  uint b : 16;
};

[[vk::push_constant]] struct Pc {
  vk::BufferPointer<Foo> ptr;
} pc;

[numthreads(1, 1, 1)]
void main() {
  pc.ptr.Get().a = 123;
}

// CHECK: [[FIELD:%[0-9]+]] = OpAccessChain %_ptr_PhysicalStorageBuffer_uint {{%[0-9]+}} %int_0
// CHECK: [[OLD:%[0-9]+]] = OpLoad %uint [[FIELD]] Aligned 4
// CHECK: [[NEW:%[0-9]+]] = OpBitFieldInsert %uint [[OLD]] %uint_123 %uint_0 %uint_16
// CHECK: OpStore [[FIELD]] [[NEW]] Aligned 4
