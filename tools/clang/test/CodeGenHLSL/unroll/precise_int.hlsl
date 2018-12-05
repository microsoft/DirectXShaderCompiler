// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s
// CHECK: @main

float4 main() : SV_Target {
  precise uint result = 1;
  [unroll]
  for (precise uint i = 0; i < 4; i++) {
    result += 10;
  }
  return float4(result, 0,0, 1);
}


