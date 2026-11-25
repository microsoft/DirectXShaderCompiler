// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// An unbounded pure recurrence replays each primal prefix instead of allocating
// a runtime-sized local tape.

// CHECK: float __dxc_ad_loop_0_initial = value.value;
// CHECK-NOT: _primal_tape
// CHECK: for (uint __dxc_ad_loop_0_replay_index = 0;
// CHECK: __dxc_ad_loop_0_replay *= __dxc_ad_loop_0_replay;
// CHECK: float __dxc_ad_loop_0_version_0_replay = __dxc_ad_loop_0_replay;
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_0_adjoint * __dxc_ad_loop_0_version_0_replay);

// Three squarings map 2 to 256. The derivative is 1024, so seed 3 yields 3072.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 2.560000e+02
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 3.072000e+03

[[dxc::AutoDiff(bwd)]]
float f(float value, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < count; ++iteration)
    value *= value;
  return value;
}
