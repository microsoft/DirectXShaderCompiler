// RUN: %dxc -E main -T vs_6_0 -Zi %s | FileCheck %s

// Test that the debug information for the result of a texture load
// is preserved after scalarization and optims.

// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: call void @llvm.dbg.value
// CHECK: extractvalue %dx.types.ResRet.i32
// CHECK: extractvalue %dx.types.ResRet.i32
// CHECK: call void @dx.op.storeOutput.i32
// CHECK: call void @dx.op.storeOutput.i32

// Exclude quoted source file (see readme)
// CHECK: {{!"[^"]*\\0A[^"]*"}}

StructuredBuffer<int2> buf;
int2 main() : OUT
{
    int2 result = buf.Load(0);
    return result;
}
