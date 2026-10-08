// RUN: not %dxc -T ps_6_7 -fcgl %s -spirv -enable-16bit-types 2>&1 | FileCheck %s

// CHECK: error: The sampled type for textures cannot be a floating point type smaller than 32-bits when targeting a Vulkan environment.
vk::SampledTexture2D<half4> tex2d;

void main() : SV_Target {}
