// RUN: %dxc -spirv -E main -T cs_6_7 %s -fspv-target-env=vulkan1.3 | FileCheck %s

template <typename T>
struct BufferAccessor {
    vk::BufferPointer<T> ptr;
};

cbuffer Test {
    vk::BufferPointer<float> buffer;
};

[numthreads(256, 1, 1)]
void main(in uint3 threadId : SV_DispatchThreadID) {
// CHECK: [[LD:%[_0-9A-Za-z]*]] = OpLoad %_ptr_PhysicalStorageBuffer_float
// CHECK: [[CC:%[_0-9A-Za-z]*]] = OpCompositeConstruct {{%[_0-9A-Za-z]*}} [[LD]]
    BufferAccessor<float> accessor = BufferAccessor<float>(buffer);

// CHECK: [[COPY:%[_0-9A-Za-z]*]] = OpCopyLogical {{%[_0-9A-Za-z]*}} [[CC]]
// CHECK: [[PTR:%[_0-9A-Za-z]*]] = OpCompositeExtract %_ptr_PhysicalStorageBuffer_float [[COPY]] 0
// CHECK: OpLoad %float [[PTR]] Aligned 4  
    buffer.Get() = accessor.ptr.Get();
}
