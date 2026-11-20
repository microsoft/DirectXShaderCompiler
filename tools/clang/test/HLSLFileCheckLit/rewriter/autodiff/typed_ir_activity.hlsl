// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/typed_ir_activity_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// An inactive expression on the left of an active operation must be a forward
// Value constant and a backward primal value. Neither mode propagates its
// derivative.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x)
// CHECK: Value<float> inactive = sin(x);
// CHECK: return (Value<float>::CreateValue(inactive.value) + x);
// CHECK: } } } // namespace user::ad::fwd

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float f(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: return compute_gradients_seeded(context, add<float>(sin(x.value), x_expr), __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

// Both modes return sin(2)+2 and report only the derivative of the active x.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 0x4007463DC0000000
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 0x4007463DC0000000
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 1.000000e+00

[[dxc::AutoDiff(fwd, bwd)]]
float f(float x) {
  float inactive = sin(x);
  return [[dxc::NoDiff]] inactive + x;
}
