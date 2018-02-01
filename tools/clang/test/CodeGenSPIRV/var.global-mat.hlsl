// Run: %dxc -T vs_6_0 -E main

// CHECK:      OpMemberDecorate %type_gMat1 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %type_gMat1 0 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %type_gMat1 0 RowMajor
// CHECK-NEXT: OpDecorate %type_gMat1 Block

// CHECK:      OpMemberDecorate %type_gMat2 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %type_gMat2 0 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %type_gMat2 0 ColMajor
// CHECK-NEXT: OpDecorate %type_gMat2 Block

// CHECK: %gMat1 = OpVariable %_ptr_Uniform_type_gMat1 Uniform
// CHECK: %gMat2 = OpVariable %_ptr_Uniform_type_gMat2 Uniform
// CHECK: %gMat3 = OpVariable %_ptr_Uniform_type_gMat1 Uniform
             float2x3 gMat1;
row_major    float2x3 gMat2;
column_major float2x3 gMat3;

void main() {
// CHECK:      [[mat:%\d+]] = OpAccessChain %_ptr_Uniform_mat2v3float %gMat1 %int_0
// CHECK-NEXT:     {{%\d+}} = OpLoad %mat2v3float [[mat]]
    float2x3 mat = gMat1;

// CHECK:      [[mat:%\d+]] = OpAccessChain %_ptr_Uniform_mat2v3float %gMat2 %int_0
// CHECK-NEXT:     {{%\d+}} = OpAccessChain %_ptr_Uniform_v3float [[mat]] %uint_0
    float3   vec = gMat2[0];

// CHECK:      [[mat:%\d+]] = OpAccessChain %_ptr_Uniform_mat2v3float %gMat3 %int_0
// CHECK-NEXT:     {{%\d+}} = OpAccessChain %_ptr_Uniform_float [[mat]] %int_0 %int_0
    float scalar = gMat3._11;
}
