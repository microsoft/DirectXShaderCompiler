// REQUIRES: dxil-1-10

// RUN: %dxc -T cs_6_10 %s | FileCheck %s

// CHECK: call i1 @dx.op.isDebuggerPresent(i32 -2147483614)  ; IsDebuggerPresent()

RWStructuredBuffer<uint> Output : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 threadId : SV_DispatchThreadID) {
    if (dx::IsDebuggerPresent()) {
        Output[threadId.x] = 1;
    } else {
        Output[threadId.x] = 0;
    }
}
