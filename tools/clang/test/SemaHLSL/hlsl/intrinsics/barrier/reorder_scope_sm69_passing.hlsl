// RUN: %dxc -T lib_6_9 -E main %s | FileCheck %s

[shader("raygeneration")]
void main() {
// CHECK:  call void @dx.op.barrierByMemoryType(i32 244, i32 1, i32 8)
  Barrier(UAV_MEMORY, REORDER_SCOPE);
}
