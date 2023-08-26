// RUN: not %dxc -E main -T ps_6_0 %s 2>&1 | FileCheck %s

// CHECK: Instructions should not read uninitialized value

float main(snorm float b : B) : SV_DEPTH
{
  float a;
  return b + a;
}