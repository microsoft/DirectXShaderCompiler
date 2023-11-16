// RUN: %dxc -T ps_6_3 %s | %opt -S -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation,parameter0=10,parameter1=20,parameter2=30 | %FileCheck %s




// The ret's instruction number should be 4 (the last integer on this line):
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %PIX_DebugUAV_Handle, i32 {{.*}}, i32 undef, i32 4
// But we'll check that instruction number:
// CHECK: ret void, !pix-dxil-inst-num [[RetInstNum:![0-9]+]]
// CHECK-DAG: [[RetInstNum]] = !{i32 3, i32 4}


float4 main() : SV_Target {
  return float4(0, 0, 0, 0);
}