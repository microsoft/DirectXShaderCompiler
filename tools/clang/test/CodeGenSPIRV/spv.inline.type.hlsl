// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// CHECK: %spirvIntrinsicType = OpTypeArray %type_2d_image %uint_4
template<typename SomeType>
using Array = vk::SpirvOpaqueType</* OpTypeArray */ 28, SomeType, vk::integral_constant<uint, 4> >;

// CHECK: %spirvIntrinsicType_0 = OpTypeInt 8 0
using uint8_t [[vk::ext_capability(/* Int8 */ 39)]] = vk::SpirvType</* OpTypeInt */ 21, 8, 8, vk::Literal<vk::integral_constant<uint, 8> >, vk::Literal<vk::integral_constant<bool, false> > >;

// CHECK: %_arr_spirvIntrinsicType_0_uint_4 = OpTypeArray %spirvIntrinsicType_0 %uint_4

void main() {
  // CHECK: %image = OpVariable %_ptr_Function_spirvIntrinsicType Function
  Array<Texture2D> image;

  // CHECK: %byte = OpVariable %_ptr_Function_spirvIntrinsicType_0
  uint8_t byte;

  // Check that uses of the same type use the same SPIR-V type definition.
  // CHECK: %byte1 = OpVariable %_ptr_Function_spirvIntrinsicType_0
  uint8_t byte1;

  // CHECK: %bytes = OpVariable %_ptr_Function__arr_spirvIntrinsicType_0_uint_4
  uint8_t bytes[4];
}
