// RUN: %dxr -generate-differentials %s | FileCheck %s

// Direct recursion would recursively invoke the generated pullback without a
// call tape, so it is rejected before call composition.

// CHECK: _Static_assert(false, "auto-diff cannot generate backward-mode for 'f': recursive pullback composition is not supported");

[[dxc::AutoDiff(bwd)]]
float f(float value) {
  return value * f(value - 1.0f);
}
