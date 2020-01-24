// RUN: %dxc -O0 -T ps_6_0 %s | FileCheck %s
// CHECK: fadd
// CHECK: fadd
// CHECK: fadd

float4 main(float4 color : A) : SV_Target
{
  float red = color.r;
  float4 outColor = color;
#if defined(__SHADER_TARGET_STAGE) && __SHADER_TARGET_STAGE == __SHADER_STAGE_PIXEL
  red += 1;
#else
  red -= 1;
#endif
#if defined(__SHADER_TARGET_MAJOR) && __SHADER_TARGET_MAJOR == 6
  red += 1;
#else
  red -= 1;
#endif
#if defined(__SHADER_TARGET_MINOR) && __SHADER_TARGET_MINOR == 0
  red += 1;
#else
  red -= 1;
#endif
  outColor.r = red;
  return outColor;
}
