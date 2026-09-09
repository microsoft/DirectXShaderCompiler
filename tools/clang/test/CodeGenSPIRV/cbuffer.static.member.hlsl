// RUN: %dxc -T vs_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// A `static` variable declared inside a cbuffer is an ordinary global variable,
// not a member of the buffer. It must be left out of the buffer's struct (it is
// not part of the layout) *and* it must not consume a member index, otherwise
// the access chains for the members declared after it are shifted, and the last
// one ends up out of bounds.

// CHECK:     OpMemberName %type_MyCBuffer 0 "a"
// CHECK:     OpMemberName %type_MyCBuffer 1 "b"
// CHECK:     OpMemberName %type_MyCBuffer 2 "c"
// CHECK-NOT: OpMemberName %type_MyCBuffer 3

// CHECK-DAG: %uint_10 = OpConstant %uint 10
// CHECK-DAG: %uint_11 = OpConstant %uint 11
// CHECK-DAG: %uint_12 = OpConstant %uint 12

// CHECK: %type_MyCBuffer = OpTypeStruct %uint %uint %uint

cbuffer MyCBuffer {
  uint a;
  static const uint a_mode = 10;
  uint b;
  static const uint b_mode = 11;
  uint c;
  static const uint c_mode = 12;
};

float4 main() : SV_Position {
// CHECK: OpAccessChain %_ptr_Uniform_uint %MyCBuffer %int_0
// CHECK: OpAccessChain %_ptr_Uniform_uint %MyCBuffer %int_1
// CHECK: OpAccessChain %_ptr_Uniform_uint %MyCBuffer %int_2
  return float4(a + b + c, a_mode, b_mode, c_mode);
}
