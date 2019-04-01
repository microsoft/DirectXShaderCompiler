// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// inc(color) will add 1 to color.
// Make sure result is add 5.2.
// CHECK: 0x4014CCCCC0000000

bool inc(inout float4 v) { v++; return false; }

float4 main(int val: A, float4 color: COLOR) : SV_TARGET {
  if (false || inc(color)) return color;

  return color + 4.2;
}

