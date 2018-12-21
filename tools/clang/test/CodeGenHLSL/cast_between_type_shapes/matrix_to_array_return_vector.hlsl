// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s

typedef int i4[4];
int4 main() : OUT
{
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 11)
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 1, i32 12)
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 2, i32 21)
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 3, i32 22)
    int2x2 m = int2x2(11, 12, 21, 22);
    i4 a = (i4)m;
    return int4(a[0], a[1], a[2], a[3]); // Workaround for GitHub #1808 - DXC doesn't support array return types
}