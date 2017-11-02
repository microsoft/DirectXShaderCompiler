// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK-NOT: @dx.op.rawBufferLoad
// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32
// CHECK: call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32
// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: call double @dx.op.makeDouble.f64
// CHECK: call double @dx.op.makeDouble.f64
// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: call double @dx.op.makeDouble.f64
// CHECK: call double @dx.op.makeDouble.f64

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