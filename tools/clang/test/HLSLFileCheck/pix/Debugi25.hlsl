// RUN: %dxc -Emain -Tps_6_1 %s | %opt -S -dxil-dbg-value-to-dbg-declare -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation,UAVSize=65536 | %FileCheck %s

// Expect an i25 cast, or this test isn't testing anything:
// CHECK: [[CAST:%.*]] = trunc i32 %{{.*}} to i25

// Check that we correctly z-extended that i25 before trying to write it to i32
// [[ZEXT:%.*]] = zext i25 [[CAST]] to i32
// call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %PIX_DebugUAV_Handle, i32 %{{.*}}, i32 undef, i32 [[ZEXT]]

uint param;

bool fn()
{
    switch (param)
    {
    case 0:
    case 20:
    case 24:
        return false;
    }
    return true;
}

float4 main() : SV_Target
{
    float4 ret = float4(0, 0, 0, 0);
    if (fn())
    {
        ret = float4(1, 1, 1, 1);
    }
    return ret;
}
