// RUN: %dxr -generate-differentials %s | FileCheck %s

// Discrete integer operations may be replayed only when their operands are
// inactive; dependence on an active parameter must remain an explicit error.

// CHECK: _Static_assert(false, "auto-diff cannot generate backward-mode for 'f': active cast not represented in typed auto-diff IR");

[[dxc::AutoDiff(bwd)]]
float f(float value) {
  return (float)(((uint)value) & 3);
}
