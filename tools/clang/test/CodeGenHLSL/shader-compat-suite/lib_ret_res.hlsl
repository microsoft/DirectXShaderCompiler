// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure handle store not unpack.
// CHECK: store %dx.types.Handle %g_samLinear_sampler, %dx.types.Handle*

SamplerState    g_samLinear;

SamplerState GetSampler () {
  return g_samLinear;
}