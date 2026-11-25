// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_generic_construct_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// A vector constructor splits its cotangent in constructor order before the
// scalar swizzles route those lanes back to the updated loop state.

// CHECK: x.value += y.value;
// CHECK: y.value = float2(x.value.y, x.value.x);
// CHECK: float2 __dxc_ad_loop_0_update_1_adjoint = __dxc_ad_loop_0_state_1_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint += float2(0.0f, __dxc_ad_loop_0_update_1_adjoint.x);
// CHECK: __dxc_ad_loop_0_state_0_adjoint += float2(__dxc_ad_loop_0_update_1_adjoint.y, 0.0f);

// One iteration returns (20,22). Seed (2,3) produces x and y gradients (5,5)
// and z gradient (4,6).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 2.000000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.200000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 5.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 5.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 4, i32 0, float 5.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 5, i32 0, float 5.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 6, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 7, i32 0, float 6.000000e+00

[[dxc::AutoDiff(bwd)]]
float2 f(float2 x, float2 y, float2 z, [[dxc::NoDiff]] uint count,
         [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y = float2(x.y, x.x);
    z *= factor;
  }
  return x + y + z;
}
