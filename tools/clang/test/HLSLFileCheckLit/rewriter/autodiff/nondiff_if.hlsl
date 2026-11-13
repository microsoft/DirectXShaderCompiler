// RUN: %dxr -generate-differentials %s | FileCheck %s
//
// Compile the rewritten output and confirm that the generated _Static_assert
// fires.
//
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: not %dxc -T ps_6_9 -HV 2021 %t.gen.hlsl 2>&1 | FileCheck %s --check-prefix=DIAG

// DIAG: error: static_assert failed "auto-diff cannot generate backward-mode for 'branchy': data-dependent control flow (if) is not differentiable

// Data-dependent control flow (if) is not soundly differentiable; the
// function body is replaced by a _Static_assert with a message pointing
// the user at [[NoDiff]] / branchless math.

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float branchy(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: _Static_assert(false, "auto-diff cannot generate backward-mode for 'branchy': data-dependent control flow (if) is not differentiable; use {{\[\[}}dxc::NoDiff]] or branchless math");
// CHECK: } } } // namespace user::ad::bwd

[[dxc::AutoDiff(bwd)]]
float branchy(float x) {
  if (x > 0)
    return x;
  return -x;
}

float main(float x : A) : SV_Target { return branchy(x); }
