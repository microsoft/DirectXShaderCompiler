// RUN: %dxc -EPSMain -Tps_6_0 %s | %opt -S -hlsl-dxil-pix-shader-access-instrumentation,config=R0:1:1i1;.. | %FileCheck %s


// Check for udpate of UAV:
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_CountUAV_Handle, i32 2, i32


float4 PSMain() : SV_Target
{
  return float4(1,2,3,4);
}