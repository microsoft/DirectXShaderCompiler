// RUN: %dxc -E main -T vs_6_0 -Zi %s | FileCheck %s

// Test that the debug information for the result of a texture load
// is preserved after scalarization and optims.

// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK-DAG: call void @llvm.dbg.value
// CHECK-DAG: extractvalue %dx.types.ResRet.i32
// CHECK-DAG: extractvalue %dx.types.ResRet.i32
// CHECK: call void @dx.op.storeOutput.i32
// CHECK: call void @dx.op.storeOutput.i32



StructuredBuffer<int2> buf;
int2 main() : OUT
{
    int2 result = buf.Load(0);
    return result;
}
