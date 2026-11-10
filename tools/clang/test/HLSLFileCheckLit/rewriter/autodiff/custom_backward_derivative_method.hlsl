// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/custom_backward_derivative_method_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// CHECK: this.bwd_apply(x.value, __dxc_ad_seed, __dxc_ad_custom_call_0_arg_0_adjoint);

// At scale=4, x=2, and seed=3, the primal is 8. The custom pullback returns
// (scale+1)*seed, so the gradient is 15 rather than the mathematical 12.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 8.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.500000e+01

struct Custom {
  float scale;

  void bwd_apply(float x, float dResult, out float dX) {
    dX = (scale + 1.0f) * dResult;
  }

  [[dxc::BackwardDerivative(bwd_apply)]]
  float apply(float x) { return scale * x; }

  [[dxc::AutoDiff(bwd)]]
  float f(float x) { return apply(x); }
};
