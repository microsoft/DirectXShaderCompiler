// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-force-early-z | %FileCheck %s

// This shader uses discard, so we better not have turned on early z:
// CHECK-NOT: !{i32 0, i64 8}

[RootSignature("")]
float4 main() : SV_Target {
  discard;
  return float4(0,0,0,0);
}