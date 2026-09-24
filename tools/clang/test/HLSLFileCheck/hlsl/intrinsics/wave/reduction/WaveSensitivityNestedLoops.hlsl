// RUN: %dxc -T lib_6_3 %s | FileCheck %s -input-file=stderr

// Test the wave sensitivity analysis with nested loops.

// CHECK-NOT: warning

export
float nested(float p : P, uint n : N) : SV_Target {
  float acc = WaveActiveSum(p);
  for (uint i = 0; i < n; i++)
    for (uint j = 0; j < n; j++)
      acc += ddx(p);
  return acc;
}

export
float nested_dependent(float p : P, uint n : N) : SV_Target {
  float acc = WaveActiveSum(p);
  for (uint i = 0; i < n; i++)
    for (uint j = 0; j < i; j++)
      acc += ddx(p);
  return acc;
}

export
float triple_nested(float p : P, uint n : N) : SV_Target {
  float acc = WaveActiveSum(p);
  for (uint i = 0; i < n; i++)
    for (uint j = 0; j < n; j++)
      for (uint k = 0; k < j; k++)
        acc += ddx(p);
  return acc;
}

// The gradient operand becomes wave sensitive on the second outer iteration,
// so a warning is expected.
// CHECK: 45:14: warning: Gradient operations are not affected by wave-sensitive data or control flow
// CHECK-NOT: warning
export
float nested_sensitive(float p : P, uint n : N) : SV_Target {
  float acc = 0;
  float q = p;
  for (uint i = 0; i < n; i++) {
    for (uint j = 0; j < n; j++)
      acc += ddx(q);
    q = WaveActiveSum(q);
  }
  return acc;
}
