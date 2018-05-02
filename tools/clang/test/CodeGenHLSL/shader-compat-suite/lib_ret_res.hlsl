// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// Make sure handle store not unpack.
// CHECK: store %struct.SamplerState {{.*}}, %struct.SamplerState*

SamplerState    g_samLinear;

SamplerState GetSampler () {
  return g_samLinear;
}