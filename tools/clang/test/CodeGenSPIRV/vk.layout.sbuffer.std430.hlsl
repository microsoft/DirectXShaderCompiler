// Run: %dxc -T ps_6_0 -E main

struct R {     // Alignment       Offset     Size       Next
    float2 rf; // 8(vec2)      -> 0        + 8(vec2)  = 8
};             // 8               8          8

struct S {      // Alignment    Offset                                Size        Next
    R      sf1; // 8         -> 0                                   + 8         = 8
    float  sf2; // 4         -> 8                                   + 4         = 12
    float3 sf3; // 16(vec4)  -> 16 (12 round up to vec4 alignment)  + 12(vec3)  = 28
    float  sf4; // 4         -> 28                                  + 4         = 32
};              // 16(max)                                                        32

struct T {           // Alignment     Offset                               Size              = Next
    int      tf1;    // 4          -> 0                                  + 4                 = 4
    R        tf2[3]; // 8          -> 8                                  + 3 * stride(8)     = 32
    float3x2 tf3;    // 16(vec4)   -> 32 (32 round up to vec4 alignment) + 2 * stride(vec4)  = 64
    S        tf4;    // 16         -> 64 (64 round up to S alignment)    + 32                = 96
    float    tf5;    // 4          -> 96                                 + 4                 = 100
};                   // 16(max)                                                                112(100 round up to T max alignment)

struct SBuffer {              // Alignment   Offset                                 Size                     Next
                 bool     a;     // 4        -> 0                                    +     4                  = 4
                 uint1    b;     // 4        -> 4                                    +     4                  = 8
                 float3   c;     // 16(vec4) -> 16 (8 round up to vec4 alignment)    + 3 * 4                  = 28
    row_major    float2x3 d;     // 16(vec4) -> 32 (28 round up to vec4 alignment)   + 2 * stride(vec4)       = 64
    column_major float2x3 e;     // 16(vec4) -> 64 (64 round up to vec2 alignment)   + 3 * stride(vec2)       = 88
                 float2x1 f;     // 8(vec2)  -> 88 (88 round up to vec2 aligment)    + 2 * 4                  = 96
    row_major    float2x3 g[3];  // 16(vec4) -> 96 (96 round up to vec4 alignment)   + 3 * 2 * stride(vec4)   = 192
    column_major float2x2 h[4];  // 16(vec4) -> 192 (192 round up to vec2 alignment) + 4 * 2 * stride(vec2)   = 256
                 T        t;     // 16       -> 256 (352 round up to T alignment)    + 112                    = 368
                 float    z;     // 4        -> 368

};

StructuredBuffer<SBuffer> MySBuffer;

// CHECK:      OpDecorate %_arr_mat2v3float_uint_3 ArrayStride 32
// CHECK:      OpDecorate %_arr_mat2v2float_uint_4 ArrayStride 16

// CHECK:      OpMemberDecorate %R 0 Offset 0

// CHECK:      OpDecorate %_arr_R_uint_3 ArrayStride 8

// CHECK:      OpMemberDecorate %S 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %S 1 Offset 8
// CHECK-NEXT: OpMemberDecorate %S 2 Offset 16
// CHECK-NEXT: OpMemberDecorate %S 3 Offset 28

// CHECK:      OpMemberDecorate %T 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %T 1 Offset 8
// CHECK-NEXT: OpMemberDecorate %T 2 Offset 32
// CHECK-NEXT: OpMemberDecorate %T 2 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %T 2 RowMajor
// CHECK-NEXT: OpMemberDecorate %T 3 Offset 64
// CHECK-NEXT: OpMemberDecorate %T 4 Offset 96

// CHECK:      OpMemberDecorate %SBuffer 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %SBuffer 1 Offset 4
// CHECK-NEXT: OpMemberDecorate %SBuffer 2 Offset 16
// CHECK-NEXT: OpMemberDecorate %SBuffer 3 Offset 32
// CHECK-NEXT: OpMemberDecorate %SBuffer 3 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %SBuffer 3 ColMajor
// CHECK-NEXT: OpMemberDecorate %SBuffer 4 Offset 64
// CHECK-NEXT: OpMemberDecorate %SBuffer 4 MatrixStride 8
// CHECK-NEXT: OpMemberDecorate %SBuffer 4 RowMajor
// CHECK-NEXT: OpMemberDecorate %SBuffer 5 Offset 88
// CHECK-NEXT: OpMemberDecorate %SBuffer 6 Offset 96
// CHECK-NEXT: OpMemberDecorate %SBuffer 6 MatrixStride 16
// CHECK-NEXT: OpMemberDecorate %SBuffer 6 ColMajor
// CHECK-NEXT: OpMemberDecorate %SBuffer 7 Offset 192
// CHECK-NEXT: OpMemberDecorate %SBuffer 7 MatrixStride 8
// CHECK-NEXT: OpMemberDecorate %SBuffer 7 RowMajor
// CHECK-NEXT: OpMemberDecorate %SBuffer 8 Offset 256
// CHECK-NEXT: OpMemberDecorate %SBuffer 9 Offset 368

// CHECK:      OpDecorate %_runtimearr_SBuffer ArrayStride 384

// CHECK:      OpMemberDecorate %type_StructuredBuffer_SBuffer 0 Offset 0
// CHECK-NEXT: OpMemberDecorate %type_StructuredBuffer_SBuffer 0 NonWritable
// CHECK-NEXT: OpDecorate %type_StructuredBuffer_SBuffer BufferBlock

float main() : SV_Target {
    return 1.0;
}
