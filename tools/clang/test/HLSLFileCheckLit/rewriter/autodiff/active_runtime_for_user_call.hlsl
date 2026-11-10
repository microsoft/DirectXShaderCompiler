// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// CHECK: __dxc_ad_loop_0_primal_tape[3]
// CHECK: __dxc_ad_loop_0_primal_tape[iteration] = value.value
// CHECK: value.value = ::g(value.value)
// CHECK: GradientContext<float> __dxc_ad_call_0_context_0
// CHECK: Variable<float> __dxc_ad_call_0_arg_0 = variable(__dxc_ad_call_0_context_0, __dxc_ad_loop_0_primal_tape[iteration])
// CHECK: ::user::ad::bwd::g(__dxc_ad_call_0_context_0, __dxc_ad_call_0_arg_0, __dxc_ad_loop_0_update_0_adjoint)
// CHECK: __dxc_ad_loop_0_state_0_adjoint += __dxc_ad_call_0_arg_0.gradient(__dxc_ad_call_0_context_0)

// Three squarings map 2 to 256. The derivative is 1024, so seed 3 yields 3072.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 2.560000e+02
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 3.072000e+03

[[dxc::AutoDiff(bwd)]]
float g(float x) { return x * x; }

[[dxc::AutoDiff(bwd)]]
float f(float value, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < min(count, 3); ++iteration)
    value = g(value);
  return value;
}
