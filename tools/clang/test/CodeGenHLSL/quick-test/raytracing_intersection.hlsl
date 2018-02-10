// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: define void [[intersection1:@"\\01\?intersection1@[^\"]+"]]() #0 {
// CHECK:   [[CurrentRayT:%[^ ]+]] = call float @dx.op.currentRayT.f32(i32 154)
// CHECK:   call i1 @dx.op.reportHit.struct.MyAttributes(i32 158, float [[CurrentRayT]], i32 0, %struct.MyAttributes* nonnull {{.*}})
// CHECK:   ret void

struct MyAttributes {
  float2 bary;
  uint id;
};

[shader("intersection")]
void intersection1()
{
  float hitT = CurrentRayT();
  MyAttributes attr = (MyAttributes)0;
  bool bReported = ReportHit(hitT, 0, attr);
}
