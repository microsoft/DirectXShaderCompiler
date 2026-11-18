// RUN: %dxc -T ps_6_6 -E main -HV 202x %s | FileCheck %s

// Verify that const local resource objects can still be used through their
// instance methods (which are now properly marked const). A const handle only
// prevents reassigning the handle, so writing through a const RW resource is
// still allowed.

Texture2D<float4>      tex   : register(t0);
SamplerState           samp  : register(s0);
RWBuffer<float4>       buf   : register(u0);
ByteAddressBuffer      bab   : register(t1);
StructuredBuffer<int>  sb    : register(t2);

// CHECK: define void @main()
float4 main(float2 uv : TEXCOORD) : SV_Target {
  const Texture2D<float4>     ltex = tex;
  const SamplerState          lsamp = samp;
  const RWBuffer<float4>      lbuf = buf;
  const ByteAddressBuffer     lbab = bab;
  const StructuredBuffer<int> lsb = sb;

  // CHECK: call %dx.types.ResRet.f32 @dx.op.sample.f32(i32 60,
  float4 sampled = ltex.Sample(lsamp, uv);
  // CHECK: call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66,
  float4 loaded  = ltex.Load(int3(0, 0, 0));
  // CHECK: call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32(i32 68,
  float4 fromBuf = lbuf.Load(0);
  // CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, {{.*}}, i32 0, i32 undef,
  uint   raw     = lbab.Load(0);
  // CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, {{.*}}, i32 0, i32 0,
  int    si      = lsb.Load(0);

  // CHECK: call %dx.types.Dimensions @dx.op.getDimensions(i32 72,
  uint w, h, l;
  ltex.GetDimensions(0, w, h, l);

  // CHECK: call void @dx.op.bufferStore.f32(i32 69,
  lbuf[1] = sampled;

  return sampled + loaded + fromBuf + float4(raw, si, w, h);
}
