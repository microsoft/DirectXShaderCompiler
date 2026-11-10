// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/static_inactive_for_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Canonical constant-bound loops whose induction variable is not used in the
// body are unrolled into immutable expression bindings.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> value)
// CHECK: value = (value * Value<float>::CreateValue(2.F));
// CHECK: value = (value * Value<float>::CreateValue(2.F));
// CHECK: value = (value * Value<float>::CreateValue(2.F));
// CHECK: return value;
// CHECK: } } } // namespace user::ad::fwd
// CHECK: return compute_gradients_seeded(context, multiply<float>(multiply<float>(multiply<float>(value_expr, 2.F), 2.F), 2.F), __dxc_ad_seed);

// f(2)=16 and seed 3 scales df/dvalue=8 to gradient 24.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.600000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.400000e+01

[[dxc::AutoDiff(fwd, bwd)]]
float f(float value) {
  for (uint iteration = 0; iteration < 3; ++iteration)
    value *= 2.0f;
  return value;
}
