// RUN: %dxc -T ps_6_0 -E main

// CHECK: warning: Ignoring unsupported 'VFACE' in the target attribute string

float4 main(float4 color : COLOR, float vface : VFACE) : SV_TARGET
{
  return (vface > 0) ? color : (color*2);
}
