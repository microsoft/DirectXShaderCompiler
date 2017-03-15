// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s


float4x4 m;
uint i;
float4 main() : SV_POSITION {
  return m[2][i];
}
