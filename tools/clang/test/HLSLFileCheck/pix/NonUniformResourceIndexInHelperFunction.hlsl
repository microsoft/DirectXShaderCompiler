// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-non-uniform-resource-index-instrumentation -hlsl-dxilemit | %FileCheck %s

// The annotation prepass inlines away each non-entry function of a non-library
// module, because PIX cannot attribute a separately instrumented function to an
// invocation. See PIXPassHelpers::InlineNonEntryFunctions. The
// non-uniform-resource-index pipeline shares that prepass for the instruction
// ordinals its diagnostics use, so an unqualified dynamic index inside a
// [noinline] helper must still be diagnosed once the helper is part of the
// entry point.
//
// The [noinline] attribute is necessary. Without it the front end inlines the
// helper and the prepass has nothing to do. The helper signature holds scalars
// only, because a function that reaches DXIL in a non-library module takes and
// returns no vector.

// The helper does not appear as a separate function.
// CHECK-NOT: define {{.*}}IndexInHelper

// The dynamic index is still reported, and it is addressed to a real ordinal
// instead of to bit 0.
// CHECK: @dx.op.waveActiveAllEqual
// CHECK: shl i32 %{{[0-9]+}}, {{[1-9][0-9]*}}
// CHECK: @dx.op.atomicBinOp.i32(i32 78
// CHECK-NOT: NuriNotInstrumentedMissingInstructionNumber

Texture2D tex[8] : register(t0);

[noinline]
float IndexInHelper(float u, float v)
{
    uint index = u * v;
    return tex[index].Load(int3(0, 0, 0)).x;
}

float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    return IndexInHelper(uv.x, uv.y);
}
