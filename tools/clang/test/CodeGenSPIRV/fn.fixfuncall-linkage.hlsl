// RUN: %dxc -T lib_6_3 -fspv-target-env=universal1.5 -fspv-fix-func-call-arguments -O0

// CHECK: OpCapability Shader
// CHECK: OpCapability Linkage
RWStructuredBuffer< float4 > output : register(u1);

// CHECK: OpDecorate %main LinkageAttributes "main" Export
// CHECK: %main = OpFunction %int None
// CHECK: [[s39:%\w+]] = OpVariable %_ptr_Function_int Function
// CHECK: [[s36:%\w+]] = OpVariable %_ptr_Function_float Function
// CHECK: [[s33:%\w+]] = OpAccessChain %_ptr_StorageBuffer_float {{%\w+}} %int_0
// CHECK: [[s34:%\w+]] = OpAccessChain %_ptr_Function_int %stru %int_1
// CHECK: [[s37:%\w+]] = OpLoad %float [[s33]]
// CHECK:                OpStore [[s36]] [[s37]]
// CHECK: [[s40:%\w+]] = OpLoad %int [[s34]]
// CHECK:                OpStore [[s39]] [[s40]]
// CHECK: {{%\w+}} = OpFunctionCall %void %func [[s36]] [[s39]]
// CHECK: [[s41:%\w+]] = OpLoad %int [[s39]]
// CHECK:                OpStore [[s34]] [[s41]]
// CHECK: [[s38:%\w+]] = OpLoad %float [[s36]]
// CHECK:                OpStore [[s33]] [[s38]]

[noinline]
void func(inout float f0, inout int f1) {
  
}

struct Stru {
  int x;
  int y;
};
         
export int main(inout float4 color) {
  output[0] = color;
  Stru stru;
  func(output[0].x, stru.y);
  return 1;
}
