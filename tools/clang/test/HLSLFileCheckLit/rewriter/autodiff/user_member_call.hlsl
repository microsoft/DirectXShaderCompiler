// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/user_member_call_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

struct Functions {
  float scale;

  [[dxc::AutoDiff(bwd)]]
  float g(float value) {
    return value * scale;
  }

  [[dxc::AutoDiff(bwd)]]
  float f(float value) {
    return g(value) + value;
  }
};

// CHECK: float __dxc_ad_primal = (::Functions::g(value.value) + value.value);
// CHECK: this.g(__dxc_ad_call_0_context_0, __dxc_ad_call_0_arg_0, __dxc_ad_seed);
// CHECK: context.gradients[value.id] += __dxc_ad_call_0_arg_0.gradient(__dxc_ad_call_0_context_0);
// CHECK: context.gradients[value.id] += __dxc_ad_seed;

// At scale=4, value=2, and seed=3: primal=10 and gradient=15.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.000000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 1.500000e+01
