// Run: %dxc -T ps_6_0 -E main

float4 main() : SV_Target
{
  float MulBy2(float f);

  // CHECK: OpFunctionCall %float %MulBy2 %param_var_f
  return float4(MulBy2(.25), 1, 0, 1);
}

// CHECK: %MulBy2 = OpFunction %float None
float MulBy2(float f)
{
  return f*2;
}

