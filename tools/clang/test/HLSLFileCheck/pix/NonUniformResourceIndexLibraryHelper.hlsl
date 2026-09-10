// RUN: %dxc -T lib_6_6 -Od %s | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-non-uniform-resource-index-instrumentation | %FileCheck %s

// Coverage for an unmarked dynamic index that stays in a library helper.
// The helper remains a separate function. The diagnostic is addressed to
// a non-zero instruction ordinal.

// CHECK-NOT: NuriNotInstrumentedMissingInstructionNumber
// CHECK: define void {{.*}}IndexInHelper
// CHECK: @dx.op.waveActiveAllEqual
// CHECK: shl i32 %{{[0-9]+}}, {{[1-9][0-9]*}}
// CHECK: @dx.op.atomicBinOp.i32(i32 78

RWTexture2D<float> RT[] : register(u0);

[noinline]
export void IndexInHelper(uint index)
{
    float2 rayIndex = DispatchRaysIndex().xy;
    RT[index][rayIndex] = 1;
}

[shader("raygeneration")]
void RayGen()
{
    IndexInHelper(DispatchRaysIndex().x);
}
