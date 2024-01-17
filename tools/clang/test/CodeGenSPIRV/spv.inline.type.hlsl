// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// TODO(5751): enable Array test when `using` has been implemented
// CHECK-TODO: %type_Array_type_2d_image = OpTypeArray %type_2d_image
// template<typename SomeType>
// using Array = vk::SpirvOpaqueType</* OpTypeArray */ 28, SomeType, 4>;

// CHECK: %spirvIntrinsicType = OpTypeArray %type_2d_image %int_4
typedef vk::SpirvOpaqueType</* OpTypeArray */ 28, Texture2D, 4> ArrayTex2D;

// CHECK: %spirvIntrinsicType_0 = OpTypeInt 8 0
typedef vk::SpirvOpaqueType</* OpTypeInt */ 21, vk::ext_literal(8), vk::ext_literal(false)> uint8_t;

// CHECK: %_arr_spirvIntrinsicType_0_uint_4 = OpTypeArray %spirvIntrinsicType_0 %uint_4

[[vk::ext_capability(/* Int8 */ 39)]]
void main() {
  // CHECK: %image = OpVariable %_ptr_Function_spirvIntrinsicType Function
  // Array<Texture2D> image;
  ArrayTex2D image;

  // %byte = OpVariable %_ptr_Function_spirvIntrinsicType_0
  uint8_t byte;

  // Check that uses of the same type use the same SPIR-V type definition.
  // %byte1 = OpVariable %_ptr_Function_spirvIntrinsicType_0
  uint8_t byte1;

  // %bytes = OpVariable %_ptr_Function__arr_spirvIntrinsicType_1_uint_4
  uint8_t bytes[4];
}
