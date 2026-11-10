// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_coupled_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Changing primal trajectories are reconstructed from initial checkpoints.

// CHECK: _initial =
// CHECK: _replay_index = 0;

// Two iterations map (1,2) to (6,4). Seed 2 yields gradients (12,12).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.000000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.200000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 1.200000e+01

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < count; ++iteration) {
    x *= y;
    y += 1.0f;
  }
  return x + y;
}
