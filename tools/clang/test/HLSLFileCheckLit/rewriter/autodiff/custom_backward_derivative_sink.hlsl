// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/custom_backward_derivative_sink_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// CHECK: ::bwd_source(index, __dxc_ad_seed);
// CHECK-NOT: __dxc_ad_custom_call_0_arg

// EXEC: rawBufferStore.f32{{.*}}float 3.000000e+00

RWStructuredBuffer<float> sink : register(u0);

void bwd_source(uint index, float dResult) { sink[index] = dResult; }

[[dxc::BackwardDerivative(bwd_source)]]
float source([[dxc::NoDiff]] uint index) { return 7.0f; }

[[dxc::AutoDiff(bwd)]]
float f([[dxc::NoDiff]] uint index) { return source(index); }
