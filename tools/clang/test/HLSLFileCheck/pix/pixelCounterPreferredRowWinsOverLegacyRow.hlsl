// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-add-pixel-hit-instrmentation,rt-width=16,num-pixels=64,preferred-sv-position-row=3,upstream-sv-position-row=2 | %FileCheck %s

// When both spellings are supplied, preferred-sv-position-row must win
// deterministically over the legacy upstream-sv-position-row alias. Row 2 and
// row 3 are both free here, so if the legacy value won instead, SV_Position
// would land on row 2, not row 3; this test pins the row down to prove which
// spelling was actually read.

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

// TEXCOORD0/1 share row 0, COLOR0 occupies row 1, leaving rows 2 and 3 free.
// CHECK-DAG: !{i32 0, !"TEXCOORD", i8 9, i8 0, !{{[0-9]+}}, i8 2, i32 1, i8 2, i32 0, i8 0, {{.*}}}
// CHECK-DAG: !{i32 1, !"TEXCOORD", i8 9, i8 0, !{{[0-9]+}}, i8 2, i32 1, i8 2, i32 0, i8 2, {{.*}}}
// CHECK-DAG: !{i32 2, !"COLOR", i8 9, i8 0, !{{[0-9]+}}, i8 2, i32 1, i8 4, i32 1, i8 0, {{.*}}}

// SV_Position lands on row 3 -- the preferred-sv-position-row value -- never
// row 2, which is what the legacy upstream-sv-position-row value would have
// produced had it won instead.
// CHECK-DAG: !{i32 {{[0-9]+}}, !"SV_Position", i8 9, i8 3, !{{[0-9]+}}, i8 4, i32 1, i8 4, i32 3, i8 0, null}
