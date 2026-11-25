// RUN: %dxr -generate-differentials %s | FileCheck %s

float g(float value);

[[dxc::AutoDiff(bwd)]]
float f(float value) {
  return g(value);
}

[[dxc::AutoDiff(bwd)]]
float g(float value) {
  return f(value);
}

// A cycle across two canonical attributed declarations requires a call tape
// and is rejected before either generated pullback recursively invokes itself.

// CHECK-DAG: auto-diff cannot generate backward-mode for 'f': recursive pullback composition is not supported
// CHECK-DAG: auto-diff cannot generate backward-mode for 'g': recursive pullback composition is not supported
