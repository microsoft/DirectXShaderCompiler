// RUN: %dxr -generate-differentials %s | FileCheck %s
//
// Compile the rewritten output and confirm that the generated _Static_assert
// fires. The fallback return also diagnoses its empty initializer.
//
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: not %dxc -T ps_6_9 -HV 2021 %t.gen.hlsl 2>&1 | FileCheck %s --check-prefix=DIAG

// DIAG: error: static_assert failed "auto-diff cannot generate forward-mode for 'use_cmp': the ternary ?: operator is not differentiable"
// DIAG: error: 'Value<float>' cannot have an explicit empty initializer

// Forward-mode counterpart to nondiff_ternary.hlsl: the ternary ?: is not
// differentiable, so the forward-mode generated function is replaced by a
// _Static_assert stub with a human-readable reason.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> use_cmp(Value<float> x, Value<float> y)
// CHECK: _Static_assert(false, "auto-diff cannot generate forward-mode for 'use_cmp': the ternary ?: operator is not differentiable");
// CHECK: } } } // namespace user::ad::fwd

[[dxc::AutoDiff(fwd)]]
float use_cmp(float x, float y) {
  return (x < y) ? x : y;
}

float main(float x : A) : SV_Target { return use_cmp(x, x); }
