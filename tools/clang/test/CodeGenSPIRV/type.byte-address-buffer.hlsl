// RUN: %dxc -T cs_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// CHECK: OpName %type_ByteAddressBuffer "type.ByteAddressBuffer"
// CHECK: OpName %type_RWByteAddressBuffer "type.RWByteAddressBuffer"
// CHECK: OpDecorate %_runtimearr_uint ArrayStride 4
// CHECK: OpMemberDecorate %type_ByteAddressBuffer 0 Offset 0
// CHECK: OpMemberDecorate %type_ByteAddressBuffer 0 NonWritable
// CHECK: OpDecorate %type_ByteAddressBuffer BufferBlock
// CHECK: OpMemberDecorate %type_RWByteAddressBuffer 0 Offset 0
// CHECK: OpDecorate %type_RWByteAddressBuffer BufferBlock
// CHECK: %_runtimearr_uint = OpTypeRuntimeArray %uint
// CHECK: %type_ByteAddressBuffer = OpTypeStruct %_runtimearr_uint
// CHECK: %_ptr_Uniform_type_ByteAddressBuffer = OpTypePointer Uniform %type_ByteAddressBuffer
// CHECK: %type_RWByteAddressBuffer = OpTypeStruct %_runtimearr_uint
// CHECK: %_ptr_Uniform_type_RWByteAddressBuffer = OpTypePointer Uniform %type_RWByteAddressBuffer
// CHECK: %Buffer0 = OpVariable %_ptr_Uniform_type_ByteAddressBuffer Uniform
// CHECK: %BufferArary = OpVariable %_ptr_Uniform__arr_type_ByteAddressBuffer_uint_2 Uniform
// CHECK: %BufferOut = OpVariable %_ptr_Uniform_type_RWByteAddressBuffer Uniform

ByteAddressBuffer Buffer0;
ByteAddressBuffer BufferArary[2];
RWByteAddressBuffer BufferOut;

// CHECK: %src_main = OpFunction
[numthreads(1, 1, 1)]
void main() {
// CHECK: %LocalArary = OpVariable %_ptr_Function__ptr_Uniform__arr_type_ByteAddressBuffer_uint_2 Function
// CHECK: %Local = OpVariable %_ptr_Function__ptr_Uniform_type_ByteAddressBuffer Function
  ByteAddressBuffer LocalArary[2];

// CHECK: OpStore %LocalArary %BufferArary
  LocalArary = BufferArary;

// CHECK: [[array:%[0-9]+]] = OpLoad %_ptr_Uniform__arr_type_ByteAddressBuffer_uint_2 %LocalArary
// CHECK: [[ac:%[0-9]+]] = OpAccessChain %_ptr_Uniform_type_ByteAddressBuffer [[array]] %int_0
// CHECK: OpStore %Local [[ac]]
  ByteAddressBuffer Local = LocalArary[0];
}
