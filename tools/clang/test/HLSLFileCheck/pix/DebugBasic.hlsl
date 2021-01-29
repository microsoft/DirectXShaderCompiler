// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-debug-instrumentation | %FileCheck %s

// Check that the basic starting header is present:

// CHECK: %PIX_DebugUAV_Handle = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)
// CHECK: %XPos = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
// CHECK: %YPos = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
// CHECK: %XIndex = fptoui float %XPos to i32
// CHECK: %YIndex = fptoui float %YPos to i32
// CHECK: %CompareToX = icmp eq i32 %XIndex, 0
// CHECK: %CompareToY = icmp eq i32 %YIndex, 0
// CHECK: %ComparePos = and i1 %CompareToX, %CompareToY
// CHECK: %OffsetMultiplicand = zext i1 %ComparePos to i32
// CHECK: %ComplementOfMultiplicand = sub i32 1, %OffsetMultiplicand
// CHECK: %OffsetAddend = mul i32 983040, %ComplementOfMultiplicand
// CHECK: %IncrementForThisInvocation = mul i32 8, %OffsetMultiplicand

// Check the first instruction was instrumented:
// CHECK: %UAVIncResult = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_DebugUAV_Handle, i32 0
// CHECK: %MaskedForUAVLimit = and i32 %UAVIncResult, 983039
// CHECK: %MultipliedForInterest = mul i32 %MaskedForUAVLimit, %OffsetMultiplicand
// CHECK: %AddedForInterest = add i32 %MultipliedForInterest, %OffsetAddend
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %PIX_DebugUAV_Handle, i32 %AddedForInterest


[RootSignature("")]
float4 main() : SV_Target {
    return float4(0,0,0,0);
}