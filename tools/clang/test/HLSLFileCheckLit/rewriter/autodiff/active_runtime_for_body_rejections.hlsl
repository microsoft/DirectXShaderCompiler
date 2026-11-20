// RUN: %dxr -generate-differentials %s | FileCheck %s

// CHECK-DAG: auto-diff cannot generate backward-mode for 'control_flow': active runtime loop body does not support control flow
// CHECK-DAG: auto-diff cannot generate backward-mode for 'aliased_update': active runtime loop update target must be a direct active parameter
// CHECK-DAG: auto-diff cannot generate backward-mode for 'side_effect': active runtime loop body does not support side-effecting statements

[[dxc::AutoDiff(bwd)]]
float control_flow(float x, float y, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < count; ++iteration) {
    if (x > 0.0f)
      x += y;
    y += 1.0f;
  }
  return x + y;
}

[[dxc::AutoDiff(bwd)]]
float aliased_update(float2 x, float y, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < count; ++iteration) {
    x.x += y;
    y += 1.0f;
  }
  return x.x + y;
}

void mutate(inout float value) { value += 1.0f; }

[[dxc::AutoDiff(bwd)]]
float side_effect(float x, float y, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < count; ++iteration) {
    mutate(x);
    y += 1.0f;
  }
  return x + y;
}
