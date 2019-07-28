// Run: %dxc -T ps_6_0 -E main -Zi

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.entry.hlsl

// CHECK:                          OpLine [[file]] 29 1
// CHECK-NEXT:             %main = OpFunction %void None %16
// CHECK-NEXT:          {{%\d+}} = OpLabel
// CHECK-NEXT:                     OpLine [[file]] 29 20
// CHECK-NEXT:      %param_var_a = OpVariable %_ptr_Function_v2float Function
// CHECK-NEXT:                     OpLine [[file]] 30 20
// CHECK-NEXT:      %param_var_b = OpVariable %_ptr_Function_v3float Function
// CHECK-NEXT:                     OpLine [[file]] 31 20
// CHECK-NEXT:      %param_var_c = OpVariable %_ptr_Function_v4float Function
// CHECK-NEXT:                     OpLine [[file]] 29 20
// CHECK-NEXT: [[texcoord:%\d+]] = OpLoad %v2float %in_var_TEXCOORD0
// CHECK-NEXT:                     OpStore %param_var_a [[texcoord]]
// CHECK-NEXT:                     OpLine [[file]] 30 20
// CHECK-NEXT:   [[normal:%\d+]] = OpLoad %v3float %in_var_NORMAL
// CHECK-NEXT:                     OpStore %param_var_b [[normal]]
// CHECK-NEXT:                     OpLine [[file]] 31 20
// CHECK-NEXT:    [[color:%\d+]] = OpLoad %v4float %in_var_COLOR
// CHECK-NEXT:                     OpStore %param_var_c [[color]]
// CHECK-NEXT:                     OpLine [[file]] 29 1
// CHECK-NEXT:   [[result:%\d+]] = OpFunctionCall %v4float %src_main %param_var_a %param_var_b %param_var_c
// CHECK-NEXT:                     OpLine [[file]] 31 33
// CHECK-NEXT:                     OpStore %out_var_SV_Target [[result]]
// CHECK-NEXT:                     OpReturn
float4 main(float2 a : TEXCOORD0,
            float3 b : NORMAL,
            float4 c : COLOR) : SV_Target {
// CHECK:                  OpLine [[file]] 29 1
// CHECK-NEXT: %src_main = OpFunction %v4float None %29
// CHECK-NEXT:             OpLine [[file]] 29 20
// CHECK-NEXT:        %a = OpFunctionParameter %_ptr_Function_v2float
// CHECK-NEXT:             OpLine [[file]] 30 20
// CHECK-NEXT:        %b = OpFunctionParameter %_ptr_Function_v3float
// CHECK-NEXT:             OpLine [[file]] 31 20
// CHECK-NEXT:        %c = OpFunctionParameter %_ptr_Function_v4float
  float4 d = float4(a, b.xy + c.zw);
  return d;
}
