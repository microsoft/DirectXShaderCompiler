// Run: %dxc -T ps_6_0 -E main -Zpc

// CHECK: OpDecorate %_runtimearr_mat2v3float ArrayStride 24
// CHECK: OpMemberDecorate %type_StructuredBuffer_mat2v3float 0 RowMajor
       StructuredBuffer<float2x3> ROSB1;
// CHECK: OpDecorate %_runtimearr_mat3v2float ArrayStride 32
// CHECK: OpMemberDecorate %type_RWStructuredBuffer_mat3v2float 0 RowMajor
     RWStructuredBuffer<float3x2> RWSB1;
// CHECK: OpDecorate %_runtimearr_mat4v3float ArrayStride 48
// CHECK: OpMemberDecorate %type_AppendStructuredBuffer_mat4v3float 0 RowMajor
 AppendStructuredBuffer<float4x3> ASB1;
// CHECK: OpDecorate %_runtimearr_mat3v4float ArrayStride 64
// CHECK: OpMemberDecorate %type_ConsumeStructuredBuffer_mat3v4float 0 RowMajor
ConsumeStructuredBuffer<float3x4> CSB1;

// NOTE: The parsed AST does not convey the majorness information for
// the following cases right now.
/*
       StructuredBuffer<row_major float2x3> ROSB2;
     RWStructuredBuffer<row_major float3x2> RWSB2;
 AppendStructuredBuffer<row_major float4x3> ASB2;
ConsumeStructuredBuffer<row_major float3x4> CSB2;

       StructuredBuffer<column_major float2x3> ROSB3;
     RWStructuredBuffer<column_major float3x2> RWSB3;
 AppendStructuredBuffer<column_major float4x3> ASB3;
ConsumeStructuredBuffer<column_major float3x4> CSB3;
*/

float4 main() : SV_Target {
    return ROSB1[0][0][0] // + ROSB2[0][0][0] + ROSB3[0][0][0]
        ;
}
