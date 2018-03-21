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

// CHECK-NEXT:     [[lbuf_a:%\d+]] = OpCompositeExtract %float [[lbuf]] 0
// CHECK-NEXT:     [[lbuf_b:%\d+]] = OpCompositeExtract %v3float [[lbuf]] 1
// CHECK-NEXT:     [[lbuf_c:%\d+]] = OpCompositeExtract %mat3v2float [[lbuf]] 2

    // Get lbuf.d[0]
// CHECK-NEXT:     [[lbuf_d:%\d+]] = OpCompositeExtract %_arr_SubBuffer_1_uint_1 [[lbuf]] 3
// CHECK-NEXT:    [[lbuf_d0:%\d+]] = OpCompositeExtract %SubBuffer_1 [[lbuf_d]] 0

    // Reconstruct lbuf.d[0].a
// CHECK-NEXT:  [[lbuf_d0_a:%\d+]] = OpCompositeExtract %_arr_float_uint_1_1 [[lbuf_d0]] 0
// CHECK-NEXT: [[lbuf_d0_a0:%\d+]] = OpCompositeExtract %float [[lbuf_d0_a]] 0
// CHECK-NEXT:  [[sbuf_d0_a:%\d+]] = OpCompositeConstruct %_arr_float_uint_1 [[lbuf_d0_a0]]

    // Reconstruct lbuf.d[0].b
// CHECK-NEXT:  [[lbuf_d0_b:%\d+]] = OpCompositeExtract %_arr_v2float_uint_1_1 [[lbuf_d0]] 1
// CHECK-NEXT: [[lbuf_d0_b0:%\d+]] = OpCompositeExtract %v2float [[lbuf_d0_b]] 0
// CHECK-NEXT:  [[sbuf_d0_b:%\d+]] = OpCompositeConstruct %_arr_v2float_uint_1 [[lbuf_d0_b0]]

    // Reconstruct lbuf.d[0].c
// CHECK-NEXT:  [[lbuf_d0_c:%\d+]] = OpCompositeExtract %_arr_mat2v3float_uint_1_1 [[lbuf_d0]] 2
// CHECK-NEXT: [[lbuf_d0_c0:%\d+]] = OpCompositeExtract %mat2v3float [[lbuf_d0_c]] 0
// CHECK-NEXT:  [[sbuf_d0_c:%\d+]] = OpCompositeConstruct %_arr_mat2v3float_uint_1 [[lbuf_d0_c0]]

// CHECK-NEXT:    [[sbuf_d0:%\d+]] = OpCompositeConstruct %SubBuffer [[sbuf_d0_a]] [[sbuf_d0_b]] [[sbuf_d0_c]]
// CHECK-NEXT:     [[sbuf_d:%\d+]] = OpCompositeConstruct %_arr_SubBuffer_uint_1 [[sbuf_d0]]
// CHECK-NEXT:   [[sbuf_val:%\d+]] = OpCompositeConstruct %BufferType [[lbuf_a]] [[lbuf_b]] [[lbuf_c]] [[sbuf_d]]

// CHECK-NEXT: OpStore [[sbuf5]] [[sbuf_val]]
    BufferType lbuf;                  // %BufferType_0                   & %SubBuffer_1
    sbuf[5]  = lbuf;             // %BufferType <- %BufferType_0

// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_SubBuffer_0 %cbuf %int_3 %int_0
// CHECK-NEXT: [[cbuf_d0:%\d+]] = OpLoad %SubBuffer_0 [[ptr]]

    // Reconstruct lbuf.d[0].a
// CHECK-NEXT:  [[cbuf_d0_a:%\d+]] = OpCompositeExtract %_arr_float_uint_1_0 [[cbuf_d0]] 0
// CHECK-NEXT: [[cbuf_d0_a0:%\d+]] = OpCompositeExtract %float [[cbuf_d0_a]] 0
// CHECK-NEXT:      [[sub_a:%\d+]] = OpCompositeConstruct %_arr_float_uint_1_1 [[cbuf_d0_a0]]

    // Reconstruct lbuf.d[0].b
// CHECK-NEXT:  [[cbuf_d0_b:%\d+]] = OpCompositeExtract %_arr_v2float_uint_1_0 [[cbuf_d0]] 1
// CHECK-NEXT: [[cbuf_d0_b0:%\d+]] = OpCompositeExtract %v2float [[cbuf_d0_b]] 0
// CHECK-NEXT:      [[sub_b:%\d+]] = OpCompositeConstruct %_arr_v2float_uint_1_1 [[cbuf_d0_b0]]

    // Reconstruct lbuf.d[0].c
// CHECK-NEXT:  [[cbuf_d0_c:%\d+]] = OpCompositeExtract %_arr_mat2v3float_uint_1_0 [[cbuf_d0]] 2
// CHECK-NEXT: [[cbuf_d0_c0:%\d+]] = OpCompositeExtract %mat2v3float [[cbuf_d0_c]] 0
// CHECK-NEXT:      [[sub_c:%\d+]] = OpCompositeConstruct %_arr_mat2v3float_uint_1_1 [[cbuf_d0_c0]]

// CHECK-NEXT:    [[sub_val:%\d+]] = OpCompositeConstruct %SubBuffer_1 [[sub_a]] [[sub_b]] [[sub_c]]
// CHECK-NEXT:                       OpStore %sub [[sub_val]]
    SubBuffer sub = cbuf.d[0];        // %SubBuffer_1 <- %SubBuffer_0
}
