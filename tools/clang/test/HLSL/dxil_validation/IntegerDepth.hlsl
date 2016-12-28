// RUN: %dxc -E main -T ps_5_0 %s | FileCheck %s

// CHECK: @main

int main(snorm float b : B, float c:C) : SV_DEPTH
{
  return b;
}