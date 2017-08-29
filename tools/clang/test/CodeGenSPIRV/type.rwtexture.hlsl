// Run: %dxc -T vs_6_0 -E main

// CHECK: %type_1d_image = OpTypeImage %int 1D 0 0 0 2 R32i
// CHECK: %_ptr_UniformConstant_type_1d_image = OpTypePointer UniformConstant %type_1d_image
// CHECK: %type_2d_image = OpTypeImage %uint 2D 0 0 0 2 Rg32ui
// CHECK: %_ptr_UniformConstant_type_2d_image = OpTypePointer UniformConstant %type_2d_image
// CHECK: %type_3d_image = OpTypeImage %float 3D 0 0 0 2 Rgba32f
// CHECK: %_ptr_UniformConstant_type_3d_image = OpTypePointer UniformConstant %type_3d_image
// CHECK: %type_1d_image_array = OpTypeImage %int 1D 0 1 0 2 R32i
// CHECK: %_ptr_UniformConstant_type_1d_image_array = OpTypePointer UniformConstant %type_1d_image_array
// CHECK: %type_2d_image_array = OpTypeImage %uint 2D 0 1 0 2 Rg32ui
// CHECK: %_ptr_UniformConstant_type_2d_image_array = OpTypePointer UniformConstant %type_2d_image_array
// CHECK: %type_1d_image_array_0 = OpTypeImage %float 1D 0 1 0 2 Rgba32f
// CHECK: %_ptr_UniformConstant_type_1d_image_array_0 = OpTypePointer UniformConstant %type_1d_image_array_0
// CHECK: %type_2d_image_array_0 = OpTypeImage %float 2D 0 1 0 2 Rgba32f
// CHECK: %_ptr_UniformConstant_type_2d_image_array_0 = OpTypePointer UniformConstant %type_2d_image_array_0


// CHECK: %t1 = OpVariable %_ptr_UniformConstant_type_1d_image UniformConstant
RWTexture1D   <int>    t1 ;

// CHECK: %t2 = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
RWTexture2D   <uint2>  t2 ;

// CHECK: %t3 = OpVariable %_ptr_UniformConstant_type_3d_image UniformConstant
RWTexture3D   <float3> t3 ;

// CHECK: %t4 = OpVariable %_ptr_UniformConstant_type_3d_image UniformConstant
RWTexture3D   <float4> t4 ;

// CHECK: %t5 = OpVariable %_ptr_UniformConstant_type_1d_image_array UniformConstant
RWTexture1DArray<int>    t5;

// CHECK: %t6 = OpVariable %_ptr_UniformConstant_type_2d_image_array UniformConstant
RWTexture2DArray<uint2>  t6;

// CHECK: %t7 = OpVariable %_ptr_UniformConstant_type_1d_image_array_0 UniformConstant
RWTexture1DArray<float3> t7;

// CHECK: %t8 = OpVariable %_ptr_UniformConstant_type_2d_image_array_0 UniformConstant
RWTexture2DArray<float4> t8;

void main() {}
