// RUN: %dxc -Emain -Tms_6_5 %s | %opt -S -hlsl-dxil-debug-instrumentation,UAVSize=128,parameter0=10,parameter1=20,parameter2=30,upstreamSVPositionRow=0 | %FileCheck %s

// A mesh shader is the one upstream stage whose signature the SV_Position
// relocation cannot reason about. The relocation rests on the claim that if the
// previous stage writes SV_Position at register N then it writes nothing else
// there, so anything the pixel shader has at register N is unpaired and safe to
// move. A mesh shader has two output signatures, per-vertex and per-primitive,
// each numbered from zero and packed by its own rules -- as this shader shows,
// with SV_Position at per-vertex register 0 and a per-primitive attribute also
// at register 0 -- so "register N" does not identify one thing to reason about.
//
// The instrumentation therefore leaves mesh shader signatures alone even when
// it is handed a row, and this test pins that down: the three signature
// elements have to come out exactly as the front end packed them.
//
// Note on what this does and does not cover. There are two independent reasons
// a mesh shader is unaffected: the ShaderKind guard in FindOrAddSV_Position, and
// the fact that DxilDebugInstrumentation only calls it for pixel shaders at all.
// The second alone is enough to make this test pass, and the checks below are on
// the *output* signature whereas the relocation only ever touches the *input*
// one, so deleting the ShaderKind guard would not turn this test red. It
// documents the front-end packing the guard's rationale rests on (two register
// spaces, both numbered from zero) and pins the end-to-end behaviour; it is not
// a unit test of the guard itself.

struct VertexOut
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct PrimitiveOut
{
    uint layer : TEXCOORD1;
};

[outputtopology("triangle")]
[numthreads(3, 1, 1)]
void main(uint threadIndex : SV_GroupIndex,
          out vertices VertexOut vertices[3],
          out primitives PrimitiveOut primitives[1],
          out indices uint3 indices[1])
{
    SetMeshOutputCounts(3, 1);
    vertices[threadIndex].position = float4(threadIndex, 0, 0, 1);
    vertices[threadIndex].uv = float2(threadIndex, 1);
    if (threadIndex == 0)
    {
        primitives[0].layer = 7;
        indices[0] = uint3(0, 1, 2);
    }
}

// The pass really did run, so the signature checks below are not vacuous.
// CHECK: %PIX_DebugUAV_Handle = call %dx.types.Handle @dx.op.createHandle
// CHECK: %ThreadIdX = call i32 @dx.op.threadId.i32(i32 93, i32 0)

// Per-vertex outputs: SV_Position at register 0, TEXCOORD0 at register 1.
// CHECK-DAG: !{i32 0, !"SV_Position", i8 9, i8 3, !{{[0-9]+}}, i8 4, i32 1, i8 4, i32 0, i8 0, {{.*}}}
// CHECK-DAG: !{i32 1, !"TEXCOORD", i8 9, i8 0, !{{[0-9]+}}, i8 2, i32 1, i8 2, i32 1, i8 0, {{.*}}}

// Per-primitive output: a different register space, whose register 0 has
// nothing to do with the per-vertex register 0 above.
// CHECK-DAG: !{i32 0, !"TEXCOORD", i8 5, i8 0, !{{[0-9]+}}, i8 1, i32 1, i8 1, i32 0, i8 0, {{.*}}}
