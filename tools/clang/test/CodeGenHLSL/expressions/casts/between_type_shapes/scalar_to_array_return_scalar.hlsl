// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s

typedef int i1[1];
int main() : OUT
{
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 1)
    int v = 1;
    i1 a = (i1)v;
    return a[0]; // Workaround for GitHub #1808 - DXC doesn't support array return types
}