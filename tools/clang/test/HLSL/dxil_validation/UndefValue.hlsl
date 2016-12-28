// RUN: %dxc -E main -T ps_5_0 %s | FileCheck %s

// CHECK: @main

float main(snorm float b : B) : SV_DEPTH
{
  float a;
  return b + a;
}