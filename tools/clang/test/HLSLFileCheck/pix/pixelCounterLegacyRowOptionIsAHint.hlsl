// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-add-pixel-hit-instrmentation,rt-width=16,num-pixels=64,upstream-sv-position-row=0 | %FileCheck %s

// PIX ships dxcompiler.dll separately from the PIX executable, so an older PIX
// talking to a newer compiler is routine. Older PIX sends row 0 both when the
// upstream stage really uses row 0 and when it could not read the upstream
// signature at all, which means the value carries no promise. Acting on it
// would evict a real interpolant -- one that is still linkage-bound to whatever
// the upstream stage actually is -- on the strength of a guess, breaking the
// pipeline the relocation exists to keep working.
//
// "upstream-sv-position-row" is the pre-rename spelling of
// preferred-sv-position-row, kept as an accepted alias so older PIX builds
// keep working. Both spellings mean a hint: use the row if it happens to be
// free, never move anything to clear it. Only the required-sv-position-row
// spelling licenses eviction; see
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
