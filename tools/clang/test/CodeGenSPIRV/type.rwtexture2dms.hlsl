// RUN: %dxc -T cs_6_7 -E main -fcgl  %s -spirv | FileCheck %s --implicit-check-not="OpCapability ImageMSArray"

// A non-arrayed multisampled storage image needs StorageImageMultisample and
// nothing else. ImageMSArray belongs to the arrayed form alone, which is why
// this file declares no arrayed type: the capability is module-global, so a
// module holding both could not tell the two requirements apart. See
// type.rwtexture2dmsarray.hlsl for that side.
// CHECK: OpCapability StorageImageMultisample

// OpTypeImage operands: sampled-type Dim Depth Arrayed MS Sampled Format.
// MS=1 with Sampled=2 distinguishes these from the read-only Texture2DMS,
// which uses Sampled=1.
// CHECK: %type_2d_image = OpTypeImage %float 2D 2 0 1 2 Rgba32f
// CHECK: %_ptr_UniformConstant_type_2d_image = OpTypePointer UniformConstant %type_2d_image
// CHECK: %type_2d_image_0 = OpTypeImage %uint 2D 2 0 1 2 R32ui
// CHECK: %_ptr_UniformConstant_type_2d_image_0 = OpTypePointer UniformConstant %type_2d_image_0
// CHECK: %type_2d_image_1 = OpTypeImage %float 2D 2 0 1 2 R32f
// CHECK: %_ptr_UniformConstant_type_2d_image_1 = OpTypePointer UniformConstant %type_2d_image_1

// CHECK: %t1 = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
RWTexture2DMS<float4> t1 : register(u1);
// The second template argument is the sample count. SPIR-V has no operand for
// it, so it is accepted and dropped.
// CHECK: %t2 = OpVariable %_ptr_UniformConstant_type_2d_image_0 UniformConstant
RWTexture2DMS<uint, 8> t2 : register(u2);
// CHECK: %t3 = OpVariable %_ptr_UniformConstant_type_2d_image_1 UniformConstant
RWTexture2DMS<float> t3 : register(u3);

[numthreads(1, 1, 1)]
void main() {
}
