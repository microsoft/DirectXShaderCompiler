// RUN: %dxr -generate-differentials %s | FileCheck %s
//
// Compile the rewritten output and confirm that the generated _Static_assert
// fires. The forward fallback also diagnoses its empty initializer.
//
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: not %dxc -T ps_6_9 -HV 2021 %t.gen.hlsl 2>&1 | FileCheck %s --check-prefix=DIAG

// DIAG: error: static_assert failed "auto-diff cannot generate forward-mode for 'uses_asint': bit-cast 'asint' is not differentiable"
// DIAG: error: 'Value<float>' cannot have an explicit empty initializer
// DIAG: error: static_assert failed "auto-diff cannot generate backward-mode for 'uses_asint': bit-cast 'asint' is not differentiable"

// Non-differentiable intrinsics generate a _Static_assert stub with a
// human-readable reason rather than miscompiling silently.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> uses_asint(Value<float> x)
// CHECK: _Static_assert(false, "auto-diff cannot generate forward-mode for 'uses_asint': bit-cast 'asint' is not differentiable");
// CHECK: } } } // namespace user::ad::fwd

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float uses_asint(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: _Static_assert(false, "auto-diff cannot generate backward-mode for 'uses_asint': bit-cast 'asint' is not differentiable");
// CHECK: } } } // namespace user::ad::bwd

[[dxc::AutoDiff(fwd, bwd)]]
float uses_asint(float x) { return asint(x); }

float main(float x : A) : SV_Target { return uses_asint(x); }
