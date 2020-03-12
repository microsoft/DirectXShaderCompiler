// Run: %dxc -T cs_6_0 -E main -fspv-target-env=vulkan1.2

// We cannot use BufferBlock decoration for SPIR-V 1.4 or above.
// Instead, we must use Block decorated StorageBuffer Storage Class.

//CHECK: OpDecorate %type_ByteAddressBuffer Block
//CHECK: OpDecorate %type_RWByteAddressBuffer Block
//CHECK: OpDecorate %type_TextureBuffer_S Block
//CHECK: OpDecorate %type_StructuredBuffer_v3uint Block

//CHECK: %_ptr_StorageBuffer_type_ByteAddressBuffer = OpTypePointer StorageBuffer %type_ByteAddressBuffer
//CHECK: %_ptr_StorageBuffer_type_RWByteAddressBuffer = OpTypePointer StorageBuffer %type_RWByteAddressBuffer
//CHECK: %_ptr_StorageBuffer_type_TextureBuffer_S = OpTypePointer StorageBuffer %type_TextureBuffer_S
//CHECK: %_ptr_StorageBuffer_type_StructuredBuffer_v3uint = OpTypePointer StorageBuffer %type_StructuredBuffer_v3uint

struct S {
  float f;
};

ByteAddressBuffer bab;
RWByteAddressBuffer rwbab;
TextureBuffer<S> tb;
StructuredBuffer<uint3> sb;

[numthreads(1, 1, 1)]
void main() {}
