// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-add-pixel-hit-instrmentation,rt-width=16,num-pixels=64,preferred-sv-position-row=0 | %FileCheck %s

// The canonical spelling of the hint option (see
// pixelCounterLegacyRowOptionIsAHint.hlsl for its pre-rename alias). Row 0
// carries no promise, so acting on it would evict a real interpolant on the
// strength of a guess. preferred-sv-position-row must not license eviction:
// use the row if it happens to be free, never move anything to clear it.
// Only the required-sv-position-row spelling does that; see
// pixelCounterRelocationRepacksIntoSharedRow.hlsl for the same shader under
// that option.

struct PSInput
{
    float2 firstUV : TEXCOORD0;
    float2 secondUV : TEXCOORD1;
    float4 color : COLOR0;
};

float4 main(PSInput input) : SV_Target
{
    return input.color + float4(input.firstUV, input.secondUV);
}

// Every declared input keeps the register the front end gave it.
// CHECK-DAG: !{i32 0, !"TEXCOORD", i8 9, i8 0, !{{[0-9]+}}, i8 2, i32 1, i8 2, i32 0, i8 0, {{.*}}}
// CHECK-DAG: !{i32 1, !"TEXCOORD", i8 9, i8 0, !{{[0-9]+}}, i8 2, i32 1, i8 2, i32 0, i8 2, {{.*}}}
// CHECK-DAG: !{i32 2, !"COLOR", i8 9, i8 0, !{{[0-9]+}}, i8 2, i32 1, i8 4, i32 1, i8 0, {{.*}}}

// SV_Position goes to the first register that can hold it instead.
// CHECK-DAG: !{i32 {{[0-9]+}}, !"SV_Position", i8 9, i8 3, !{{[0-9]+}}, i8 4, i32 1, i8 4, i32 2, i8 0, null}
