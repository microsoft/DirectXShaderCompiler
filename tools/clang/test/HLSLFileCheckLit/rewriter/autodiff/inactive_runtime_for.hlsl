// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/inactive_runtime_for_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Runtime loops over entirely inactive state replay in the primal before the
// active expression is differentiated. No reverse iteration tape is needed.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> value, uint count, float factor)
// CHECK: for (uint iteration = 0; iteration < count; ++iteration)
// CHECK: factor *= 2.F;
// CHECK: return (value * Value<float>::CreateValue(factor));
// CHECK: for (uint iteration = 0; iteration < count; ++iteration)
// CHECK: factor *= 2.F;
// CHECK: return compute_gradients_seeded(context, multiply<float>(value_expr, factor), __dxc_ad_seed);

// At count=3, factor grows from 1 to 8. Value 2 and seed 3 produce primal 16
// and gradient 24.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.600000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.400000e+01

[[dxc::AutoDiff(fwd, bwd)]]
float f(float value, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < count; ++iteration)
    factor *= 2.0f;
  return value * factor;
}
