// RUN: %dxc -T vs_6_0 -E main %s | FileCheck %s

int main() : OUT
{
    // CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 11)
    int1x1 m = int1x1(11);
    return (int)m;
}