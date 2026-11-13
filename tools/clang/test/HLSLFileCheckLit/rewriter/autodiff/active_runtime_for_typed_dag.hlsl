// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_versioned_tape_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// The assignment's typed update DAG contains a nested active sum. Its
// pullback reconstructs every state reference from the appropriate tape.

// CHECK: float __dxc_ad_loop_0_version_1_primal_tape[8];
// CHECK: y.value = ((x.value + z.value) * z.value);
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_1_adjoint * __dxc_ad_loop_2_primal_tape[iteration]);
// CHECK: __dxc_ad_loop_0_state_2_adjoint += (__dxc_ad_loop_0_update_1_adjoint * (__dxc_ad_loop_0_version_1_primal_tape[iteration] + __dxc_ad_loop_2_primal_tape[iteration]));

// Starting at (1,2,3), two iterations produce (21,162,12), returning 195.
// With seed 2, generic reverse replay produces gradients (56,56,266).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.950000e+02
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 5.600000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 5.600000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 2.660000e+02

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y = (x + z) * z;
    z *= factor;
  }
  return x + y + z;
}
