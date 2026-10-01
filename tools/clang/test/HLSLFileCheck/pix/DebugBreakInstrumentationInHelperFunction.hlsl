// RUN: %dxc -Emain -Tcs_6_10 %s | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debugbreak-instrumentation -hlsl-dxilemit | %FileCheck %s

// The debug-break pipeline shares the annotation prepass, so it also sees a
// module whose helpers are inlined away. A DebugBreak inside a [noinline]
// helper must still be found and instrumented once the helper is part of the
// entry point.

// The helper does not appear as a separate function.
// CHECK-NOT: define {{.*}}BreakInHelper

// The prepass reports no surviving helper, so PIX offers one steppable range.
// CHECK-NOT: UninlinedFunction:
// CHECK: InstructionRange: {{[0-9]+}} {{[0-9]+}} main cs
// CHECK-NOT: InstructionRange:

// The break is still recorded, from inside the entry point.
// CHECK: %PixUAVHandle = call %dx.types.Handle @dx.op.createHandleFromBinding(
// CHECK: %DebugBreakBitSet = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle
// CHECK-NOT: @dx.op.debugBreak

RWStructuredBuffer<uint> Output : register(u0);

[noinline]
uint BreakInHelper(uint value)
{
    DebugBreak();
    return value + 1;
}

[numthreads(1, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    Output[0] = BreakInHelper(threadId.x);
}
