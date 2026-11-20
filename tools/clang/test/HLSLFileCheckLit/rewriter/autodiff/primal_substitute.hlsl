// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/primal_substitute_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// CHECK-DAG: PrimalSubstituteOf
// CHECK: float __dxc_ad_primal = ::reference(x.value);
// CHECK: ::user::ad::bwd::reference(__dxc_ad_call_0_context_0, __dxc_ad_call_0_arg_0, __dxc_ad_seed);
// CHECK: float __dxc_ad_primal = ::reference_custom(x.value);
// CHECK: ::bwd_reference_custom(x.value, __dxc_ad_seed, __dxc_ad_custom_call_0_arg_0_adjoint);

// The rendering primal returns 100+x. The differential replays x*x and its
// generated pullback. At x=2 and seed=3, these produce 102, 4, and 12.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.020000e+02
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 1.200000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 1.020000e+02
// EXEC: rawBufferStore.f32{{.*}}i32 4, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 5, i32 0, float 1.500000e+01

float original(float x) { return 100.0f + x; }

[[dxc::PrimalSubstituteOf(original)]]
[[dxc::AutoDiff(bwd)]]
float reference(float x) { return x * x; }

[[dxc::AutoDiff(bwd)]]
float f(float x) { return original(x); }

float original_custom(float x) { return 100.0f + x; }

void bwd_reference_custom(float x, float dResult, out float dX) {
	dX = 5.0f * dResult;
}

[[dxc::PrimalSubstituteOf(original_custom)]]
[[dxc::BackwardDerivative(bwd_reference_custom)]]
float reference_custom(float x) { return x * x; }

[[dxc::AutoDiff(bwd)]]
float f_custom(float x) { return original_custom(x); }
