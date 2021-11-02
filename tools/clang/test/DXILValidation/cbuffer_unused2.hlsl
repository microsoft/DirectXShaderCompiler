// RUN: %dxc -E main -T ps_6_0 %s
// TODO: No check lines found, we should update this

// Check used flag in reflection.

int a;
int b;

float4 main() : SV_Target {
  return b;
}
