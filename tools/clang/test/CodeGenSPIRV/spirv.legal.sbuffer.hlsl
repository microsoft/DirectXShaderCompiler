// Run: %dxc -T vs_6_0 -E main

// Note: The following is invalid SPIR-V code.
//
// * The assignment ignores storage class (and thus layout) difference.
// * Associated counters for certain types are not assigned together.
//
// For the first, we will need SPIRV-Tools opt to legalize it.
// For the second, it is a TODO.

struct T {
    float f;
};

struct ResourceBundle {
    SamplerState               sampl;
    Buffer<float3>             buf;
    RWBuffer<float3>           rwbuf;
    Texture2D<float>           tex2d;
    RWTexture2D<float4>        rwtex2d;
    StructuredBuffer<T>        sbuf;
    RWStructuredBuffer<T>      rwsbuf;
    AppendStructuredBuffer<T>  asbuf;
    ConsumeStructuredBuffer<T> csbuf;
    ByteAddressBuffer          babuf;
    RWByteAddressBuffer        rwbabuf;
};

SamplerState               g_sampl;
Buffer<float3>             g_buf;
RWBuffer<float3>           g_rwbuf;
Texture2D<float>           g_tex2d;
RWTexture2D<float4>        g_rwtex2d;
StructuredBuffer<T>        g_sbuf;
RWStructuredBuffer<T>      g_rwsbuf;
AppendStructuredBuffer<T>  g_asbuf;
ConsumeStructuredBuffer<T> g_csbuf;
ByteAddressBuffer          g_babuf;
RWByteAddressBuffer        g_rwbabuf;

void main() {
    ResourceBundle b;

// CHECK:      [[val:%\d+]] = OpLoad %type_sampler %g_sampl
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_sampler %b %int_0
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    b.sampl   = g_sampl;

// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_buffer_image %g_buf
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_buffer_image %b %int_1
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    b.buf     = g_buf;

// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_buffer_image_0 %g_rwbuf
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_buffer_image_0 %b %int_2
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    b.rwbuf   = g_rwbuf;

// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_2d_image %g_tex2d
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_2d_image %b %int_3
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    b.tex2d   = g_tex2d;

// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_2d_image_0 %g_rwtex2d
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_2d_image_0 %b %int_4
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    b.rwtex2d = g_rwtex2d;

// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_StructuredBuffer_T %g_sbuf
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_StructuredBuffer_T_0 %b %int_5
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    b.sbuf    = g_sbuf;

// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_RWStructuredBuffer_T %g_rwsbuf
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_RWStructuredBuffer_T_0 %b %int_6
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    b.rwsbuf  = g_rwsbuf;

// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_RWStructuredBuffer_T %g_asbuf
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_RWStructuredBuffer_T_0 %b %int_7
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    b.asbuf   = g_asbuf;

// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_RWStructuredBuffer_T %g_csbuf
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_RWStructuredBuffer_T_0 %b %int_8
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    b.csbuf   = g_csbuf;

// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_ByteAddressBuffer %g_babuf
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_ByteAddressBuffer %b %int_9
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    b.babuf   = g_babuf;

// CHECK-NEXT: [[val:%\d+]] = OpLoad %type_RWByteAddressBuffer %g_rwbabuf
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_type_RWByteAddressBuffer %b %int_10
// CHECK-NEXT:                OpStore [[ptr]] [[val]]
    b.rwbabuf = g_rwbabuf;
}
