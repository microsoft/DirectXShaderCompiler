// RUN: %dxr -generate-differentials %s | FileCheck %s
//
// Compile the rewritten output and confirm that the generated _Static_assert
// fires. The fallback return also diagnoses its empty initializer.
//
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: not %dxc -T ps_6_9 -HV 2021 %t.gen.hlsl 2>&1 | FileCheck %s --check-prefix=DIAG

// DIAG: error: static_assert failed "auto-diff cannot generate forward-mode for 'branchy': data-dependent control flow (if) is not differentiable
// DIAG: error: 'Value<float>' cannot have an explicit empty initializer

// Forward-mode counterpart to nondiff_if.hlsl: data-dependent control flow
// (if) is not soundly differentiable; the forward-mode function body is
// replaced by a _Static_assert stub pointing the user at [[dxc::NoDiff]] or
// branchless math.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> branchy(Value<float> x)
// CHECK: _Static_assert(false, "auto-diff cannot generate forward-mode for 'branchy': data-dependent control flow (if) is not differentiable; use {{\[\[}}dxc::NoDiff]] or branchless math");
// CHECK: } } } // namespace user::ad::fwd

[[dxc::AutoDiff(fwd)]]
float branchy(float x) {
  if (x > 0)
    return x;
  return -x;
}

float main(float x : A) : SV_Target { return branchy(x); }
