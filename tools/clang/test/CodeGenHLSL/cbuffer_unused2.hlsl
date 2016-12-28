// RUN: %dxc -E main -T ps_5_0 %s

// Check used flag in reflection.

int a;
int b;

float4 main() : SV_Target {
  return b;
}
