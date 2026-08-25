// RUN: %dxc -Emain -Tvs_6_0 %s | %opt -S -hlsl-dxil-debug-instrumentation,parameter0=1,parameter1=2 -hlsl-dxilemit | %FileCheck %s

// The debugger identifies a vertex shader invocation by (SV_VertexID, SV_InstanceID),
// injecting whichever of the two the shader did not already declare. A vertex shader
// input signature is allocated by the input assembler, which gives every element its
// own register, and D3D allows only 32 of them. The shader below uses 31 already, so
// there is room for exactly one injected system value.
//
// The pass injects as many as fit, most discriminating first, and reports which ones
// it got so PIX knows the selection is by vertex only. Selection is then exact for a
// single-instance draw and degrades to "first matching instance" otherwise, which is
// strictly better than refusing to debug the shader.

// CHECK: VertexShaderSelection:VertexIdOnly

// The injected SV_VertexID is element 1 (the 31-row ATTR array is a single element).
// CHECK: %VertId = call i32 @dx.op.loadInput.i32(i32 4, i32 1, i32 0, i8 0, i32 undef)
// Nothing may be loaded in between: there is no instance id to compare against.
// CHECK-NEXT: %CompareToVertId = icmp eq i32 %VertId, 1
// CHECK-NEXT: br i1 %CompareToVertId, label %PIXInterestingBlock, label %PIXNonInterestingBlock

// SV_VertexID must occupy the one free register, 31, and be the last thing injected.
// See DxilMDHelper::EmitSignatureElement for the meaning of these entries:
//               ID                   TypeU32 SemKin Sem-Idx         interp  Rows  Cols   Row    Col
//                |                     |     |       |                |      |     |      |      |
// CHECK: = !{i32 1, !"SV_VertexID", i8 5, i8 1, ![[VIDID:[0-9]*]], i8 0, i32 1, i8 1, i32 31, i8 0,

// CHECK-NOT: !"SV_InstanceID"

struct DenseVertexShaderInput
{
  float4 attributes[31] : ATTR;
};

float4 main(DenseVertexShaderInput input) : SV_Position
{
  float4 result = 0;
  [unroll] for (uint index = 0; index < 31; ++index)
  {
    result += input.attributes[index];
  }
  return result;
}
