// Run: %dxc -T vs_6_0 -E main -Zpc

// CHECK: OpDecorate %_arr_mat2v3float_uint_5 ArrayStride 32
// CHECK: OpDecorate %_arr_mat2v3float_uint_5_0 ArrayStride 48

// CHECK: OpMemberDecorate %type_MyCBuffer 0 ColMajor
// CHECK: OpMemberDecorate %type_MyCBuffer 1 RowMajor
// CHECK: OpMemberDecorate %type_MyCBuffer 2 RowMajor

// CHECK: %type_MyCBuffer = OpTypeStruct %_arr_mat2v3float_uint_5 %_arr_mat2v3float_uint_5_0 %_arr_mat2v3float_uint_5_0
cbuffer MyCBuffer {
    row_major    float2x3 matrices1[5];
    column_major float2x3 matrices2[5];
                 float2x3 matrices3[5];
}

void main() {
    // Check that the result types for access chains are correct
// CHECK: {{%\d+}} = OpAccessChain %_ptr_Uniform__arr_mat2v3float_uint_5 %MyCBuffer %int_0
// CHECK: {{%\d+}} = OpAccessChain %_ptr_Uniform__arr_mat2v3float_uint_5_0 %MyCBuffer %int_1
// CHECK: {{%\d+}} = OpAccessChain %_ptr_Uniform__arr_mat2v3float_uint_5_0 %MyCBuffer %int_2
    float2x3 m1 = matrices1[1];
    float2x3 m2 = matrices2[2];
    float2x3 m3 = matrices3[3];
}
