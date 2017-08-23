// Run: %dxc -T vs_6_0 -E main

// Deep nested array of matrices
// Depp nested majorness
struct R {                         // Alignment    Offset  Size                              Next
    row_major    float2x3 rf1[3];  // 16(vec4)  -> 0     + 3(array) * stride(2 * 16(vec4)) = 96
    column_major float2x3 rf2[4];  // 16(vec4)  -> 96    + 4(array) * stride(3 * 16(vec4)) = 288
                 float2x3 rf3[2];  // 16(vec4)  -> 288   + 2(array) * stride(3 * 16(vec4)) = 384
                 int      rf4;     // 4         -> 384   + 4                               = 388
};                                 // 16(max)                                                400 (388 round up to R alignment)

// Array of scalars, vectors, matrices, and structs
struct S {                         // Alignment   Offset  Size                              Next
    float3       sf1[3];           // 16(vec4) -> 0     + 3(array) * 16(vec4)             = 48
    float        sf2[3];           // 4        -> 48    + 3(array) * 16(vec4)             = 96
    R            sf3[4];           // 16       -> 96    + 4(array) * stride(400)          = 1696
    row_major    float3x2 sf4[2];  // 16(vec4) -> 1696  + 2(array) * stride(3 * 16(vec4)) = 1792
    column_major float3x2 sf5[3];  // 16(vec4) -> 1792  + 3(array) * stride(2 * 16(vec4)) = 1888
                 float3x2 sf6[4];  // 16(vec4) -> 1888  + 4(array) * stride(2 * 16(vec4)) = 2016
                 float    sf7;     // 4        -> 2016  + 4                               = 2020
};                                 // 16(max)                                               2032 (2020 round up to S alignment)

struct T {        // Alignment    Offset  Size              Next
    R    tf1[2];  // 16        -> 0     + 2(array) * 400  = 800
    S    tf2[3];  // 16        -> 800   + 3(array) * 2032 = 6896
    uint tf3;     // 4         -> 6896  + 4               = 6900
};                // 16(max)                                6912 (6900 round up to T alignment)

cbuffer MyCbuffer {  // Alignment   Offset   Size              Next
    T    t[2];       // 16       -> 0      + 2(array) * 6912 = 13824
    bool z;          // 4        -> 13824
};

// CHECK:      OpDecorate %_arr_mat2v3float_uint_3 ArrayStride 32
// CHECK:      OpDecorate %_arr_mat2v3float_uint_4 ArrayStride 48
// CHECK:      OpDecorate %_arr_mat2v3float_uint_2 ArrayStride 48

// CHECK:      OpMemberDecorate %R 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %R 0 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %R 0 ColMajor
// CHECK-NEXT: OpMemberDecorate %R 1 Offset 96
// CHECK-NEXT: OpMemberDecorate %R 1 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %R 1 RowMajor
// CHECK-NEXT: OpMemberDecorate %R 2 Offset 288
// CHECK-NEXT: OpMemberDecorate %R 2 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %R 2 RowMajor
// CHECK-NEXT: OpMemberDecorate %R 3 Offset 384

// CHECK:      OpDecorate %_arr_R_uint_2 ArrayStride 400
// CHECK:      OpDecorate %_arr_v3float_uint_3 ArrayStride 16
// CHECK:      OpDecorate %_arr_float_uint_3 ArrayStride 16
// CHECK:      OpDecorate %_arr_R_uint_4 ArrayStride 400

// CHECK:      OpDecorate %_arr_mat3v2float_uint_2 ArrayStride 48
// CHECK:      OpDecorate %_arr_mat3v2float_uint_3 ArrayStride 32
// CHECK:      OpDecorate %_arr_mat3v2float_uint_4 ArrayStride 32

// CHECK:      OpMemberDecorate %S 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %S 1 Offset 48
// CHECK-NEXT: OpMemberDecorate %S 2 Offset 96
// CHECK-NEXT: OpMemberDecorate %S 3 Offset 1696
// CHECK-NEXT: OpMemberDecorate %S 3 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %S 3 ColMajor
// CHECK-NEXT: OpMemberDecorate %S 4 Offset 1792
// CHECK-NEXT: OpMemberDecorate %S 4 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %S 4 RowMajor
// CHECK-NEXT: OpMemberDecorate %S 5 Offset 1888
// CHECK-NEXT: OpMemberDecorate %S 5 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %S 5 RowMajor
// CHECK-NEXT: OpMemberDecorate %S 6 Offset 2016

// CHECK:      OpDecorate %_arr_S_uint_3 ArrayStride 2032

// CHECK:      OpMemberDecorate %T 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %T 1 Offset 800
// CHECK-NEXT: OpMemberDecorate %T 2 Offset 6896

// CHECK:      OpDecorate %_arr_T_uint_2 ArrayStride 6912

// CHECK-NEXT: OpMemberDecorate %type_MyCbuffer 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %type_MyCbuffer 1 Offset 13824

// CHECK:      OpDecorate %type_MyCbuffer Block
float main() : A {
    return 1.0;
}
