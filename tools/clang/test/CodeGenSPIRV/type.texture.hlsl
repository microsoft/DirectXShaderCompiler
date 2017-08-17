// Run: %dxc -T vs_6_0 -E main

// CHECK: OpCapability Sampled1D

// CHECK: %type_1d_image = OpTypeImage %float 1D 0 0 0 1 Unknown
// CHECK: %_ptr_UniformConstant_type_1d_image = OpTypePointer UniformConstant %type_1d_image

// CHECK: %type_2d_image = OpTypeImage %int 2D 0 0 0 1 Unknown
// CHECK: %_ptr_UniformConstant_type_2d_image = OpTypePointer UniformConstant %type_2d_image

// CHECK: %type_3d_image = OpTypeImage %uint 3D 0 0 0 1 Unknown
// CHECK: %_ptr_UniformConstant_type_3d_image = OpTypePointer UniformConstant %type_3d_image

// CHECK: %type_cube_image = OpTypeImage %float Cube 0 0 0 1 Unknown
// CHECK: %_ptr_UniformConstant_type_cube_image = OpTypePointer UniformConstant %type_cube_image

// CHECK: %type_1d_image_array = OpTypeImage %float 1D 0 1 0 1 Unknown
// CHECK: %_ptr_UniformConstant_type_1d_image_array = OpTypePointer UniformConstant %type_1d_image_array

// CHECK: %type_2d_image_array = OpTypeImage %int 2D 0 1 0 1 Unknown
// CHECK: %_ptr_UniformConstant_type_2d_image_array = OpTypePointer UniformConstant %type_2d_image_array

// CHECK: %type_cube_image_array = OpTypeImage %float Cube 0 1 0 1 Unknown
// CHECK: %_ptr_UniformConstant_type_cube_image_array = OpTypePointer UniformConstant %type_cube_image_array

// CHECK: %t1 = OpVariable %_ptr_UniformConstant_type_1d_image UniformConstant
Texture1D   <float4> t1 : register(t1);
// CHECK: %t2 = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
Texture2D   <int4>   t2 : register(t2);
// CHECK: %t3 = OpVariable %_ptr_UniformConstant_type_3d_image UniformConstant
Texture3D   <uint4>  t3 : register(t3);
// CHECK: %t4 = OpVariable %_ptr_UniformConstant_type_cube_image UniformConstant
TextureCube <float4> t4 : register(t4);


// CHECK: %t5 = OpVariable %_ptr_UniformConstant_type_1d_image_array UniformConstant
Texture1DArray   <float4> t5 : register(t5);
// CHECK: %t6 = OpVariable %_ptr_UniformConstant_type_2d_image_array UniformConstant
Texture2DArray   <int4>   t6 : register(t6);
// CHECK: %t7 = OpVariable %_ptr_UniformConstant_type_cube_image_array UniformConstant
TextureCubeArray <float4> t7 : register(t7);

void main() {
// CHECK-LABEL: %main = OpFunction
}
