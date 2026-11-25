// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/inactive_integer_ops_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Inactive LOD and index arithmetic remains in the primal replay while the
// active floating-point path is differentiated normally.

// CHECK: float f(inout GradientContext<float> context, uint width, uint lod, Variable<float> value, float __dxc_ad_seed)
// CHECK: return compute_gradients_seeded(context, multiply<float>(value_expr, (float)((width >> lod) % 7)), __dxc_ad_seed);

// At width=32 and lod=2, the inactive factor is (32 >> 2) % 7 = 1.
// Value 3 and seed 4 therefore produce primal 3 and gradient 4.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 3.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 4.000000e+00

[[dxc::AutoDiff(bwd)]]
float f([[dxc::NoDiff]] uint width, [[dxc::NoDiff]] uint lod, float value) {
  width >>= lod;
  uint index = width % 7;
  return value * (float)index;
}
