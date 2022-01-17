// RUN: %dxc -T ps_6_0 -E main -spirv


// CHECK: OpExecutionModeId {{%\w+}} LocalSizeId %uint_8 %uint_8 %uint_8

int main() : SV_Target0 {
  
  vk::ext_execution_mode_id(/*LocalSizeId*/38, 8, 8, 8);
  return 3;
}
