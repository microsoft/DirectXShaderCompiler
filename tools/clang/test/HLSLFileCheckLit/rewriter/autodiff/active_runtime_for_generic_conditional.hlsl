// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_generic_conditional_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// An inactive condition selects an active state expression without requiring
// primal tapes. Reverse replay gates each arm's cotangent by that condition.

// CHECK: y.value = (choose ? x.value : -(x.value));
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (choose ? __dxc_ad_loop_0_update_1_adjoint : (float2)0);
// CHECK: __dxc_ad_loop_0_state_0_adjoint += -((choose ? (float2)0 : __dxc_ad_loop_0_update_1_adjoint));

// The true arm returns (18,24) with x/y gradients (4,6) and z gradient (4,6).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.800000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.400000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 4, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 5, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 6, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 7, i32 0, float 6.000000e+00

// The false arm cancels x from the result, leaving zero x/y gradients.
// EXEC: rawBufferStore.f32{{.*}}i32 8, i32 0, float 1.000000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 9, i32 0, float 1.200000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 10, i32 0, float 0.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 11, i32 0, float 0.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 12, i32 0, float 0.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 13, i32 0, float 0.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 14, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 15, i32 0, float 6.000000e+00

[[dxc::AutoDiff(bwd)]]
float2 f(float2 x, float2 y, float2 z, [[dxc::NoDiff]] uint count,
         [[dxc::NoDiff]] bool choose, [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y = choose ? x : -x;
    z *= factor;
  }
  return x + y + z;
}
