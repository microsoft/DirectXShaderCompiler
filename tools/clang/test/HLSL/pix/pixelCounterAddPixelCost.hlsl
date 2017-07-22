// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-add-pixel-hit-instrmentation,rt-width=16,num-pixels=64,add-pixel-cost=1 | %FileCheck %s

// Check the write to the UAV was emitted:
// CHECK: %UAVIncResult = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_CountUAV_Handle, i32 0, i32 %Clamped, i32 0, i32 0, i32 1)

// Check for pixel cost instructions:
// CHECK: %Weight = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %PIX_CountUAV_Handle, i32 128, i32 0)
// CHECK: %7 = add i32 %Clamped, 64
// Note this is intentionally failing right now: the last bit should have %weight, but when it runs in PIX, there's a problem...
// CHECK: %UAVIncResult2 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_CountUAV_Handle, i32 0, i32 %7, i32 0, i32 0, i32 1)


float4 main(float4 pos : SV_Position) : SV_Target {
  return pos;
}
