// Run: %dxc -T ps_6_0 -E main

struct SubBuffer {
    float    a[1];
    float2   b[1];
    float2x3 c[1];
};

struct BufferType {
    float     a;
    float3    b;
    float3x2  c;
    SubBuffer d[1];
};

RWStructuredBuffer<BufferType> sbuf;  // %BufferType                     & %SubBuffer
    ConstantBuffer<BufferType> cbuf;  // %type_ConstantBuffer_BufferType & %SubBuffer_0

void main(uint index: A) {
    // Same storage class

// CHECK:      [[sbuf0:%\d+]] = OpAccessChain %_ptr_Uniform_BufferType %sbuf %int_0 %uint_0
// CHECK-NEXT: [[val:%\d+]] = OpLoad %BufferType [[sbuf0]]
// CHECK-NEXT: [[sbuf8:%\d+]] = OpAccessChain %_ptr_Uniform_BufferType %sbuf %int_0 %uint_8
// CHECK-NEXT: OpStore [[sbuf8]] [[val]]
    sbuf[8] = sbuf[0];

    // Different storage class


// CHECK-NEXT: [[lbuf:%\d+]] = OpLoad %BufferType_0 %lbuf
// CHECK-NEXT: [[sbuf5:%\d+]] = OpAccessChain %_ptr_Uniform_BufferType %sbuf %int_0 %uint_5

    // sbuf[5].a <- lbuf.a
// CHECK-NEXT: [[val:%\d+]] = OpCompositeExtract %float [[lbuf]] 0
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_float [[sbuf5]] %uint_0
// CHECK-NEXT: OpStore [[ptr]] [[val]]

    // sbuf[5].b <- lbuf.b
// CHECK-NEXT: [[val:%\d+]] = OpCompositeExtract %v3float [[lbuf]] 1
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_v3float [[sbuf5]] %uint_1
// CHECK-NEXT: OpStore [[ptr]] [[val]]

    // sbuf[5].c <- lbuf.c
// CHECK-NEXT: [[val:%\d+]] = OpCompositeExtract %mat3v2float [[lbuf]] 2
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_mat3v2float [[sbuf5]] %uint_2
// CHECK-NEXT: OpStore [[ptr]] [[val]]

// CHECK-NEXT: [[lbuf_d:%\d+]] = OpCompositeExtract %_arr_SubBuffer_1_uint_1 [[lbuf]] 3
// CHECK-NEXT: [[sbuf_d:%\d+]] = OpAccessChain %_ptr_Uniform__arr_SubBuffer_uint_1 [[sbuf5]] %uint_3
// CHECK-NEXT: [[lbuf_d0:%\d+]] = OpCompositeExtract %SubBuffer_1 [[lbuf_d]] 0
// CHECK-NEXT: [[sbuf_d0:%\d+]] = OpAccessChain %_ptr_Uniform_SubBuffer [[sbuf_d]] %uint_0

    // sbuf[5].d[0].a[0] <- lbuf.a[0]
// CHECK-NEXT: [[lbuf_d0_a:%\d+]] = OpCompositeExtract %_arr_float_uint_1_1 [[lbuf_d0]] 0
// CHECK-NEXT: [[sbuf_d0_a:%\d+]] = OpAccessChain %_ptr_Uniform__arr_float_uint_1 [[sbuf_d0]] %uint_0
// CHECK-NEXT: [[lbuf_d0_a0:%\d+]] = OpCompositeExtract %float [[lbuf_d0_a]] 0
// CHECK-NEXT: [[sbuf_d0_a0:%\d+]] = OpAccessChain %_ptr_Uniform_float [[sbuf_d0_a]] %uint_0
// CHECK-NEXT: OpStore [[sbuf_d0_a0]] [[lbuf_d0_a0]]

    // sbuf[5].d[0].b[0] <- lbuf.b[0]
// CHECK-NEXT: [[lbuf_d0_b:%\d+]] = OpCompositeExtract %_arr_v2float_uint_1_1 [[lbuf_d0]] 1
// CHECK-NEXT: [[sbuf_d0_b:%\d+]] = OpAccessChain %_ptr_Uniform__arr_v2float_uint_1 [[sbuf_d0]] %uint_1
// CHECK-NEXT: [[lbuf_d0_b0:%\d+]] = OpCompositeExtract %v2float [[lbuf_d0_b]] 0
// CHECK-NEXT: [[sbuf_d0_b0:%\d+]] = OpAccessChain %_ptr_Uniform_v2float [[sbuf_d0_b]] %uint_0
// CHECK-NEXT: OpStore [[sbuf_d0_b0]] [[lbuf_d0_b0]]

    // sbuf[5].d[0].c[0] <- lbuf.c[0]
// CHECK-NEXT: [[lbuf_d0_c:%\d+]] = OpCompositeExtract %_arr_mat2v3float_uint_1_1 [[lbuf_d0]] 2
// CHECK-NEXT: [[sbuf_d0_c:%\d+]] = OpAccessChain %_ptr_Uniform__arr_mat2v3float_uint_1 [[sbuf_d0]] %uint_2
// CHECK-NEXT: [[lbuf_d0_c0:%\d+]] = OpCompositeExtract %mat2v3float [[lbuf_d0_c]] 0
// CHECK-NEXT: [[sbuf_d0_c0:%\d+]] = OpAccessChain %_ptr_Uniform_mat2v3float [[sbuf_d0_c]] %uint_0
// CHECK-NEXT: OpStore [[sbuf_d0_c0]] [[lbuf_d0_c0]]
    BufferType lbuf;                  // %BufferType_0                   & %SubBuffer_1
    sbuf[5]  = lbuf;             // %BufferType <- %BufferType_0

// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_SubBuffer_0 %cbuf %int_3 %int_0
// CHECK-NEXT: [[cbuf_d0:%\d+]] = OpLoad %SubBuffer_0 [[ptr]]

    // sub.a[0] <- cbuf.d[0].a[0]
// CHECK-NEXT: [[cbuf_d0_a:%\d+]] = OpCompositeExtract %_arr_float_uint_1_0 [[cbuf_d0]] 0
// CHECK-NEXT: [[sub_a:%\d+]] = OpAccessChain %_ptr_Function__arr_float_uint_1_1 %sub %uint_0
// CHECK-NEXT: [[cbuf_d0_a0:%\d+]] = OpCompositeExtract %float [[cbuf_d0_a]] 0
// CHECK-NEXT: [[sub_a0:%\d+]] = OpAccessChain %_ptr_Function_float [[sub_a]] %uint_0
// CHECK-NEXT: OpStore [[sub_a0]] [[cbuf_d0_a0]]

    // sub.b[0] <- cbuf.d[0].b[0]
// CHECK-NEXT: [[cbuf_d0_b:%\d+]] = OpCompositeExtract %_arr_v2float_uint_1_0 [[cbuf_d0]] 1
// CHECK-NEXT: [[sub_b:%\d+]] = OpAccessChain %_ptr_Function__arr_v2float_uint_1_1 %sub %uint_1
// CHECK-NEXT: [[cbuf_d0_b0:%\d+]] = OpCompositeExtract %v2float [[cbuf_d0_b]] 0
// CHECK-NEXT: [[sub_b0:%\d+]] = OpAccessChain %_ptr_Function_v2float [[sub_b]] %uint_0
// CHECK-NEXT: OpStore [[sub_b0]] [[cbuf_d0_b0]]

    // sub.c[0] <- cbuf.d[0].c[0]
// CHECK-NEXT: [[cbuf_d0_c:%\d+]] = OpCompositeExtract %_arr_mat2v3float_uint_1_0 [[cbuf_d0]] 2
// CHECK-NEXT: [[sub_c:%\d+]] = OpAccessChain %_ptr_Function__arr_mat2v3float_uint_1_1 %sub %uint_2
// CHECK-NEXT: [[cbuf_d0_c0:%\d+]] = OpCompositeExtract %mat2v3float [[cbuf_d0_c]] 0
// CHECK-NEXT: [[sub_c0:%\d+]] = OpAccessChain %_ptr_Function_mat2v3float [[sub_c]] %uint_0
// CHECK-NEXT: OpStore [[sub_c0]] [[cbuf_d0_c0]]
    SubBuffer sub = cbuf.d[0];        // %SubBuffer_1 <- %SubBuffer_0
}
