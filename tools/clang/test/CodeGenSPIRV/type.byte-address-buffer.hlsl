// Run: %dxc -T cs_6_0 -E main

// CHECK: OpName %type_ByteAddressBuffer "type.ByteAddressBuffer"
// CHECK: OpName %type_RWByteAddressBuffer "type.RWByteAddressBuffer"
// CHECK: OpDecorate %_runtimearr_uint ArrayStride 4
// CHECK: OpDecorate %type_ByteAddressBuffer BufferBlock
// CHECK: OpMemberDecorate %type_ByteAddressBuffer 0 Offset 0
// CHECK: OpMemberDecorate %type_ByteAddressBuffer 0 NonWritable
// CHECK: OpDecorate %type_RWByteAddressBuffer BufferBlock
// CHECK: OpMemberDecorate %type_RWByteAddressBuffer 0 Offset 0
// CHECK: %_runtimearr_uint = OpTypeRuntimeArray %uint
// CHECK: %type_ByteAddressBuffer = OpTypeStruct %_runtimearr_uint
// CHECK: %_ptr_Uniform_type_ByteAddressBuffer = OpTypePointer Uniform %type_ByteAddressBuffer
// CHECK: %type_RWByteAddressBuffer = OpTypeStruct %_runtimearr_uint
// CHECK: %_ptr_Uniform_type_RWByteAddressBuffer = OpTypePointer Uniform %type_RWByteAddressBuffer
// CHECK: %Buffer0 = OpVariable %_ptr_Uniform_type_ByteAddressBuffer Uniform
// CHECK: %BufferOut = OpVariable %_ptr_Uniform_type_RWByteAddressBuffer Uniform

ByteAddressBuffer Buffer0;
RWByteAddressBuffer BufferOut;

[numthreads(1, 1, 1)]
void main() {}
