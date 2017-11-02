// RUN: %dxc -E main -T ps_6_2 %s | FileCheck %s

// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 1)  ; RawBufferLoad(srv,index,wot,mask)
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 3)  ; RawBufferLoad(srv,index,wot,mask)
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 7)  ; RawBufferLoad(srv,index,wot,mask)
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %buf1_texture_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 15)  ; RawBufferLoad(srv,index,wot,mask)
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 1)  ; RawBufferLoad(srv,index,wot,mask)
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 3)  ; RawBufferLoad(srv,index,wot,mask)
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 7)  ; RawBufferLoad(srv,index,wot,mask)
// CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %buf2_UAV_rawbuf, i32 %{{[0-9]+}}, i32 undef, i8 15)  ; RawBufferLoad(srv,index,wot,mask)
// CHECK: sitofp
// CHECK: uitofp

ByteAddressBuffer buf1;
RWByteAddressBuffer buf2;

float4 main(uint idx1 : IDX1, uint idx2 : IDX2) : SV_Target {
  uint status;
  float4 r = float4(0,0,0,0);
  r.x += buf1.LoadInt(idx1);
  r.xy += buf1.LoadInt2(idx1, status);
  r.xyz += buf1.LoadInt3(idx1);
  r.xyzw += buf1.LoadInt4(idx1, status);

  r.x += buf2.LoadInt(idx2, status);
  r.xy += buf2.LoadInt2(idx2);
  r.xyz += buf2.LoadInt3(idx2, status);
  r.xyzw += buf2.LoadInt4(idx2);

  r.x += buf1.Load(idx1);
  r.xy += buf1.Load2(idx1, status);
  r.xyz += buf1.Load3(idx1);
  r.xyzw += buf1.Load4(idx1, status);

  r.x += buf2.Load(idx2, status);
  r.xy += buf2.Load2(idx2);
  r.xyz += buf2.Load3(idx2, status);
  r.xyzw += buf2.Load4(idx2);
 
  return r;
}