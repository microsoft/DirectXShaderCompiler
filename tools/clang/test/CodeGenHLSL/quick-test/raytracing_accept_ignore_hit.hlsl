// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// CHECK: call void @dx.op.acceptHitAndEndSearch(i32 156)
// CHECK: call void @dx.op.ignoreHit(i32 155)

float4 emit(uint shader)  {
  if (shader < 2)
    AcceptHitAndEndSearch();
  if (shader < 9)
    IgnoreHit();
   return 2.6;
}