// Run: %dxc -T ps_6_0 -E main

// Deep nested array of matrices
// Depp nested majorness
struct R {                         // Alignment    Offset  Size                              Next
    row_major    float2x3 rf1[3];  // 16(vec4)  -> 0     + 3(array) * stride(2 * 16(vec4)) = 96
    column_major float2x3 rf2[4];  // 8(vec2)   -> 96    + 4(array) * stride(3 * 8(vec2))  = 192
                 float2x3 rf3[2];  // 8(vec2)   -> 192   + 2(array) * stride(3 * 8(vec2))  = 240
                 int      rf4;     // 4         -> 240   + 4                               = 244
};                                 // 16(max)                                                256 (244 round up to R alignment)

// Array of scalars, vectors, matrices, and structs
struct S {                         // Alignment   Offset  Size                              Next
    float3       sf1[3];           // 16(vec4) -> 0     + 3(array) * 16(vec4)             = 48
    float        sf2[3];           // 4        -> 48    + 3(array) * 4                    = 60
    R            sf3[4];           // 16       -> 64    + 4(array) * stride(256)          = 1088
    row_major    float3x2 sf4[2];  // 8(vec2)  -> 1088  + 2(array) * stride(3 * 8(vec2))  = 1136
    column_major float3x2 sf5[3];  // 16(vec4) -> 1136  + 3(array) * stride(2 * 16(vec4)) = 1232
                 float3x2 sf6[4];  // 16(vec4) -> 1232  + 4(array) * stride(2 * 16(vec4)) = 1360
                 float    sf7;     // 4        -> 1360  + 4                               = 1364
};                                 // 16(max)                                               1376 (1364 round up to S alignment)

struct T {        // Alignment    Offset  Size              Next
    R    tf1[2];  // 16        -> 0     + 2(array) * 256  = 512
    S    tf2[3];  // 16        -> 512   + 3(array) * 1376 = 4640
    uint tf3;     // 4         -> 4640  + 4               = 4644
};                // 16(max)                                4656 (4640 round up to T alignment)

struct SBuffer {  // Alignment   Offset   Size                 Next
    T    t[2];       // 16       -> 0      + 2(array) * 4656 = 9312
    bool z;          // 4        -> 9312
};

RWStructuredBuffer<SBuffer> MySBuffer;

// CHECK:      OpDecorate %_arr_mat2v3float_uint_3 ArrayStride 32
// CHECK:      OpDecorate %_arr_mat2v3float_uint_4 ArrayStride 24
// CHECK:      OpDecorate %_arr_mat2v3float_uint_2 ArrayStride 24

// CHECK:      OpMemberDecorate %R 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %R 0 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %R 0 ColMajor
// CHECK-NEXT: OpMemberDecorate %R 1 Offset 96
// CHECK-NEXT: OpMemberDecorate %R 1 MatrixStride 8
// CHECK-NEXT: OpMemberDecorate %R 1 RowMajor
// CHECK-NEXT: OpMemberDecorate %R 2 Offset 192
// CHECK-NEXT: OpMemberDecorate %R 2 MatrixStride 8
// CHECK-NEXT: OpMemberDecorate %R 2 RowMajor
// CHECK-NEXT: OpMemberDecorate %R 3 Offset 240

// CHECK:      OpDecorate %_arr_R_uint_2 ArrayStride 256
// CHECK:      OpDecorate %_arr_v3float_uint_3 ArrayStride 16
// CHECK:      OpDecorate %_arr_float_uint_3 ArrayStride 4
// CHECK:      OpDecorate %_arr_R_uint_4 ArrayStride 256

// CHECK:      OpDecorate %_arr_mat3v2float_uint_2 ArrayStride 24
// CHECK:      OpDecorate %_arr_mat3v2float_uint_3 ArrayStride 32
// CHECK:      OpDecorate %_arr_mat3v2float_uint_4 ArrayStride 32

// CHECK:      OpMemberDecorate %S 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %S 1 Offset 48
// CHECK-NEXT: OpMemberDecorate %S 2 Offset 64
// CHECK-NEXT: OpMemberDecorate %S 3 Offset 1088
// CHECK-NEXT: OpMemberDecorate %S 3 MatrixStride 8
// CHECK-NEXT: OpMemberDecorate %S 3 ColMajor
// CHECK-NEXT: OpMemberDecorate %S 4 Offset 1136
// CHECK-NEXT: OpMemberDecorate %S 4 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %S 4 RowMajor
// CHECK-NEXT: OpMemberDecorate %S 5 Offset 1232
// CHECK-NEXT: OpMemberDecorate %S 5 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %S 5 RowMajor
// CHECK-NEXT: OpMemberDecorate %S 6 Offset 1360

// CHECK:      OpDecorate %_arr_S_uint_3 ArrayStride 1376

// CHECK:      OpMemberDecorate %T 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %T 1 Offset 512
// CHECK-NEXT: OpMemberDecorate %T 2 Offset 4640

// CHECK:      OpDecorate %_arr_T_uint_2 ArrayStride 4656

// CHECK-NEXT: OpMemberDecorate %SBuffer 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %SBuffer 1 Offset 9312

// CHECK:      OpDecorate %_runtimearr_SBuffer ArrayStride 9328

// CHECK:      OpMemberDecorate %type_RWStructuredBuffer_SBuffer 0 Offset 0
// CHECK-NEXT: OpDecorate %type_RWStructuredBuffer_SBuffer BufferBlock

float4 main() : SV_Target {
    return 1.0;
}
