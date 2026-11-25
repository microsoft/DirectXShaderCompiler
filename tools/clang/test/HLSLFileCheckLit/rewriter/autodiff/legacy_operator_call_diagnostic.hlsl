// RUN: %dxr -generate-differentials %s | FileCheck %s

// The unsupported loop selects legacy fallback after the typed builder has
// visited a vector subscript. Overloaded operator names must remain printable
// in that fallback rather than asserting in NamedDecl::getName().

// CHECK: _Static_assert(false, "auto-diff cannot generate backward-mode for 'f': unknown callee 'operator[]' has no auto-diff builder");

[[dxc::AutoDiff(bwd)]]
float f(float2 value, [[dxc::NoDiff]] uint index) {
  float result = value[index];
  while (false)
    result += 1.0f;
  return result;
}
