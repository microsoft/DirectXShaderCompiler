// RUN: %dxr -generate-differentials %s | FileCheck %s

// Changing primal trajectories are reconstructed from initial checkpoints.

// CHECK: _initial =
// CHECK: _replay_index = 0;

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float bias) {
  for (uint iteration = 0; iteration < count; ++iteration) {
    x = y * x - bias;
    y += z;
    z *= 2.0f;
  }
  return x + y + z;
}
