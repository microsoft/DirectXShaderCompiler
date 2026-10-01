// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-non-uniform-resource-index-instrumentation | %FileCheck %s

// This pass addresses each diagnostic by the PIX instruction ordinal.
// This RUN line omits the annotation prepass, so no createHandle carries
// an ordinal. The pass leaves the handle uninstrumented and reports the
// missing precondition.
//
// The pass writes its messages to the same stream as the -S module print,
// and writes them before the module, so the message checks come first.

// CHECK-NOT: FoundDynamicIndexingNoNuri
// CHECK: NuriNotInstrumentedMissingInstructionNumber
// CHECK-NOT: @dx.op.waveActiveAllEqual
// CHECK-NOT: @dx.op.atomicBinOp

Texture2D tex[8] : register(t0);

float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    uint index = uv.x * uv.y;
    return tex[index].Load(int3(0, 0, 0));
}
