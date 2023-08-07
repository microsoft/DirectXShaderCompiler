// RUN: %dxc -T ps_6_0 -E main -spirv

// CHECK-NOT: OpCapability ShaderClockKHR
// CHECK-NOT: OpExtension "SPV_KHR_shader_clock"
// CHECK: OpExecutionModeId {{%\w+}} LocalSizeId %uint_8 %uint_8 %uint_8
// CHECK: OpExecutionModeId {{%\w+}} LocalSizeHintId %uint_4 %uint_4 %uint_4

int main() : SV_Target0 {
  vk::ext_execution_mode_id(/*LocalSizeId*/38, 8, 8, 8);
  vk::ext_execution_mode_id(/*LocalSizeHintId*/39, 4, 4, 4);
  return 3;
}
