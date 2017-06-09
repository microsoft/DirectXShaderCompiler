// RUN: %dxc -E not_main -T ps_6_0 %s | FileCheck %s

// Make sure internal global is removed.
// CHECK-NOT:  = internal

static float2x2 a;

float2x2 b;

float d;

// Not use main as entry name to disable GlobalOpt.
float4 not_main() : SV_TARGET
{
  a = b+2;
  float c = 0;
  // Make big number of instructions to disable gvn.
  [unroll]
  for (uint i=0;i<100;i++) {
    c += sin(i+d);
  }

  return a - c;
}