// RUN: %dxr -generate-differentials %s | FileCheck %s

// Generated user pullbacks can contain side effects, so an unbounded loop must
// not replay them without an explicit purity contract.

// CHECK: _Static_assert(false, "auto-diff cannot generate backward-mode for 'f': active runtime loop pullback requires a min(count, N) bound with N between 1 and 1024");

[[dxc::AutoDiff(bwd)]]
float g(float x) { return x * x; }

[[dxc::AutoDiff(bwd)]]
float f(float value, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < count; ++iteration)
    value = g(value);
  return value;
}
