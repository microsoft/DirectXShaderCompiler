// RUN: %dxc -Emain -Tps_6_0 -Od %s | %opt -S -hlsl-dxil-debug-instrumentation,authoritativeSVPositionRow=0,UAVSize=65536 -hlsl-dxilemit | %FileCheck %s -check-prefixes=AUTHORITATIVE
// RUN: %dxc -Emain -Tps_6_0 -Od %s | %opt -S -hlsl-dxil-debug-instrumentation,upstreamSVPositionRow=0,UAVSize=65536 -hlsl-dxilemit | %FileCheck %s -check-prefixes=HINT

// The debugger needs SV_Position to identify a pixel, and injects one when the
// shader does not declare it. Which register it lands on matters: the upstream
// stage writes position to a particular register, and reading it from any other
// gives the debugger coordinates nothing wrote.
//
// PIX cannot always read the upstream signature. Some PIX builds send row 0
// both when the previous stage genuinely uses row 0 and when the row is
// unknown. The two cannot be distinguished by value, so they are
// distinguished by option name, and this pass has to honour that distinction
// the same way DxilAddPixelHitInstrumentation does.
//
// The shader below packs TEXCOORD0 and TEXCOORD1 into register 0, so the two
// spellings have visibly different correct answers.

// Authoritative: the caller vouches for register 0, so SV_Position must land
// there and the TEXCOORDs are repacked out of the way. Checked with -DAG
// because the signature elements are emitted in element order, which puts the
// displaced TEXCOORDs ahead of the injected SV_Position.
//                                                                      Row    Col
//                                                                       |      |
// AUTHORITATIVE-DAG: !{i32 3, !"SV_Position", i8 9, i8 3, {{.*}}, i32 0, i8 0, null}
// AUTHORITATIVE-DAG: !{i32 0, !"TEXCOORD", i8 9, i8 0, {{.*}}, i32 2, i8 0,
// AUTHORITATIVE-DAG: !{i32 1, !"TEXCOORD", i8 9, i8 0, {{.*}}, i32 2, i8 2,

// Hint: the row may have been fabricated, so nothing already in the signature
// is moved. The TEXCOORDs keep register 0 and SV_Position goes elsewhere.
// HINT-DAG: !{i32 0, !"TEXCOORD", i8 9, i8 0, {{.*}}, i32 0, i8 0,
// HINT-DAG: !{i32 1, !"TEXCOORD", i8 9, i8 0, {{.*}}, i32 0, i8 2,
// HINT-NOT: !{i32 3, !"SV_Position", i8 9, i8 3, {{.*}}, i32 0, i8 0, null}

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
