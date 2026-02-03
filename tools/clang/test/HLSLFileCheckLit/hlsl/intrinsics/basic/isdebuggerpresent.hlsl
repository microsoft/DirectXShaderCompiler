// REQUIRES: dxil-1-10

// RUN: %dxc -T cs_6_10 %s | FileCheck %s
// RUN: %dxc -T cs_6_10 -fcgl %s | FileCheck %s --check-prefix=FCGL
// RUN: %dxc -T cs_6_10 -ast-dump %s | FileCheck %s --check-prefix=AST

// CHECK: call i1 @dx.op.isDebuggerPresent(i32 -2147483614)  ; IsDebuggerPresent()

// FCGL: call i1 @"dx.hl.op.ro.i1 (i32)"(i32 426)

// AST: CallExpr {{.*}} 'bool'
// AST-NEXT: `-ImplicitCastExpr {{.*}} 'bool (*)()' <FunctionToPointerDecay>
// AST-NEXT:   `-DeclRefExpr {{.*}} 'IsDebuggerPresent' 'bool ()'

RWStructuredBuffer<uint> Output : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 threadId : SV_DispatchThreadID) {
    if (dx::IsDebuggerPresent()) {
        Output[threadId.x] = 1;
    } else {
        Output[threadId.x] = 0;
    }
}
