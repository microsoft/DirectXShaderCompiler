// RUN: %dxc -EPSMain -Tps_6_0 %s | %opt -S -hlsl-dxil-pix-shader-access-instrumentation,config=R0:1:2i1;.. | %FileCheck %s


// Check for udpate of UAV for each target (last column is byte offset into UAV, indexed by RT array index)
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_CountUAV_Handle, i32 2, i32 4
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_CountUAV_Handle, i32 2, i32 8
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_CountUAV_Handle, i32 2, i32 12

struct PSOut
{
  float4 rt0 : SV_Target;
  uint4 rt1 : SV_Target1;
  int4 rt2 : SV_Target2;

};

PSOut PSMain()
{
  PSOut o;
  o.rt0 = float4(1, 2, 3, 4);
  o.rt1 = uint4(5, 6, 7, 8);
  o.rt2 = int4(9, 10, 11, 12);
  return o;
}
