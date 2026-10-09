// RUN: %dxc -Emain -Tgs_6_0 %s | %opt -S -hlsl-dxil-debug-instrumentation,UAVSize=128,parameter0=1,parameter1=2,upstreamSVPositionRow=0 | %FileCheck %s

// A geometry shader can write several output streams, each with its own
// register space, and only one of them is rasterized. This shader deliberately
// puts SV_Position on register 0 of stream 0 and register 1 of stream 1, so
// "the row SV_Position is on" is ambiguous unless the stream is part of the
// question.
//
// Whoever reads this signature to decide where a downstream pixel shader should
// expect SV_Position has to filter on the rasterized stream; picking the first
// SV_Position in the list gets register 0 here, which is the wrong answer
// whenever stream 1 is the rasterized one. The instrumentation itself never
// relocates a geometry shader signature, and this test pins that down: all four
// elements have to come out exactly as the front end packed them, so that a
// stream-blind row query cannot be papered over by a relocation.
//
// As with MeshShaderSignatureIsNotRelocated.hlsl, this pins the end-to-end
// behaviour rather than the ShaderKind guard in FindOrAddSV_Position: the
// debug-instrumentation pass never reaches that helper for a geometry shader,
// and the checks below are on the output signature while the relocation only
// touches the input one. Removing the guard would leave this test green.

struct FirstStreamOut
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct SecondStreamOut
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};

[maxvertexcount(3)]
void main(triangle float4 input[3] : SV_Position,
          inout PointStream<FirstStreamOut> firstStream,
          inout PointStream<SecondStreamOut> secondStream)
{
    FirstStreamOut first = (FirstStreamOut)0;
    first.position = input[0];
    first.uv = float2(1, 2);
    firstStream.Append(first);

    SecondStreamOut second = (SecondStreamOut)0;
    second.position = input[1];
    second.uv = float2(3, 4);
    secondStream.Append(second);
}

// The pass really did run, so the signature checks below are not vacuous.
// CHECK: call i32 @dx.op.primitiveID.i32(i32 108)

// Stream 0: SV_Position at register 0, TEXCOORD0 at register 1.
// CHECK-DAG: !{i32 0, !"SV_Position", i8 9, i8 3, !{{[0-9]+}}, i8 4, i32 1, i8 4, i32 0, i8 0, {{.*}}}
// CHECK-DAG: !{i32 1, !"TEXCOORD", i8 9, i8 0, !{{[0-9]+}}, i8 2, i32 1, i8 2, i32 1, i8 0, {{.*}}}

// Stream 1: the same two semantics, at the opposite registers.
// CHECK-DAG: !{i32 2, !"TEXCOORD", i8 9, i8 0, !{{[0-9]+}}, i8 2, i32 1, i8 2, i32 0, i8 0, {{.*}}}
// CHECK-DAG: !{i32 3, !"SV_Position", i8 9, i8 3, !{{[0-9]+}}, i8 4, i32 1, i8 4, i32 1, i8 0, {{.*}}}
