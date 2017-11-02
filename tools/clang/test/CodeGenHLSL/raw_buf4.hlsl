// RUN: %dxc -E main -T ps_6_2 -no-min-precision %s | FileCheck %s

// CHECK: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 1)
// CHECK: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 3)
// CHECK: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 7)
// CHECK: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 15)

// CHECK: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 1)
// CHECK: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 3)
// CHECK: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 7)
// CHECK: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 15)

// CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 1)
// CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 3)
// CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 7)
// CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 15)

// CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 1)
// CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 3)
// CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 7)
// CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 15)

// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 3)
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 15)
// CHECK: call double @dx.op.makeDouble.f64

// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 3)
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 15)
// CHECK: call double @dx.op.makeDouble.f64

ByteAddressBuffer buf1;
RWByteAddressBuffer buf2;

float4 main(uint idx1 : IDX1, uint idx2 : IDX2) : SV_Target {
  uint status;
  float4 r = float4(0,0,0,0);

  r.x += buf1.LoadFloat(idx1, status);
  r.xy += buf1.LoadFloat2(idx1);
  r.xyz += buf1.LoadFloat3(idx1, status);
  r.xyzw += buf1.LoadFloat4(idx1);

  r.x += buf2.LoadFloat(idx2);
  r.xy += buf2.LoadFloat2(idx2, status);
  r.xyz += buf2.LoadFloat3(idx2);
  r.xyzw += buf2.LoadFloat4(idx2, status);

  r.x += buf1.LoadHalf(idx1, status);
  r.xy += buf1.LoadHalf2(idx1);
  r.xyz += buf1.LoadHalf3(idx1, status);
  r.xyzw += buf1.LoadHalf4(idx1);

  r.x += buf2.LoadHalf(idx2);
  r.xy += buf2.LoadHalf2(idx2, status);
  r.xyz += buf2.LoadHalf3(idx2);
  r.xyzw += buf2.LoadHalf4(idx2, status);

  r.x += buf1.LoadDouble(idx1);
  r.xy += buf1.LoadDouble2(idx1, status);

  r.x += buf2.LoadDouble(idx2, status);
  r.xy += buf2.LoadDouble2(idx2);

  return r;
}