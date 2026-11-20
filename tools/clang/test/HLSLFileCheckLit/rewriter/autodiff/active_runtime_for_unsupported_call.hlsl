// RUN: %dxr -generate-differentials %s | FileCheck %s

// CHECK: _Static_assert(false, "auto-diff cannot generate backward-mode for 'f': active runtime loop pullback does not support call 'abs'");

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x = abs(x);
    y += 1.0f;
  }
  return x + y;
}
