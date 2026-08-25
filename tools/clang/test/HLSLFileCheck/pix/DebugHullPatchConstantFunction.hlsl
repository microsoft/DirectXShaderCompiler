// RUN: %dxc -Emain -Ths_6_2 %s | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation,parameter0=1,parameter1=2 -hlsl-dxilemit | %FileCheck %s

// The runtime invokes a hull shader patch-constant function rather than the
// entry point does, so the inlining keeps it and the annotation pass numbers it
// as a steppable range of its own. An uninstrumented range emits no trace
// record, so a user who steps into the patch-constant body sees instructions
// with no values behind them. The pass instruments it as well as the entry
// point.
//
// SV_OutputControlPointID only means something in the control point phase, so
// the patch-constant function selects an invocation by primitive alone.

// Two functions are numbered, so the helper that both of them call is inlined
// away. No third range is advertised, and none survives uninlined.
// CHECK-DAG: InstructionRange: {{[0-9]+ [0-9]+}} main hs
// CHECK-DAG: InstructionRange: {{[0-9]+ [0-9]+}} PatchConstantFunction
// CHECK-NOT: InstructionRange:
// CHECK-NOT: UninlinedFunction:

// The patch-constant function selects on the primitive alone.
// CHECK: define void @"\01?PatchConstantFunction
// CHECK: %PrimId = call i32 @dx.op.primitiveID.i32(i32 108)
// CHECK-NEXT: %CompareToPrimId = icmp eq i32 %PrimId, 1
// CHECK-NEXT: br i1 %CompareToPrimId, label %PIXInterestingBlock, label %PIXNonInterestingBlock

// The entry point selects on the control point and the primitive.
// CHECK: define void @main()
// CHECK: %ControlPointId = call i32 @dx.op.outputControlPointID.i32(i32 107)
// CHECK-NEXT: %PrimId = call i32 @dx.op.primitiveID.i32(i32 108)
// CHECK-NEXT: %CompareToPrimId = icmp eq i32 %PrimId, 1
// CHECK-NEXT: %CompareToControlPointId = icmp eq i32 %ControlPointId, 2
// CHECK-NEXT: %CompareBoth = and i1 %CompareToControlPointId, %CompareToPrimId
// CHECK-NEXT: br i1 %CompareBoth, label %PIXInterestingBlock, label %PIXNonInterestingBlock

// CHECK-NOT: HullHelper

struct HsConstantData
{
  float Edges[3] : SV_TessFactor;
  float Inside   : SV_InsideTessFactor;
};

struct ControlPoint
{
  float3 position : WORLDPOS;
};

struct OutputPoint
{
  float3 vPosition : BEZIERPOS;
};

[noinline]
float HullHelper(float value)
{
  return value * 2.f;
}

HsConstantData PatchConstantFunction(InputPatch<ControlPoint, 3> ip)
{
  HsConstantData Output;
  Output.Edges[0] = HullHelper(ip[0].position.x);
  Output.Edges[1] = 8;
  Output.Edges[2] = 8;
  Output.Inside = 8;
  return Output;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("PatchConstantFunction")]
OutputPoint main(InputPatch<ControlPoint, 3> ip, uint i : SV_OutputControlPointID)
{
  OutputPoint Output;
  Output.vPosition = ip[i].position * HullHelper(2.f);
  return Output;
}
