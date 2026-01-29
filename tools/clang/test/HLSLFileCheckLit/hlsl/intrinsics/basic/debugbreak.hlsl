// REQUIRES: dxil-1-10

// RUN: %dxc -T cs_6_10 %s | FileCheck %s

// CHECK: call void @dx.op.debugBreak(i32

[numthreads(8, 8, 1)]
void main(uint3 threadId : SV_DispatchThreadID) {
    if (threadId.x == 0) {
        DebugBreak();
    }
}
