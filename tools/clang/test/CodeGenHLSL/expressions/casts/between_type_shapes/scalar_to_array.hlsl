// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s | XFail GitHub #1808

typedef int i1[1];
i1 main() : OUT
{
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 1)
    int v = 1;
    return (i1)v;
}