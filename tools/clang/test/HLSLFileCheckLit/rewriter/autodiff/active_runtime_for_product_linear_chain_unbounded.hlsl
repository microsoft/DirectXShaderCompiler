// RUN: %dxr -generate-differentials %s | FileCheck %s

// Changing primal trajectories are reconstructed from initial checkpoints.

// CHECK: _initial =
// CHECK: _replay_index = 0;

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float scale, [[dxc::NoDiff]] float slope) {
  for (uint iteration = 0; iteration < count; ++iteration) {
    x = scale * x * y - slope * x;
    y += z;
    z *= 2.0f;
  }
  return x + y + z;
}
