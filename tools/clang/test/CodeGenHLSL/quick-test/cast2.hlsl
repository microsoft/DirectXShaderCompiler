// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: @main
float main(uint a : A) : SV_Target
{
  return a;
}
