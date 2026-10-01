// RUN: %dxc -T lib_6_3 %s | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation,parameter0=1,parameter1=2,parameter2=3 -hlsl-dxilemit | %FileCheck %s

// A callable shader is a valid CallShader() target, so PIX must be able to step
// into it. The pass numbers its instructions and advertises them to PIX, so it
// must instrument it as well.
//
// A callable shader has no ray and no thread of its own, but
// dx.op.dispatchRaysIndex is legal in it, and reports the index of the ray
// generation invocation that is responsible for the call. PIX selects a raygen,
// any-hit, closest-hit or miss invocation on that same identity, so a callable
// shader uses it too.

// The callable shader is the only shader in the module, so a Block# line means
// that it is instrumented.
// CHECK: Block#

// CHECK: %RayX = call i32 @dx.op.dispatchRaysIndex.i32(i32 145, i8 0)
// CHECK: %RayY = call i32 @dx.op.dispatchRaysIndex.i32(i32 145, i8 1)
// CHECK: %RayZ = call i32 @dx.op.dispatchRaysIndex.i32(i32 145, i8 2)
// CHECK: %CompareToThreadIdX = icmp eq i32 %RayX, 1
// CHECK: %CompareToThreadIdY = icmp eq i32 %RayY, 2
// CHECK: %CompareToThreadIdZ = icmp eq i32 %RayZ, 3
// CHECK: %CompareAll = and i1 %CompareXAndY, %CompareToThreadIdZ
// CHECK: br i1 %CompareAll, label %PIXInterestingBlock, label %PIXNonInterestingBlock

RWStructuredBuffer<float> Output : register(u0);

struct CallableParameters
{
  float value;
};

[shader("callable")]
void MyCallable(inout CallableParameters parameters)
{
  parameters.value = parameters.value * 2.f;
  Output[0] = parameters.value;
}
