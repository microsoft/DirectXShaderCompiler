// RUN: %dxc /T vs_6_0 /E main > %s | FileCheck %s | XFail GitHub #1795

int2 main() : OUT
{
    // call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 1)
    // call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 1, i32 2)
    int array[] = { 1, 2 };
    return (int2)array;
}
