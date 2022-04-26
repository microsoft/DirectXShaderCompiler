// RUN: %dxc -T ps_6_0 -E PSMain -fspv-entrypoint-name=main

// CHECK: OpEntryPoint Fragment %PSMain "main" %in_var_COLOR %out_var_SV_TARGET
float4 PSMain(float4 color : COLOR) : SV_TARGET
{
    return color;
}
