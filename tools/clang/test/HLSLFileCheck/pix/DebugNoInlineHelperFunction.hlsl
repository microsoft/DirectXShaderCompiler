// RUN: %dxc -Emain -Tcs_6_2 /Od /Zi %s | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation,parameter0=1,parameter1=2,parameter2=3 -hlsl-dxilemit | %FileCheck %s
// RUN: %dxc -Emain -Tcs_6_2 /Od /Zi %s | %opt -S -dxil-dbg-value-to-dbg-declare -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation,parameter0=1,parameter1=2,parameter2=3 -hlsl-dxilemit | %FileCheck %s -check-prefixes=LOCALS,TRACED

// PIX names a shader invocation by the stream of records that one thread writes
// into the debug UAV, and maps that stream to exactly one function. A helper
// instrumented as a function of its own writes its records under a second
// invocation identity for a thread that runs once, and PIX discards them.
//
// The passes inline a helper into the entry point before anything is numbered.
// PIX recovers the helper frame from the inlinedAt chain of each inlined
// instruction, and the helper locals stay attributed to the helper.

// One function gives one invocation identity.
// CHECK: InstructionRange: {{[0-9]+}} {{[0-9]+}} main cs
// CHECK-NOT: InstructionRange:

// Every helper is inlined, so the pass reports none as surviving.
// CHECK-NOT: UninlinedFunction:

// CHECK: define void @main()
// CHECK-NOT: define {{.*}}ScaleHelper

// Debug info still names the helper, so PIX rebuilds the call stack.
// CHECK: !DISubprogram(name: "ScaleHelper"
// CHECK: inlinedAt:

// A helper local must be traced as well as scoped, or PIX reads it as
// unavailable. main holds no float local of its own, so a float alloca that
// carries a virtual register belongs to the inlined helper.
// TRACED: [[SCALED:%[0-9]+]] = alloca [1 x float], i32 0, !pix-alloca-reg

// The helper local stays scoped to the helper, which puts it under the correct
// frame in the PIX locals view.
// LOCALS: call void @llvm.dbg.declare(metadata [1 x float]* [[SCALED]],{{.*}}; var:"scaled"

// The store that traces the local carries no debug location. Requiring
// !pix-dxil-inst-num to follow the pointer operand immediately checks this.
// llvm::InlineFunction stamps the call site location onto each inlined
// instruction that carries none, so shadow storage must not exist yet when the
// helper is inlined.
// TRACED: [[SCALEDGEP:%[0-9]+]] = getelementptr [1 x float], [1 x float]* [[SCALED]], i32 0, i32 0
// TRACED-NEXT: store float %{{[A-Za-z0-9_.]+}}, float* [[SCALEDGEP]], !pix-dxil-inst-num {{![0-9]+}}, !pix-alloca-reg-write

// LOCALS: ![[HELPER:[0-9]+]] = !DISubprogram(name: "ScaleHelper"
// LOCALS: !DILocalVariable({{.*}}name: "scaled", scope: ![[HELPER]],

RWStructuredBuffer<float> Output : register(u0);

[noinline]
float ScaleHelper(float value)
{
  float scaled = value * 3.f;
  return scaled;
}

[numthreads(1, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
  Output[threadId.x] = ScaleHelper(threadId.y);
}
