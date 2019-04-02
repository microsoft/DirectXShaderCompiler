// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: @main
float main(int a : A) : SV_Target
{
  return a;
}
