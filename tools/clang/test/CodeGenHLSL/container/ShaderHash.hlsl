// RUN: %dxc -E main -T vs_6_0 %s -Zsb | FileCheck %s

// CHECK: ; shader hash: 9f3f8b8ebb42a9707866c758265145c5

float4 main() : SV_Position {
  return 1.0;
}
