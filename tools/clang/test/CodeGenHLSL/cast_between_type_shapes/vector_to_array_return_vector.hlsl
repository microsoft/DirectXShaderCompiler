// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s

typedef int i2[2];
int2 main() : OUT
{
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 1)
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 1, i32 2)
    int2 v = int2(1, 2);
    i2 a = (i2)v;
    return int2(a[0], a[1]); // Workaround for GitHub #1808 - DXC doesn't support array return types
}