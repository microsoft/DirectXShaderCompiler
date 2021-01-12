// RUN: %dxc %s -E main -T ps_6_0 -Zi -Od -fcgl | FileCheck %s

// CHECK: @main

[RootSignature("")]
float4 main() : SV_Target {
  return float4(1,1,1,1);
};
