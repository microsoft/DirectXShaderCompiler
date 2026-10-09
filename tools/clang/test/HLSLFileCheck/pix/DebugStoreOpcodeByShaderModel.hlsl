// RUN: %dxc -Emain -Tcs_6_0 %s | %opt -S -hlsl-dxil-debug-instrumentation,UAVSize=1024 -hlsl-dxilemit | %FileCheck %s -check-prefix=SM60
// RUN: %dxc -Emain -Tcs_6_1 %s | %opt -S -hlsl-dxil-debug-instrumentation,UAVSize=1024 -hlsl-dxilemit | %FileCheck %s -check-prefix=SM61
// RUN: %dxc -Emain -Tcs_6_2 %s | %opt -S -hlsl-dxil-debug-instrumentation,UAVSize=1024 -hlsl-dxilemit | %FileCheck %s -check-prefix=SM62

// SM60: %PIX_DebugUAV_Handle = call %dx.types.Handle @dx.op.createHandle
// SM60-DAG: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %PIX_DebugUAV_Handle
// SM60-DAG: declare void @dx.op.bufferStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i8)

// SM61: %PIX_DebugUAV_Handle = call %dx.types.Handle @dx.op.createHandle
// SM61-DAG: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %PIX_DebugUAV_Handle
// SM61-DAG: declare void @dx.op.bufferStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i8)

// SM62: %PIX_DebugUAV_Handle = call %dx.types.Handle @dx.op.createHandle
// SM62-DAG: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %PIX_DebugUAV_Handle
// SM62-DAG: declare void @dx.op.rawBufferStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i8, i32)

[RootSignature("")]
[numthreads(1, 1, 1)]
void main(uint threadId : SV_DispatchThreadID) {
  uint value = threadId;
}
