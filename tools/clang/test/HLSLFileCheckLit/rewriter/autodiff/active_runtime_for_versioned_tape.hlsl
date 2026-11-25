// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_versioned_tape_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// The second update consumes x version 1, produced earlier in the same
// iteration, while z still refers to version 0.

// CHECK: float __dxc_ad_loop_0_version_1_primal_tape[8];
// CHECK: x.value += y.value;
// CHECK: __dxc_ad_loop_0_version_1_primal_tape[iteration] = x.value;
// CHECK: y.value = (x.value * z.value);
// CHECK: float __dxc_ad_loop_0_update_1_adjoint = __dxc_ad_loop_0_state_1_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_1_adjoint * __dxc_ad_loop_2_primal_tape[iteration]);
// CHECK: __dxc_ad_loop_0_state_2_adjoint += (__dxc_ad_loop_0_update_1_adjoint * __dxc_ad_loop_0_version_1_primal_tape[iteration]);

// Starting at (1,2,3), two iterations produce (12,72,12), returning 96.
// With seed 2, generic reverse replay produces gradients (56,56,98).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 9.600000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 5.600000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 5.600000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 9.800000e+01

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y = x * z;
    z *= factor;
  }
  return x + y + z;
}
