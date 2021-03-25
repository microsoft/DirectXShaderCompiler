// Run: %dxc -T ps_6_0 -E main

// CHECK: error: identifier is unsupported in HLSL

float4 main(float4 color : COLOR, float vface : VFACE) : SV_TARGET
{
  return (vface > 0) ? color : (color*2);
}
