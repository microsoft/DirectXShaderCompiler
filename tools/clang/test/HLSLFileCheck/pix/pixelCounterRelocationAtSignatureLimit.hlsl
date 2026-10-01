// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-add-pixel-hit-instrmentation,rt-width=16,num-pixels=64,required-sv-position-row=30 | %FileCheck %s

// The pathological case for the relocation: a pixel shader that has already
// used 31 of the 32 available input registers. ATTR occupies rows 0-29 as a
// single indexed element, and the two rasterizer system values share row 30 --
// which is where the upstream stage put SV_Position, so both of them have to
// be evicted.
//
// There is exactly one spare register left, and both evicted elements are one
// component wide, so both of them fit in it. An allocator that hands each
// evicted element a fresh row instead needs two, runs off the end of the
// register file, and emits register 32 -- a number no D3D signature can hold
// and that PIX ships straight to the driver, because it does not re-run the
// validator over the modules it patches.

struct DensePSInput
{
    float4 attributes[30] : ATTR;
    uint primitiveId : SV_PrimitiveID;
    bool isFrontFace : SV_IsFrontFace;
};

float4 main(DensePSInput input) : SV_Target
{
    float4 accumulated = 0;
    [unroll] for (uint index = 0; index < 30; ++index)
    {
        accumulated += input.attributes[index];
    }

    accumulated.a = input.primitiveId + (input.isFrontFace ? 1.0f : 0.0f);
    return accumulated;
}

// The 30-row array is not on the target row and must stay exactly where the
// front end packed it.
// CHECK-DAG: !{i32 0, !"ATTR", i8 9, i8 0, !{{[0-9]+}}, i8 2, i32 30, i8 4, i32 0, i8 0, {{.*}}}

// SV_Position takes the register the upstream stage used.
// CHECK-DAG: !{i32 {{[0-9]+}}, !"SV_Position", i8 9, i8 3, !{{[0-9]+}}, i8 4, i32 1, i8 4, i32 30, i8 0, null}

// Both evicted system values land in the last available register, packed into
// separate components of it rather than taking a row each.
// CHECK-DAG: !{i32 1, !"SV_PrimitiveID", i8 5, i8 10, !{{[0-9]+}}, i8 1, i32 1, i8 1, i32 31, i8 0, {{.*}}}
// CHECK-DAG: !{i32 2, !"SV_IsFrontFace", i8 5, i8 13, !{{[0-9]+}}, i8 1, i32 1, i8 1, i32 31, i8 1, {{.*}}}
