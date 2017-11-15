// Run: %dxc -T vs_6_0 -E main

// CHECK: OpDecorate %_arr_v2float_uint_3 ArrayStride 8
// CHECK: OpDecorate %_arr_mat3v2float_uint_2 ArrayStride 32

// CHECK: OpMemberDecorate %T 0 Offset 0
// CHECK: OpMemberDecorate %T 1 Offset 32
// CHECK: OpMemberDecorate %T 1 MatrixStride 16
// CHECK: OpMemberDecorate %T 1 RowMajor
struct T {
                 float2   f1[3];
    column_major float3x2 f2[2];
};

// CHECK: OpMemberDecorate %type_PushConstant_S 0 Offset 0
// CHECK: OpMemberDecorate %type_PushConstant_S 1 Offset 16
// CHECK: OpMemberDecorate %type_PushConstant_S 2 Offset 32
// CHECK: OpMemberDecorate %type_PushConstant_S 3 Offset 128
// CHECK: OpMemberDecorate %type_PushConstant_S 3 MatrixStride 16
// CHECK: OpMemberDecorate %type_PushConstant_S 3 ColMajor

// CHECK: OpDecorate %type_PushConstant_S Block
struct S {
              float    f1;
              float3   f2;
              T        f4;
    row_major float2x3 f3;
};

[[vk::push_constant]]
S pcs;

float main() : A {
    return pcs.f1;
}
