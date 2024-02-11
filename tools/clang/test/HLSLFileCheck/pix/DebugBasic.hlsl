// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-debug-instrumentation,UAVSize=128 | %FileCheck %s

// Check that the basic starting header is present:

// CHECK: %PIX_DebugUAV_Handle = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)
// CHECK: %XPos = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
// CHECK: %YPos = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
// CHECK: %XIndex = fptoui float %XPos to i32
// CHECK: %YIndex = fptoui float %YPos to i32
// CHECK: %CompareToX = icmp eq i32 %XIndex, 0
// CHECK: %CompareToY = icmp eq i32 %YIndex, 0
// CHECK: %ComparePos = and i1 %CompareToX, %CompareToY


// Check for branches-for-interest and AND value and counter location for a UAV size of 128
// CHECK: br i1 %ComparePos, label %PIXInterestingBlock, label %PIXNonInterestingBlock
// CHECK: %PIXOffsetOr = phi i32 [ 0, %PIXInterestingBlock ], [ 64, %PIXNonInterestingBlock ]
// CHECK: %PIXCounterLocation = phi i32 [ 63, %PIXInterestingBlock ], [ 127, %PIXNonInterestingBlock ]

// Check the first block header was emitted: (increment, AND + OR)
// CHECK: call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_DebugUAV_Handle, i32 0
// CHECK: and i32 
// CHECK: or i32



[RootSignature("")]
float4 main() : SV_Target {
    return float4(0,0,0,0);
}