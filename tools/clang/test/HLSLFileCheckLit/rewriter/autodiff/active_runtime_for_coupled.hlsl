// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_coupled_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// The sequential recurrence has Jacobian [[1,1],[0,2]]. Reverse replay applies
// its transpose once per iteration.

// CHECK: float __dxc_ad_loop_0_update_1_adjoint = __dxc_ad_loop_0_state_1_adjoint;
// CHECK: __dxc_ad_loop_0_state_1_adjoint = (float)0;
// CHECK: __dxc_ad_loop_0_state_1_adjoint += (__dxc_ad_loop_0_update_1_adjoint * 2.F);
// CHECK: __dxc_ad_loop_0_state_1_adjoint += __dxc_ad_loop_0_update_0_adjoint;
// CHECK: context.gradients[x.id] += __dxc_ad_loop_0_state_0_adjoint;
// CHECK: context.gradients[y.id] += __dxc_ad_loop_0_state_1_adjoint;

// Starting from (1,2), two iterations produce (7,8), so the result is 15.
// J^2=[[1,3],[0,4]], and seed 2 on x+y gives gradients (2,14).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.500000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 1.400000e+01

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < count; ++iteration) {
    x += y;
    y *= 2.0f;
  }
  return x + y;
}
