// RUN: %dxc -O0 -Gec -T ps_6_0 %s | FileCheck %s
// CHECK: fadd

float4 main(float4 color : A) : SV_Target
{
  float red = color.r;
  float4 outColor = color;
  // Default should be the highest available version
#if defined(__HLSL_VERSION) && __HLSL_VERSION == 2016
  red += 1;
#else
  red -= 1;
#endif
  outColor.r = red;
  return outColor;
}
