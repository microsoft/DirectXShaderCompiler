// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/typed_pullback_abi_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Inactive parameters retain their heterogeneous primal types and allocate no
// gradient slots. The active vector parameter uses its own type, and the
// explicit vector result seed becomes its cotangent.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float3> f(float2 uv, float4 screenPosition, Value<float3> value)
// CHECK: return value;
// CHECK: } } } // namespace user::ad::fwd

// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: float3 f(inout GradientContext<float3> context, float2 uv, float4 screenPosition, Variable<float3> value, float3 __dxc_ad_seed)
// CHECK-NOT: uv_expr
// CHECK-NOT: screenPosition_expr
// CHECK: VariableExpr<float3> value_expr = makeVariableExpr<float3>(value);
// CHECK: return compute_gradients_seeded(context, value_expr, __dxc_ad_seed);
// CHECK: } } } // namespace user::ad::bwd

// The primal is (1,2,3), and the identity pullback copies seed (4,5,6).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 2.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 3.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 4, i32 0, float 5.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 5, i32 0, float 6.000000e+00

[[dxc::AutoDiff(fwd, bwd)]]
float3 f([[dxc::NoDiff]] float2 uv,
         [[dxc::NoDiff]] float4 screenPosition, float3 value) {
  return value;
}
