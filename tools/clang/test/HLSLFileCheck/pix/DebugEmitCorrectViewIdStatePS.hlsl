// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-debug-instrumentation,parameter0=1,parameter1=2 -viewid-state -hlsl-dxilemit | %FileCheck %s

// CHECK: !dx.viewIdState = !{![[VIEWIDDATA:[0-9]*]]}

// The debug instrumentation adds SV_Position to the input signature for this
// PS on a row of its own -- overlapping TEXCOORD's row produces a module the
// validator rejects -- so the signature spans two rows and view id state
// describes eight input components.
//
// The first two entries are the input and output component counts. The remaining
// eight are the output mask for each input component: TEXCOORD.x and .y are
// components 0 and 1, and "input.Tex.xyxy" makes them drive outputs {0,2} and
// {1,3} respectively. SV_Position's four components drive nothing.
// CHECK: ![[VIEWIDDATA]] = !{[10 x i32] [i32 8, i32 4, i32 5, i32 10, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0]}


struct VS_OUTPUT {
  float2 Tex : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : SV_Target {
  return input.Tex.xyxy;
}