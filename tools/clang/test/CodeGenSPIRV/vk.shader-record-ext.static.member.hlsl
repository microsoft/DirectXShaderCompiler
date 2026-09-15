// RUN: %dxc -T lib_6_3 -fspv-target-env=vulkan1.2 -fcgl  %s -spirv | FileCheck %s

// Same as cbuffer.static.member.hlsl, but for a shader record buffer, which
// goes through createShaderRecordBuffer() instead of createCTBuffer(). A
// `static` variable declared inside the buffer is an ordinary global variable,
// not a member of the buffer. It must be left out of the buffer's struct (it is
// not part of the layout) *and* it must not consume a member index, otherwise
// the access chains for the members declared after it are shifted, and the last
// one ends up out of bounds.

// CHECK:     OpMemberName %type_ShaderRecordBufferKHR_block 0 "a"
// CHECK:     OpMemberName %type_ShaderRecordBufferKHR_block 1 "b"
// CHECK:     OpMemberName %type_ShaderRecordBufferKHR_block 2 "c"
// CHECK-NOT: OpMemberName %type_ShaderRecordBufferKHR_block 3

// CHECK-DAG: %uint_10 = OpConstant %uint 10
// CHECK-DAG: %uint_11 = OpConstant %uint 11
// CHECK-DAG: %uint_12 = OpConstant %uint 12

// CHECK: %type_ShaderRecordBufferKHR_block = OpTypeStruct %uint %uint %uint

[[vk::shader_record_ext]]
cbuffer block {
  uint a;
  static const uint a_mode = 10;
  uint b;
  static const uint b_mode = 11;
  uint c;
  static const uint c_mode = 12;
}

struct Payload { float p; };
struct Attr    { float a; };

[shader("closesthit")]
void main(inout Payload P, in Attr A) {
// CHECK: OpAccessChain %_ptr_ShaderRecordBufferKHR_uint %block %int_0
// CHECK: OpAccessChain %_ptr_ShaderRecordBufferKHR_uint %block %int_1
// CHECK: OpAccessChain %_ptr_ShaderRecordBufferKHR_uint %block %int_2
  P.p = a + b + c + a_mode + b_mode + c_mode;
}
