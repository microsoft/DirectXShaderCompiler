// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_intrinsic_chain_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Sine and cosine target factors replay through the generic local pullback.

// CHECK: x.value = ((::sin(x.value) * y.value) + (::cos(x.value) * y.value));
// CHECK: float __dxc_ad_loop_0_update_0_adjoint = __dxc_ad_loop_0_state_0_adjoint;
// CHECK: __dxc_ad_loop_0_state_0_adjoint = (float)0;

// Starting at (0,2,3), one iteration produces (2,5,6), returning 13.
// With seed 2, reverse Jacobian replay produces gradients (4,4,6).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.300000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 6.000000e+00

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x = sin(x) * y + cos(x) * y;
    y += z;
    z *= factor;
  }
  return x + y + z;
}
