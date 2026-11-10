// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_generic_subscript_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Inactive runtime indices reconstruct scalar loop-state projections and
// scatter each constructor cotangent back into the selected vector lane.

// CHECK: x.value += y.value;
// CHECK: y.value = float2(x.value[first], x.value[second]);
// CHECK: __dxc_ad_loop_0_state_0_adjoint += float2((first == 0 ? __dxc_ad_loop_0_update_1_adjoint.x : 0.0f), (first == 1 ? __dxc_ad_loop_0_update_1_adjoint.x : 0.0f));
// CHECK: __dxc_ad_loop_0_state_0_adjoint += float2((second == 0 ? __dxc_ad_loop_0_update_1_adjoint.y : 0.0f), (second == 1 ? __dxc_ad_loop_0_update_1_adjoint.y : 0.0f));

// With indices (1,0), one iteration returns (20,22). Seed (2,3) produces x
// and y gradients (5,5), and z gradient (4,6).
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
         [[dxc::NoDiff]] uint first, [[dxc::NoDiff]] uint second,
         [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y = float2(x[first], x[second]);
    z *= factor;
  }
  return x + y + z;
}
