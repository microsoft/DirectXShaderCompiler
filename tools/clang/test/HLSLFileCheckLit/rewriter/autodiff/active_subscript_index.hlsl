// RUN: %dxr -generate-differentials %s | FileCheck %s

// A data-dependent subscript is discontinuous with respect to its index. The
// typed IR must reject it rather than silently treating the index as inactive.

// CHECK: float f(inout GradientContext<uint> context, Variable<uint> index, float __dxc_ad_seed)
// CHECK: _Static_assert(false, "auto-diff cannot generate backward-mode for 'f': active subscript index in typed auto-diff IR");

static const float2 values = float2(1.0, 2.0);

[[dxc::AutoDiff(bwd)]]
float f(uint index) {
  return values[index];
}
