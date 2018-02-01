// RUN: %dxc -T lib_6_2 %s | FileCheck %s

// CHECK: call void @dx.op.acceptHitAndEndSearch(i32 155)
// CHECK: call void @dx.op.commitHitAndStopRay(i32 156)

float4 emit(uint shader)  {
  if (shader < 2)
    AcceptHitAndEndSearch();
  if (shader < 9)
    CommitHitAndStopRay();
   return 2.6;
}