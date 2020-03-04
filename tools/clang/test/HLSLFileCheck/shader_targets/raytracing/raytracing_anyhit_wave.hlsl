// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// CHECK:   %[[id:id|[0-9]+]] = getelementptr inbounds %struct.MyAttributes, %struct.MyAttributes* %[[attr:attr|[0-9]+]], i32 0, i32 1
// CHECK:   %[[_0_:[0-9]+]] = load i32, i32* %[[id]], align 4
// CHECK:   %[[WaveActiveAllEqual:WaveActiveAllEqual|[0-9]+]] = call i1 @dx.op.waveActiveAllEqual.i32(i32 115, i32 %[[_0_]])
// CHECK:   br i1 %[[WaveActiveAllEqual]], label %[[if_then:if\.then|[0-9]+]], label %[[if_end:if\.end|[0-9]+]]

// CHECK:   call void @dx.op.acceptHitAndEndSearch(i32 156)
// CHECK:   unreachable

struct MyPayload {
  float4 color;
  uint2 pos;
};

struct MyAttributes {
  float2 bary;
  uint id;
};

[shader("anyhit")]
void anyhit1( inout MyPayload payload : SV_RayPayload,
              in MyAttributes attr : SV_IntersectionAttributes )
{
  if (WaveActiveAllEqual(attr.id)) {
    AcceptHitAndEndSearch();
  }
  payload.color += float4(0.125, 0.25, 0.5, 1.0);
}
