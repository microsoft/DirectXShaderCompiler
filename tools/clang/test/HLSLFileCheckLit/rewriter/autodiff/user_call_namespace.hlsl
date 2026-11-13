// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/user_call_namespace_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

namespace math {

[[dxc::AutoDiff(bwd)]]
float square(float value) {
  return value * value;
}

[[dxc::AutoDiff(bwd)]]
float f(float value) {
  return square(value) + value;
}

} // namespace math

// CHECK: namespace user { namespace ad { namespace bwd { namespace math {
// CHECK: float __dxc_ad_primal = (::math::square(value.value) + value.value);
// CHECK: square(__dxc_ad_call_0_context_0, __dxc_ad_call_0_arg_0, __dxc_ad_seed);
// CHECK: } } } } // namespace user::ad::bwd::math

// At value=2 and seed=3, primal=6 and gradient=15.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.500000e+01
