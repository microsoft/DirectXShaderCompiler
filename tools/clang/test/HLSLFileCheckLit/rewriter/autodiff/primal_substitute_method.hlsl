// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/primal_substitute_method_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// CHECK: ::Functions::reference(x.value)
// CHECK: this.reference(__dxc_ad_call_0_context_0, __dxc_ad_call_0_arg_0, __dxc_ad_seed);

// At scale=4 and x=2, the original returns 106. The differential replays
// scale*x and propagates seed=3, producing a primal of 8 and a gradient of 12.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 1.060000e+02
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 8.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 1.200000e+01

struct Functions {
  float scale;

  float original(float x) { return 100.0f + scale + x; }

  [[dxc::PrimalSubstituteOf(original)]]
  [[dxc::AutoDiff(bwd)]]
  float reference(float x) { return scale * x; }

  [[dxc::AutoDiff(bwd)]]
  float f(float x) { return original(x); }
};
