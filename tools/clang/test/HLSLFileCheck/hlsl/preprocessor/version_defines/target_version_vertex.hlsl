// RUN: %dxc -O0 -T vs_6_0 %s | FileCheck %s
// CHECK: fadd
// CHECK: fadd
// CHECK: fadd

float4 main(float4 pos : IN) : OUT
{
  float x = pos.x;
  float4 outPos = pos;
#if defined(__SHADER_TARGET_STAGE) && __SHADER_TARGET_STAGE == __SHADER_STAGE_VERTEX
  x += 1;
#else
  x -= 1;
#endif
// Compiler upgrades to 6.0 if less
#if defined(__SHADER_TARGET_MAJOR) && __SHADER_TARGET_MAJOR == 6
  x += 1;
#else
  x -= 1;
#endif
// Compiler upgrades to 6.0 if less
#if defined(__SHADER_TARGET_MINOR) && __SHADER_TARGET_MINOR == 0
  x += 1;
#else
  x -= 1;
#endif
  outPos.x = x;
  return outPos;
}
