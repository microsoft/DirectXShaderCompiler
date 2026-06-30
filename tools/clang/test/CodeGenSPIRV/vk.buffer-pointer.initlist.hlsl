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
    BufferAccessor<float> accessor = BufferAccessor<float>(buffer);

// CHECK: OpLoad %float [[LD]] Aligned 4
    buffer.Get() = accessor.ptr.Get();
}
