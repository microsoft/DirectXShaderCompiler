// Run: %dxc -T ps_6_0 -E main -Zi

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.intrinsic.hlsl

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.intrinsic.hlsl

ConsumeStructuredBuffer<bool2> consume_v2bool;
AppendStructuredBuffer<float2> append_v2float;
ByteAddressBuffer byte_addr;
RWTexture2D<int3> rw_tex;
SamplerState sam;
Texture2D<float4> t2f4;
RWByteAddressBuffer rw_byte;

// Note that preprocessor prepends a "#line 1 ..." line to the whole file and
// the compliation sees line numbers incremented by 1.

void main() {
  float2 v2f;
  uint4 v3i;
  float2x2 m2x2f;

  append_v2float.Append(consume_v2bool.Consume());

  modf(v2f, v3i.xy);

  v3i.xyz = byte_addr.Load3(v3i.x);

  rw_tex.GetDimensions(v3i.x, v3i.y);

  byte_addr.GetDimensions(v3i.z);

  v3i = t2f4.GatherRed(sam, v2f, v3i.xy);

  v3i = msad4(v3i.x, v3i.xy, v3i);

  v3i = mad(m2x2f, float2x2(v3i), float2x2(v2f, v2f));

  v2f = mul(v2f, m2x2f);

  v2f.x = dot(v3i.xy, v2f);

  half h = f16tof32(v3i.x);

  m2x2f = ddx(m2x2f);

  m2x2f = fmod(m2x2f, float2x2(v3i));

  rw_byte.InterlockedAdd(16, 42, v2f.x);

  if (all(v2f))
    sincos(v3i.x, v3i.y, v3i.z);

  v2f = saturate(v2f);
}
