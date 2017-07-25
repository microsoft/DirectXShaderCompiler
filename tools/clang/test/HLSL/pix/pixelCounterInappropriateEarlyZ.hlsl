// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-add-pixel-hit-instrmentation,rt-width=16,num-pixels=64,force-early-z=1 | %FileCheck %s

// Check the write to the UAV was emitted:
// CHECK: %UAVIncResult = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_CountUAV_Handle, i32 0, i32 %Clamped, i32 0, i32 0, i32 1)

// Early z should NOT be present even though we asked for it, due to the discard instruction. That 8 (with its key value of 0) at the end should be a 0
// CHECK-NOT: !{i32 0, i64 8}

float4 main(float4 pos : SV_Position) : SV_Target{
  discard;
  return pos;
}

