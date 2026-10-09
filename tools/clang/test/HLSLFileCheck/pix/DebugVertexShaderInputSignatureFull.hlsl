// RUN: %dxc -Emain -Tvs_6_0 %s | %opt -S -hlsl-dxil-debug-instrumentation,parameter0=1,parameter1=2 -hlsl-dxilemit | %FileCheck %s

// The companion to DebugDenseVertexShaderInput.hlsl, which uses 31 of the 32
// input registers and so has room for one injected system value. This shader
// uses all 32, so neither SV_VertexID nor SV_InstanceID fits and the debugger
// has no identity at all to select an invocation by.
//
// Two things are checked below.
//
// First, the loadInput declaration must not survive unused: it is materialised
// before the pass knows whether either system value is available, and with
// neither call emitted it would be left behind as an unused external function,
// which the validator rejects with "External function 'dx.op.loadInput.i32' is
// unused".
//
// Second, the fallback selects no invocation rather than every invocation.
// Selecting every invocation would hand PIX an arbitrary vertex's trace to
// present as the one the user asked for. Selecting none is the honest answer:
// PIX reports the shader as undebuggable rather than debugging the wrong
// vertex.

// CHECK: VertexShaderSelection:None

// Neither identity is available, so no invocation is selected. "br i1 true"
// here would mean every vertex writes debug records.
// CHECK: br i1 false, label %PIXInterestingBlock, label %PIXNonInterestingBlock

// The integer loadInput overload must not survive as an unused declaration.
// The shader's own attributes are float, so any .i32 loadInput at all - call
// or declare - means the orphan is back. Checked after the branch above so
// this scans the declaration block at the end of the module.
// CHECK-NOT: loadInput.i32

struct DenseVertexShaderInput
{
  float4 attributes[32] : ATTR;
};

float4 main(DenseVertexShaderInput input) : SV_Position
{
  float4 result = 0;
  [unroll] for (uint index = 0; index < 32; ++index)
  {
    result += input.attributes[index];
  }
  return result;
}
