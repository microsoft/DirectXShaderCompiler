// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-non-uniform-resource-index-instrumentation | %FileCheck %s

// With the annotation prepass in place, the diagnostic is addressed to
// the ordinal of the createHandle that performed the unmarked dynamic
// indexing. The pass encodes that ordinal as a shift. A shift of zero
// aliases the diagnostic onto bit 0.
//
// Match any non-zero shift rather than a literal ordinal. A createHandle
// whose index comes from an interpolated input is never the first
// numbered instruction.

// CHECK-NOT: NuriNotInstrumentedMissingInstructionNumber
// CHECK: @dx.op.waveActiveAllEqual
// CHECK: shl i32 %{{[0-9]+}}, {{[1-9][0-9]*}}
// CHECK: @dx.op.atomicBinOp.i32(i32 78

Texture2D tex[8] : register(t0);

float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    uint index = uv.x * uv.y;
    return tex[index].Load(int3(0, 0, 0));
}
