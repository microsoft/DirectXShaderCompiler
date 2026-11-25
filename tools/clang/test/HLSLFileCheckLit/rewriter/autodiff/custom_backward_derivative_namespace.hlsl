// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/custom_backward_derivative_namespace_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// CHECK: ::custom::bwd_apply(scale, x.value, __dxc_ad_seed, __dxc_ad_custom_call_0_arg_1_adjoint);

// At scale=5, x=2, and seed=3, the primal is 10 and the gradient is 15.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.000000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.500000e+01

namespace custom {
void bwd_apply(float scale, float x, float dResult, out float dX) {
  dX = scale * dResult;
}

[[dxc::BackwardDerivative(bwd_apply)]]
float apply([[dxc::NoDiff]] float scale, float x) { return scale * x; }
} // namespace custom

[[dxc::AutoDiff(bwd)]]
float f([[dxc::NoDiff]] float scale, float x) {
  return custom::apply(scale, x);
}
