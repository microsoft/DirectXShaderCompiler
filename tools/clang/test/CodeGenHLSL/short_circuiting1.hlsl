// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// inc(color) will add 1 to color.
// Make sure result is add 1.
//CHECK: 1.0

bool inc(inout float4 v) { v++; return true; }

float4 main(int val: A, float4 color: COLOR) : SV_TARGET {
  if (true && inc(color)) return color;

  return color + 4.2;
}

