// RUN: %dxc -T lib_6_3 %s | FileCheck -input-file=stderr %s

// CHECK:error: static global resource use is disallowed for library functions. Value: global_texture
// CHECK:error: static global resource use is disallowed for library functions. Value: global_ss

SamplerState ss;
Texture2D<float4> t;

static SamplerState global_ss = ss;
static Texture2D<float4> global_texture = t;

export float4 test() {
  return global_texture.Sample(global_ss, 0.);
}