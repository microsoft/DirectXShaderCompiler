// Run: %dxc -T vs_6_0 -E main

// Note: The following is invalid SPIR-V code.
//
// * Putting samplers and textures (of opaque types) in structs and loading/
//   storing them is not allowed in Vulkan.

struct T {
    float f;
};

struct ResourceBundle {
    SamplerState               sampl;
    Buffer<float3>             buf;
    RWBuffer<float3>           rwbuf;
    Texture2D<float>           tex2d;
    RWTexture2D<float4>        rwtex2d;
};

SamplerState               g_sampl;
Buffer<float3>             g_buf;
RWBuffer<float3>           g_rwbuf;
Texture2D<float>           g_tex2d;
RWTexture2D<float4>        g_rwtex2d;

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
}
